#include "p1stream_priv.h"

#include <string.h>
#include <node_buffer.h>

namespace p1stream {

static const char *simple_vertex_shader =
    "#version 150\n"

    "uniform sampler2DRect u_Texture;\n"
    "in vec2 a_Position;\n"
    "in vec2 a_TexCoords;\n"
    "out vec2 v_TexCoords;\n"

    "void main(void) {\n"
        "gl_Position = vec4(a_Position.x, a_Position.y, 0.0, 1.0);\n"
        "v_TexCoords = a_TexCoords * textureSize(u_Texture);\n"
    "}\n";

static const char *simple_fragment_shader =
    "#version 150\n"

    "uniform sampler2DRect u_Texture;\n"
    "in vec2 v_TexCoords;\n"
    "out vec4 o_FragColor;\n"

    "void main(void) {\n"
        "o_FragColor = texture(u_Texture, v_TexCoords);\n"
    "}\n";

static const char *yuv_kernel_source =
    "const sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_LINEAR;\n"

    "kernel void yuv(read_only image2d_t input, global write_only uchar* output)\n"
    "{\n"
        "size_t wUV = get_global_size(0);\n"
        "size_t hUV = get_global_size(1);\n"
        "size_t xUV = get_global_id(0);\n"
        "size_t yUV = get_global_id(1);\n"

        "size_t wY = wUV * 2;\n"
        "size_t hY = hUV * 2;\n"
        "size_t xY = xUV * 2;\n"
        "size_t yY = yUV * 2;\n"

        "float2 xyImg = (float2)(xY, yY);\n"
        "size_t lenUV = wUV * hUV;\n"
        "size_t lenY = wY * hY;\n"

        "float4 s;\n"
        "size_t base;\n"
        "float value;\n"

        // Write 2x2 block of Y values.
        "base = yY * wY + xY;\n"
        "for (size_t dx = 0; dx < 2; dx++) {\n"
            "for (size_t dy = 0; dy < 2; dy++) {\n"
                "s = read_imagef(input, sampler, xyImg + (float2)(dx, dy) + 0.5f);\n"
                "value = 16 + 65.481f*s.r + 128.553f*s.g + 24.966f*s.b;\n"
                "output[base + dy * wY + dx] = value;\n"
            "}\n"
        "}\n"

        // Write UV values.
        "s = read_imagef(input, sampler, xyImg + 1.0f);\n"
        "base = yUV * wUV + xUV;\n"
        "value = 128 - 37.797f*s.r - 74.203f*s.g + 112.0f*s.b;\n"
        "output[lenY + base] = value;\n"
        "value = 128 + 112.0f*s.r - 93.786f*s.g - 18.214f*s.b;\n"
        "output[lenY + lenUV + base] = value;\n"
    "}\n";

static const GLsizei vbo_stride = 4 * sizeof(GLfloat);
static const GLsizei vbo_size = 4 * vbo_stride;
static const void *vbo_tex_coord_offset = (void *)(2 * sizeof(GLfloat));


void video_mixer_base::init(const FunctionCallbackInfo<Value>& args)
{
    bool ok;
    cl_device_id device_id;
    cl_int cl_err;
    GLenum gl_err;
    int i_ret;

    Handle<Object> params;
    Handle<Value> val;
    cl_program yuv_program;

    Wrap(args.This());
    args.GetReturnValue().Set(handle());
    isolate = args.GetIsolate();
    context.Reset(isolate, isolate->GetCurrentContext());

    callback.init(std::bind(&video_mixer_base::emit_last, this));

    if (!(ok = (args.Length() == 1)))
        strcpy(last_error, "Expected one argument");
    else if (!(ok = args[0]->IsObject()))
        strcpy(last_error, "Expected an object");
    else
        params = args[0].As<Object>();

    if (ok) {
        val = params->Get(buffer_size_sym.Get(isolate));
        if (!(ok = val->IsUint32()))
            strcpy(last_error, "Invalid or missing buffer size");
    }

    if (ok) {
        buffer_size = val->Uint32Value();
        if (!(ok = (buffer_size > 0)))
            strcpy(last_error, "Invalid or missing buffer size");
    }

    if (ok) {
        buffer = new uint8_t[buffer_size];
        buffer_pos = buffer;

        val = params->Get(width_sym.Get(isolate));
        if (!(ok = val->IsUint32()))
            strcpy(last_error, "Invalid or missing width");
    }

    if (ok) {
        out_dimensions.width = val->Uint32Value();

        val = params->Get(height_sym.Get(isolate));
        if (!(ok = val->IsUint32()))
            strcpy(last_error, "Invalid or missing height");
    }

    if (ok) {
        out_dimensions.height = val->Uint32Value();

        val = params->Get(on_data_sym.Get(isolate));
        if (!(ok = val->IsFunction()))
            strcpy(last_error, "Invalid or missing onData handler");
    }

    if (ok) {
        on_data.Reset(isolate, val.As<Function>());

        val = params->Get(on_error_sym.Get(isolate));
        if (!(ok = val->IsFunction()))
            strcpy(last_error, "Invalid or missing onError handler");
    }

    if (ok) {
        on_error.Reset(isolate, val.As<Function>());

        ok = platform_init(params);
    }

    if (ok) {
        out_size = out_dimensions.width * out_dimensions.height * 1.5;
        yuv_work_size[0] = out_dimensions.width / 2;
        yuv_work_size[1] = out_dimensions.height / 2;

        size_t size;
        cl_err = clGetContextInfo(cl, CL_CONTEXT_DEVICES, sizeof(cl_device_id), &device_id, &size);
        if (!(ok = (cl_err == CL_SUCCESS)))
            sprintf(last_error, "clGetContextInfo error %d", cl_err);
        else if (!(ok = (size != 0)))
            strcpy(last_error, "No suitable OpenCL devices");
    }

    if (ok) {
        clq = clCreateCommandQueue(cl, device_id, 0, &cl_err);
        if (!(ok = (cl_err == CL_SUCCESS)))
            sprintf(last_error, "clCreateCommandQueue error %d", cl_err);
    }

    if (ok) {
        i_ret = x264_picture_alloc(&out_pic, X264_CSP_I420, out_dimensions.width, out_dimensions.height);
        if (!(ok = (i_ret >= 0)))
            strcpy(last_error, "x264_picture_alloc error");
    }

    if (ok) {
        glGenFramebuffers(1, &fbo);
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, texture_, 0);
        program = glCreateProgram();
        if (!(ok = ((gl_err = glGetError()) == GL_NO_ERROR)))
            sprintf(last_error, "OpenGL error %d", gl_err);
    }

    if (ok) {
        glBindAttribLocation(program, 0, "a_Position");
        glBindAttribLocation(program, 1, "a_TexCoords");
        glBindFragDataLocation(program, 0, "o_FragColor");
        ok = build_program();
    }

    if (ok) {
        tex_u = glGetUniformLocation(program, "u_Texture");

        tex_mem = clCreateFromGLTexture(cl, CL_MEM_READ_ONLY, GL_TEXTURE_RECTANGLE, 0, texture_, &cl_err);
        if (!(ok = (cl_err == CL_SUCCESS)))
            sprintf(last_error, "clCreateFromGLTexture error %d", cl_err);
    }

    if (ok) {
        out_mem = clCreateBuffer(cl, CL_MEM_WRITE_ONLY, out_size, NULL, &cl_err);
        if (!(ok = (cl_err == CL_SUCCESS)))
            sprintf(last_error, "clCreateBuffer error %d", cl_err);
    }

    if (ok) {
        yuv_program = clCreateProgramWithSource(cl, 1, &yuv_kernel_source, NULL, &cl_err);
        if (!(ok = (cl_err == CL_SUCCESS)))
            sprintf(last_error, "clCreateProgramWithSource error %d", cl_err);
    }

    if (ok) {
        cl_err = clBuildProgram(yuv_program, 0, NULL, NULL, NULL, NULL);
        if (!(ok = (cl_err == CL_SUCCESS))) {
            sprintf(last_error, "clBuildProgram error %d", cl_err);
            clReleaseProgram(yuv_program);
        }
    }

    if (ok) {
        yuv_kernel = clCreateKernel(yuv_program, "yuv", &cl_err);
        clReleaseProgram(yuv_program);
        if (!(ok = (cl_err == CL_SUCCESS)))
            sprintf(last_error, "clCreateKernel error %d", cl_err);
    }

    if (ok) {
        // GL state init. Most of this is up here because we can.
        glViewport(0, 0, out_dimensions.width, out_dimensions.height);
        glClearColor(0, 0, 0, 1);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glUseProgram(program);
        glUniform1i(tex_u, 0);
        glBindVertexArray(vao);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, vbo_stride, 0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, vbo_stride, vbo_tex_coord_offset);
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        if (!(ok = ((gl_err = glGetError()) == GL_NO_ERROR)))
            sprintf(last_error, "OpenGL error %d", gl_err);
    }

    if (ok) {
        cl_err = clSetKernelArg(yuv_kernel, 0, sizeof(cl_mem), &tex_mem);
        if (!(ok = (cl_err == CL_SUCCESS)))
            sprintf(last_error, "clSetKernelArg error %d", cl_err);
    }

    if (ok) {
        cl_err = clSetKernelArg(yuv_kernel, 1, sizeof(cl_mem), &out_mem);
        if (!(ok = (cl_err == CL_SUCCESS)))
            sprintf(last_error, "clSetKernelArg error %d", cl_err);
    }

    if (ok) {
        x264_param_default(&enc_params);

        val = params->Get(x264_preset_sym.Get(isolate));
        if (val->IsString()) {
            String::Utf8Value v(val);
            if (!(ok = (*v != NULL &&
                        x264_param_default_preset(&enc_params, *v, NULL) != 0)))
                strcpy(last_error, "Invalid x264 preset");
        }
    }

    if (ok) {
        val = params->Get(x264_tuning_sym.Get(isolate));
        if (val->IsString()) {
            String::Utf8Value v(val);
            if (!(ok = (*v != NULL &&
                        x264_param_default_preset(&enc_params, NULL, *v) != 0)))
                strcpy(last_error, "Invalid x264 tuning");
        }
    }

    if (ok) {
        val = params->Get(x264_params_sym.Get(isolate));
        if (val->IsObject()) {
            auto obj = val.As<Object>();
            auto arr = obj->GetPropertyNames();
            uint32_t len = arr->Length();
            for (uint32_t i = 0; i < len; i++) {
                auto key = arr->Get(i);
                String::Utf8Value k(key);
                String::Utf8Value v(obj->Get(key));
                if (!(ok = (*k != NULL && *v != NULL &&
                            x264_param_parse(&enc_params, *k, *v) != 0))) {
                    strcpy(last_error, "Invalid x264 parameters");
                    break;
                }
            }
        }
    }

    if (ok) {
        x264_param_apply_fastfirstpass(&enc_params);

        // Apply profile.
        val = params->Get(x264_profile_sym.Get(isolate));
        if (val->IsString()) {
            String::Utf8Value v(val);
            if (!(ok = (*v != NULL &&
                        x264_param_apply_profile(&enc_params, *v) != 0)))
                strcpy(last_error, "Invalid x264 profile");
        }
    }

    if (ok) {
        val = params->Get(clock_sym.Get(isolate));
        if (!val->IsObject())
            strcpy(last_error, "Invalid clock");

        auto obj = val.As<Object>();
        auto clock = ObjectWrap::Unwrap<video_clock>(obj);
        lock_handle lock(*clock);

        clock_ctx = new video_clock_context_full(this, clock);
        ok = clock->link_video_clock(*clock_ctx);
        if (!ok) {
            delete clock_ctx;
            clock_ctx = nullptr;
        }
    }

    if (ok) {
        enc_params.i_log_level = X264_LOG_INFO;

        enc_params.i_timebase_num = 1;
        enc_params.i_timebase_den = 1000000000;

        enc_params.b_aud = 1;
        enc_params.b_annexb = 0;

        enc_params.i_width = out_dimensions.width;
        enc_params.i_height = out_dimensions.height;

        Ref();
    }
    else {
        destroy(false);
        isolate->ThrowException(pop_last_error());
    }
}

void video_mixer_base::destroy(bool unref)
{
    lock_handle lock(*this);
    cl_int cl_err;

    if (clock_ctx != nullptr) {
        clock_ctx->clock()->unlink_video_clock(*clock_ctx);

        delete clock_ctx;
        clock_ctx = nullptr;
    }

    clear_sources();

    callback.destroy();

    if (enc != NULL) {
        x264_encoder_close(enc);
        enc = NULL;
    }

    if (yuv_kernel != NULL) {
        cl_err = clReleaseKernel(yuv_kernel);
        if (cl_err != CL_SUCCESS)
            fprintf(stderr, "clReleaseKernel error %d\n", cl_err);
        yuv_kernel = NULL;
    }

    if (out_mem != NULL) {
        cl_err = clReleaseMemObject(out_mem);
        if (cl_err != CL_SUCCESS)
            fprintf(stderr, "clReleaseMemObject error %d\n", cl_err);
        out_mem = NULL;
    }

    if (tex_mem != NULL) {
        cl_err = clReleaseMemObject(tex_mem);
        if (cl_err != CL_SUCCESS)
            fprintf(stderr, "clReleaseMemObject error %d\n", cl_err);
        tex_mem = NULL;
    }

    x264_picture_clean(&out_pic);

    if (clq != NULL) {
        cl_err = clReleaseCommandQueue(clq);
        if (cl_err != CL_SUCCESS)
            fprintf(stderr, "clReleaseCommandQueue error %d\n", cl_err);
        clq = NULL;
    }

    if (cl != nullptr) {
        cl_int cl_err = clReleaseContext(cl);
        if (cl_err != CL_SUCCESS)
            fprintf(stderr, "clReleaseContext error %d", cl_err);
        cl = nullptr;
    }

    platform_destroy();

    if (buffer != nullptr) {
        delete[] buffer;
        buffer = nullptr;
    }

    on_data.Reset();
    on_error.Reset();
    context.Reset();

    if (unref)
        Unref();
}

lockable *video_mixer_base::lock()
{
    return clock_ctx ? clock_ctx->clock()->lock() : nullptr;
}

Handle<Value> video_mixer_base::pop_last_error()
{
    Handle<Value> ret;
    if (last_error[0] != '\0') {
        ret = Exception::Error(String::NewFromUtf8(isolate, last_error));
        last_error[0] = '\0';
    }
    return ret;
}

void video_mixer_base::clear_hooks()
{
    for (auto &ctx : hook_ctxes)
        ctx.hook()->unlink_video_hook(ctx);
    hook_ctxes.clear();
}

void video_mixer_base::set_hooks(const FunctionCallbackInfo<Value>& args)
{
    if (args.Length() != 1)
        return throw_type_error("Expected one argument");

    if (!args[0]->IsArray())
        return throw_type_error("Expected an array");

    lock_handle lock(*this);

    clear_hooks();

    bool ok = true;
    auto arr = args[0].As<Array>();
    uint32_t len = arr->Length();

    for (uint32_t i = 0; i < len; i++) {
        auto val = arr->Get(i);
        if (!(ok = val->IsObject())) {
            throw_type_error("Expected only objects in the array");
            break;
        }

        auto hook_obj = val.As<Object>();
        auto *hook = ObjectWrap::Unwrap<video_hook>(hook_obj);

        hook_ctxes.emplace_back(this, hook);
        auto &ctx = hook_ctxes.back();

        if (!(ok = ctx.hook()->link_video_hook(ctx))) {
            hook_ctxes.pop_back();
            break;
        }
    }

    if (!ok)
        clear_hooks();
}

void video_mixer_base::clear_sources()
{
    for (auto &ctx : source_ctxes) {
        ctx.source()->unlink_video_source(ctx);
        glDeleteTextures(1, &ctx.texture);
    }
    source_ctxes.clear();
}

void video_mixer_base::set_sources(const FunctionCallbackInfo<Value>& args)
{
    if (args.Length() != 1)
        return throw_type_error("Expected one argument");

    if (!args[0]->IsArray())
        return throw_type_error("Expected an array");

    lock_handle lock(*this);

    clear_sources();

    bool ok = activate_gl();

    if (ok) {
        auto arr = args[0].As<Array>();
        uint32_t len = arr->Length();

        auto l_source_sym = source_sym.Get(isolate);
        auto l_x1_sym = x1_sym.Get(isolate);
        auto l_y1_sym = y1_sym.Get(isolate);
        auto l_x2_sym = x2_sym.Get(isolate);
        auto l_y2_sym = y2_sym.Get(isolate);
        auto l_u1_sym = u1_sym.Get(isolate);
        auto l_v1_sym = v1_sym.Get(isolate);
        auto l_u2_sym = u2_sym.Get(isolate);
        auto l_v2_sym = v2_sym.Get(isolate);

        for (uint32_t i = 0; i < len; i++) {
            auto val = arr->Get(i);
            if (!(ok = val->IsObject())) {
                throw_type_error("Expected only objects in the array");
                break;
            }

            auto obj = val.As<Object>();

            val = obj->Get(l_source_sym);
            if (!(ok = val->IsObject())) {
                throw_type_error("Invalid or missing source");
                break;
            }

            auto source_obj = val.As<Object>();
            auto *source = ObjectWrap::Unwrap<video_source>(source_obj);

            source_ctxes.emplace_back(this, source);
            auto &ctx = source_ctxes.back();

            ctx.x1 = obj->Get(l_x1_sym)->NumberValue();
            ctx.y1 = obj->Get(l_y1_sym)->NumberValue();
            ctx.x2 = obj->Get(l_x2_sym)->NumberValue();
            ctx.y2 = obj->Get(l_y2_sym)->NumberValue();
            ctx.u1 = obj->Get(l_u1_sym)->NumberValue();
            ctx.v1 = obj->Get(l_v1_sym)->NumberValue();
            ctx.u2 = obj->Get(l_u2_sym)->NumberValue();
            ctx.v2 = obj->Get(l_v2_sym)->NumberValue();

            GLenum gl_err;
            glGenTextures(1, &ctx.texture);
            if (!(ok = ((gl_err = glGetError()) == GL_NO_ERROR))) {
                sprintf(last_error, "OpenGL error %d", gl_err);
                isolate->ThrowException(pop_last_error());
                source_ctxes.pop_back();
                break;
            }

            if (!(ok = ctx.source()->link_video_source(ctx))) {
                glDeleteTextures(1, &ctx.texture);
                source_ctxes.pop_back();
                break;
            }
        }
    }

    if (!ok)
        clear_sources();
}

void video_mixer_base::tick(frame_time_t time)
{
    bool ok = (last_error[0] == '\0');

    // Render.
    GLenum gl_err;

    if (ok) {
        ok = activate_gl();
    }

    if (ok) {
        glClear(GL_COLOR_BUFFER_BIT);
        for (auto &ctx : source_ctxes) {
            glBindTexture(GL_TEXTURE_RECTANGLE, ctx.texture);
            ctx.source()->produce_video_frame(ctx);
        }
        glFinish();
        if (!(ok = ((gl_err = glGetError()) == GL_NO_ERROR)))
            sprintf(last_error, "OpenGL error %d", gl_err);
    }

    if (ok) {
        for (auto &ctx : hook_ctxes)
            ctx.hook()->video_post_render(ctx);
    }

    // Convert colorspace.
    cl_int cl_err;
    int i_ret;
    x264_nal_t *nals;
    int nals_len;
    x264_picture_t enc_pic;

    if (ok) {
        cl_err = clEnqueueAcquireGLObjects(clq, 1, &tex_mem, 0, NULL, NULL);
        if (!(ok = (cl_err == CL_SUCCESS)))
            sprintf(last_error, "clEnqueueAcquireGLObjects error %d", cl_err);
    }

    if (ok) {
        cl_err = clEnqueueNDRangeKernel(clq, yuv_kernel, 2, NULL, yuv_work_size, NULL, 0, NULL, NULL);
        if (!(ok = (cl_err == CL_SUCCESS)))
            sprintf(last_error, "clEnqueueNDRangeKernel error %d", cl_err);
    }

    if (ok) {
        cl_err = clEnqueueReleaseGLObjects(clq, 1, &tex_mem, 0, NULL, NULL);
        if (!(ok = (cl_err == CL_SUCCESS)))
            sprintf(last_error, "clEnqueueReleaseGLObjects error %d", cl_err);
    }

    if (ok) {
        cl_err = clEnqueueReadBuffer(clq, out_mem, CL_FALSE, 0, out_size, out_pic.img.plane[0], 0, NULL, NULL);
        if (!(ok = (cl_err == CL_SUCCESS)))
            sprintf(last_error, "clEnqueueReadBuffer error %d", cl_err);
    }

    if (ok) {
        cl_err = clFinish(clq);
        if (!(ok = (cl_err == CL_SUCCESS)))
            sprintf(last_error, "clFinish error %d", cl_err);
    }

    // Encode.
    if (ok && enc == NULL) {
        fraction_t fps = clock_ctx->clock()->video_ticks_per_second(*clock_ctx);
        enc_params.i_fps_num = fps.num;
        enc_params.i_fps_den = fps.den;

        enc = x264_encoder_open(&enc_params);
        if (!(ok = (enc != NULL)))
            strcpy(last_error, "x264_encoder_open error");

        if (ok) {
            i_ret = x264_encoder_headers(enc, &nals, &nals_len);
            if (!(ok = (i_ret >= 0)))
                strcpy(last_error, "x264_encoder_headers error");
            else if (i_ret > 0)
                ok = buffer_nals(nals, nals_len, NULL);
        }
    }

    if (ok) {
        out_pic.i_dts = out_pic.i_pts = time;
        i_ret = x264_encoder_encode(enc, &nals, &nals_len, &out_pic, &enc_pic);
        if (!(ok = (i_ret >= 0)))
            strcpy(last_error, "x264_encoder_encode error");
        else if (i_ret > 0)
            ok = buffer_nals(nals, nals_len, &enc_pic);
    }

    // Signal main thread.
    if (!ok || buffer_pos != buffer)
        callback.send();
}

bool video_mixer_base::buffer_nals(x264_nal_t *nals, int nals_len, x264_picture_t *pic)
{
    x264_nal_t &last_nal = nals[nals_len - 1];
    uint8_t *start = nals[0].p_payload;
    uint8_t *end = last_nal.p_payload + last_nal.i_payload;
    size_t nals_size = nals_len * sizeof(x264_nal_t);
    size_t payload_size = end - start;
    size_t claim = sizeof(video_mixer_frame) + nals_size + payload_size;
    size_t available = buffer + buffer_size - buffer_pos;

    if (claim > available) {
        // FIXME: Improve logging
        fprintf(stderr, "video mixer overflow, dropping video frames\n");
        return false;
    }

    auto *frame = (video_mixer_frame *) buffer_pos;
    buffer_pos += claim;

    if (pic != NULL) {
        frame->pts = pic->i_pts;
        frame->dts = pic->i_dts;
        frame->keyframe = pic->b_keyframe ? true : false;
    }
    else {
        frame->pts = 0;
        frame->dts = 0;
        frame->keyframe = false;
    }

    frame->nals_len = nals_len;
    memcpy(frame->nals, nals, nals_size);

    uint8_t *p = (uint8_t *) (frame->nals + nals_len);
    memcpy(p, nals[0].p_payload, payload_size);

    return true;
}

void video_mixer_base::emit_last()
{
    HandleScope handle_scope(isolate);
    Context::Scope context_scope(Local<Context>::New(isolate, context));

    Handle<Value> err;
    uint8_t *copy = NULL;
    size_t size = 0;

    // With lock, extract a copy of buffer (or error).
    {
        lock_handle lock(*this);
        err = pop_last_error();
        if (err.IsEmpty()) {
            size = buffer_pos - buffer;
            if (size != 0) {
                copy = new uint8_t[size];
                memcpy(copy, buffer, size);
                buffer_pos = buffer;
            }
        }
    }

    if (!err.IsEmpty()) {
        auto l_on_error = Local<Function>::New(isolate, on_error);
        MakeCallback(isolate, handle(isolate), l_on_error, 1, &err);
    }
    if (size == 0)
        return;

    // Create meta structure from buffer.
    auto obj = Object::New(isolate);

    auto buf = Buffer::New(isolate, (char *) copy, size, free_callback, NULL);
    obj->Set(buf_sym.Get(isolate), buf);

    auto frames_arr = Array::New(isolate);
    obj->Set(frames_sym.Get(isolate), frames_arr);

    auto l_pts_sym = pts_sym.Get(isolate);
    auto l_dts_sym = dts_sym.Get(isolate);
    auto l_keyframe_sym = keyframe_sym.Get(isolate);
    auto l_nals_sym = nals_sym.Get(isolate);
    auto l_type_sym = type_sym.Get(isolate);
    auto l_priority_sym = priority_sym.Get(isolate);
    auto l_start_sym = start_sym.Get(isolate);
    auto l_end_sym = end_sym.Get(isolate);

    auto *p = copy;
    auto *end = p + size;
    uint32_t i_frame = 0;
    while (p != end) {
        auto *frame = (video_mixer_frame *) p;
        auto frame_obj = Object::New(isolate);
        frames_arr->Set(i_frame++, frame_obj);

        auto *nals = frame->nals;
        auto nals_len = frame->nals_len;
        auto nals_arr = Array::New(isolate, nals_len);
        frame_obj->Set(l_pts_sym, Number::New(isolate, frame->pts));
        frame_obj->Set(l_dts_sym, Number::New(isolate, frame->dts));
        frame_obj->Set(l_keyframe_sym, frame->keyframe ?
                                       True(isolate) : False(isolate));
        frame_obj->Set(l_nals_sym, nals_arr);

        p = ((uint8_t *) nals) + nals_len * sizeof(x264_nal_t);
        frame_obj->Set(l_start_sym, Uint32::New(isolate, p - copy));

        for (int32_t i_nal = 0; i_nal < nals_len; i_nal++) {
            auto &nal = nals[i_nal];
            auto nal_obj = Object::New(isolate);
            nals_arr->Set(i_nal, nal_obj);

            nal_obj->Set(l_type_sym, Integer::New(isolate, nal.i_type));
            nal_obj->Set(l_priority_sym, Integer::New(isolate, nal.i_ref_idc));

            nal_obj->Set(l_start_sym, Uint32::New(isolate, p - copy));
            p += nal.i_payload;
            nal_obj->Set(l_end_sym, Uint32::New(isolate, p - copy));
        }

        frame_obj->Set(l_end_sym, Uint32::New(isolate, p - copy));
    }

    auto l_on_data = Local<Function>::New(isolate, on_data);
    Handle<Value> arg = obj;
    MakeCallback(isolate, handle(isolate), l_on_data, 1, &arg);
}

GLuint video_mixer_base::build_shader(GLuint type, const char *source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint log_size = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_size);
    if (log_size) {
        GLchar log[log_size];
        glGetShaderInfoLog(shader, log_size, NULL, log);
        fprintf(stderr, "%s", log);
    }

    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        sprintf(last_error, "glCompileShader error %d", err);
        return 0;
    }
    if (success != GL_TRUE) {
        strcpy(last_error, "glCompileShader error");
        return 0;
    }

    return shader;
}

bool video_mixer_base::build_program()
{
    GLuint vertex_shader = build_shader(GL_VERTEX_SHADER, simple_vertex_shader);
    if (vertex_shader == 0)
        return false;

    GLuint fragment_shader = build_shader(GL_FRAGMENT_SHADER, simple_fragment_shader);
    if (fragment_shader == 0)
        return false;

    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    glDetachShader(program, vertex_shader);
    glDetachShader(program, fragment_shader);

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    GLint log_size = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_size);
    if (log_size) {
        GLchar log[log_size];
        glGetProgramInfoLog(program, log_size, NULL, log);
        fprintf(stderr, "%s", log);
    }

    GLint success = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        sprintf(last_error, "glLinkProgram error %d", err);
        return false;
    }
    if (success != GL_TRUE) {
        strcpy(last_error, "glLinkProgram error");
        return false;
    }

    return true;
}

void video_mixer_base::free_callback(char *data, void *hint)
{
    auto *p = (uint8_t *) data;
    delete[] p;
}

void video_mixer_base::init_prototype(Handle<FunctionTemplate> func)
{
    NODE_SET_PROTOTYPE_METHOD(func, "destroy", [](const FunctionCallbackInfo<Value>& args) {
        auto mixer = ObjectWrap::Unwrap<video_mixer_base>(args.This());
        mixer->destroy();
    });
    NODE_SET_PROTOTYPE_METHOD(func, "setSources", [](const FunctionCallbackInfo<Value>& args) {
        auto mixer = ObjectWrap::Unwrap<video_mixer_base>(args.This());
        mixer->set_sources(args);
    });
    NODE_SET_PROTOTYPE_METHOD(func, "setHooks", [](const FunctionCallbackInfo<Value>& args) {
        auto mixer = ObjectWrap::Unwrap<video_mixer_base>(args.This());
        mixer->set_hooks(args);
    });
}


void video_clock_context::tick(frame_time_t time)
{
    ((video_mixer_base *) mixer_)->tick(time);
}

bool video_source::link_video_source(video_source_context &ctx)
{
    return true;
}

void video_source::unlink_video_source(video_source_context &ctx)
{
}

bool video_hook::link_video_hook(video_hook_context &ctx)
{
    return true;
}

void video_hook::unlink_video_hook(video_hook_context &ctx)
{
}

void video_source_context::render_texture()
{
    auto &f = *((video_source_context_full *) this);
    GLfloat data[] = {
        f.x1, f.y1, f.u1, f.v1,
        f.x1, f.y2, f.u1, f.v2,
        f.x2, f.y1, f.u2, f.v1,
        f.x2, f.y2, f.u2, f.v2
    };
    glBufferData(GL_ARRAY_BUFFER, vbo_size, data, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void video_source_context::render_buffer(dimensions_t dimensions, void *data)
{
    glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA8, dimensions.width, dimensions.height, 0,
                 GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, data);
    render_texture();
}


}  // namespace p1stream
