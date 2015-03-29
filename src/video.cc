#include "p1stream_priv.h"

#include <string.h>
#include <node_buffer.h>

namespace p1stream {

// Video frame event. One of these is created per x264_encoder_{headers|encode}
// call. The struct is followed by an array of x264_nals_t, and then the
// sequential payloads.
struct video_frame_data {
    int64_t pts;
    int64_t dts;
    bool keyframe;

    int nals_len;
    x264_nal_t nals[0];
};

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

static Local<Value> video_events_transform(Isolate *isolate, event &ev, buffer_slicer &slicer);
static Local<Value> video_frame_to_js(Isolate *isolate, video_frame_data &frame, buffer_slicer &slicer);
static void encoder_log_callback(void *priv, int level, const char *format, va_list ap);


video_mixer_base::video_mixer_base() :
    buffer(this, video_events_transform, 1048576),  // 1 MiB event buffer
    running(), clock_ctx(), cl(), out_pic(), clq(), tex_mem(), out_mem(), yuv_kernel(), enc()
{
}

void video_mixer_base::init(const FunctionCallbackInfo<Value>& args)
{
    bool ok;
    cl_device_id device_id;
    cl_int cl_err;
    cl_program yuv_program;
    GLenum gl_err;
    int i_ret;
    isolate = args.GetIsolate();
    Handle<Value> val;

    if (args.Length() != 1 || !args[0]->IsObject()) {
        isolate->ThrowException(Exception::TypeError(
            String::NewFromUtf8(isolate, "Expected an object")));
        return;
    }
    auto params = args[0].As<Object>();

    val = params->Get(width_sym.Get(isolate));
    if (!val->IsUint32()) {
        isolate->ThrowException(Exception::TypeError(
            String::NewFromUtf8(isolate, "Invalid or missing width")));
        return;
    }
    out_dimensions.width = val->Uint32Value();

    val = params->Get(height_sym.Get(isolate));
    if (!val->IsUint32()) {
        isolate->ThrowException(Exception::TypeError(
            String::NewFromUtf8(isolate, "Invalid or missing height")));
        return;
    }
    out_dimensions.height = val->Uint32Value();

    val = params->Get(on_event_sym.Get(isolate));
    if (!val->IsFunction()) {
        isolate->ThrowException(Exception::TypeError(
            String::NewFromUtf8(isolate, "Expected an onEvent function")));
        return;
    }

    // Parameters checked, from here on we no longer throw exceptions.
    Wrap(args.This());
    Ref();
    args.GetReturnValue().Set(handle(isolate));

    buffer.set_callback(isolate->GetCurrentContext(), val.As<Function>());

    ok = platform_init(params);

    if (ok) {
        out_size = out_dimensions.width * out_dimensions.height * 1.5;
        yuv_work_size[0] = out_dimensions.width / 2;
        yuv_work_size[1] = out_dimensions.height / 2;

        size_t size;
        cl_err = clGetContextInfo(cl, CL_CONTEXT_DEVICES, sizeof(cl_device_id), &device_id, &size);
        if (!(ok = (cl_err == CL_SUCCESS)))
            buffer.emitf(EV_LOG_ERROR, "clGetContextInfo error 0x%x", cl_err);
        else if (!(ok = (size != 0)))
            buffer.emitf(EV_LOG_ERROR, "No suitable OpenCL devices");
    }

    if (ok) {
        clq = clCreateCommandQueue(cl, device_id, 0, &cl_err);
        if (!(ok = (cl_err == CL_SUCCESS)))
            buffer.emitf(EV_LOG_ERROR, "clCreateCommandQueue error 0x%x", cl_err);
    }

    if (ok) {
        i_ret = x264_picture_alloc(&out_pic, X264_CSP_I420, out_dimensions.width, out_dimensions.height);
        if (!(ok = (i_ret >= 0)))
            buffer.emitf(EV_LOG_ERROR, "x264_picture_alloc error");
    }

    if (ok) {
        glGenFramebuffers(1, &fbo);
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, texture_, 0);
        program = glCreateProgram();
        if (!(ok = ((gl_err = glGetError()) == GL_NO_ERROR)))
            buffer.emitf(EV_LOG_ERROR, "OpenGL error 0x%x", gl_err);
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
            buffer.emitf(EV_LOG_ERROR, "clCreateFromGLTexture error 0x%x", cl_err);
    }

    if (ok) {
        out_mem = clCreateBuffer(cl, CL_MEM_WRITE_ONLY, out_size, NULL, &cl_err);
        if (!(ok = (cl_err == CL_SUCCESS)))
            buffer.emitf(EV_LOG_ERROR, "clCreateBuffer error 0x%x", cl_err);
    }

    if (ok) {
        yuv_program = clCreateProgramWithSource(cl, 1, &yuv_kernel_source, NULL, &cl_err);
        if (!(ok = (cl_err == CL_SUCCESS)))
            buffer.emitf(EV_LOG_ERROR, "clCreateProgramWithSource error 0x%x", cl_err);
    }

    if (ok) {
        cl_err = clBuildProgram(yuv_program, 0, NULL, NULL, NULL, NULL);
        if (!(ok = (cl_err == CL_SUCCESS))) {
            buffer.emitf(EV_LOG_ERROR, "clBuildProgram error 0x%x", cl_err);
            clReleaseProgram(yuv_program);
        }
    }

    if (ok) {
        yuv_kernel = clCreateKernel(yuv_program, "yuv", &cl_err);
        clReleaseProgram(yuv_program);
        if (!(ok = (cl_err == CL_SUCCESS)))
            buffer.emitf(EV_LOG_ERROR, "clCreateKernel error 0x%x", cl_err);
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
            buffer.emitf(EV_LOG_ERROR, "OpenGL error 0x%x", gl_err);
    }

    if (ok) {
        cl_err = clSetKernelArg(yuv_kernel, 0, sizeof(cl_mem), &tex_mem);
        if (!(ok = (cl_err == CL_SUCCESS)))
            buffer.emitf(EV_LOG_ERROR, "clSetKernelArg error 0x%x", cl_err);
    }

    if (ok) {
        cl_err = clSetKernelArg(yuv_kernel, 1, sizeof(cl_mem), &out_mem);
        if (!(ok = (cl_err == CL_SUCCESS)))
            buffer.emitf(EV_LOG_ERROR, "clSetKernelArg error 0x%x", cl_err);
    }

    if (ok) {
        x264_param_default(&enc_params);

        enc_params.i_log_level = X264_LOG_INFO;
        enc_params.pf_log = encoder_log_callback;
        enc_params.p_log_private = this;

        val = params->Get(x264_preset_sym.Get(isolate));
        if (val->IsString()) {
            String::Utf8Value v(val);
            if (!(ok = (*v != NULL &&
                        x264_param_default_preset(&enc_params, *v, NULL) != 0)))
                buffer.emitf(EV_LOG_ERROR, "Invalid x264 preset");
        }
    }

    if (ok) {
        val = params->Get(x264_tuning_sym.Get(isolate));
        if (val->IsString()) {
            String::Utf8Value v(val);
            if (!(ok = (*v != NULL &&
                        x264_param_default_preset(&enc_params, NULL, *v) != 0)))
                buffer.emitf(EV_LOG_ERROR, "Invalid x264 tuning");
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
                    buffer.emitf(EV_LOG_ERROR, "Invalid x264 parameters");
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
                buffer.emitf(EV_LOG_ERROR, "Invalid x264 profile");
        }
    }

    if (ok) {
        val = params->Get(clock_sym.Get(isolate));
        if (!(ok = val->IsObject())) {
            buffer.emitf(EV_LOG_ERROR, "Invalid clock");
        }
        else {
            auto obj = val.As<Object>();
            auto clock = ObjectWrap::Unwrap<video_clock>(obj);
            lock_handle lock(*clock);

            clock_ctx = new video_clock_context_full(this, clock);
            clock->link_video_clock(*clock_ctx);
        }
    }

    if (ok) {
        enc_params.i_timebase_num = 1;
        enc_params.i_timebase_den = 1000000000;

        enc_params.b_aud = 1;
        enc_params.b_annexb = 0;

        enc_params.i_width = out_dimensions.width;
        enc_params.i_height = out_dimensions.height;

        running = true;
    }
    else {
        buffer.emit(EV_FAILURE);
    }
}

void video_mixer_base::destroy()
{
    lock_handle lock(*this);
    cl_int cl_err;

    running = false;

    if (clock_ctx != nullptr) {
        clock_ctx->clock()->unlink_video_clock(*clock_ctx);

        delete clock_ctx;
        clock_ctx = nullptr;
    }

    clear_sources();

    if (enc != NULL) {
        x264_encoder_close(enc);
        enc = NULL;
    }

    if (yuv_kernel != NULL) {
        cl_err = clReleaseKernel(yuv_kernel);
        if (cl_err != CL_SUCCESS)
            buffer.emitf(EV_LOG_ERROR, "clReleaseKernel error 0x%x\n", cl_err);
        yuv_kernel = NULL;
    }

    if (out_mem != NULL) {
        cl_err = clReleaseMemObject(out_mem);
        if (cl_err != CL_SUCCESS)
            buffer.emitf(EV_LOG_ERROR, "clReleaseMemObject error 0x%x\n", cl_err);
        out_mem = NULL;
    }

    if (tex_mem != NULL) {
        cl_err = clReleaseMemObject(tex_mem);
        if (cl_err != CL_SUCCESS)
            buffer.emitf(EV_LOG_ERROR, "clReleaseMemObject error 0x%x\n", cl_err);
        tex_mem = NULL;
    }

    x264_picture_clean(&out_pic);

    if (clq != NULL) {
        cl_err = clReleaseCommandQueue(clq);
        if (cl_err != CL_SUCCESS)
            buffer.emitf(EV_LOG_ERROR, "clReleaseCommandQueue error 0x%x\n", cl_err);
        clq = NULL;
    }

    if (cl != nullptr) {
        cl_int cl_err = clReleaseContext(cl);
        if (cl_err != CL_SUCCESS)
            buffer.emitf(EV_LOG_ERROR, "clReleaseContext error 0x%x\n", cl_err);
        cl = nullptr;
    }

    platform_destroy();

    buffer.flush();

    Unref();
}

lockable *video_mixer_base::lock()
{
    return clock_ctx ? clock_ctx->clock()->lock() : nullptr;
}

void video_mixer_base::clear_hooks()
{
    for (auto &ctx : hook_ctxes)
        ctx.hook()->unlink_video_hook(ctx);
    hook_ctxes.clear();
}

void video_mixer_base::set_hooks(const FunctionCallbackInfo<Value>& args)
{
    if (args.Length() != 1 || !args[0]->IsArray()) {
        isolate->ThrowException(Exception::TypeError(
            String::NewFromUtf8(isolate, "Expected an array")));
        return;
    }
    auto arr = args[0].As<Array>();
    uint32_t len = arr->Length();

    std::vector<video_hook_context_full> new_ctxes;
    for (uint32_t i = 0; i < len; i++) {
        auto val = arr->Get(i);
        if (!val->IsObject()) {
            isolate->ThrowException(Exception::TypeError(
                String::NewFromUtf8(isolate, "Expected only objects in the array")));
            return;
        }
        auto hook_obj = val.As<Object>();
        auto *hook = ObjectWrap::Unwrap<video_hook>(hook_obj);
        new_ctxes.emplace_back(this, hook);
    }

    // Parameters checked, from here on we no longer throw exceptions.
    lock_handle lock(*this);

    if (!running)
        return;

    clear_hooks();
    hook_ctxes = new_ctxes;

    for (uint32_t i = 0; i < len; i++) {
        auto &ctx = hook_ctxes[i];
        ctx.hook()->link_video_hook(ctx);
    }
}

void video_mixer_base::clear_sources()
{
    uint32_t len = source_ctxes.size();
    GLuint textures[len];
    for (uint32_t i = 0; i < len; i++) {
        auto &ctx = source_ctxes[i];
        ctx.source()->unlink_video_source(ctx);
        textures[i] = ctx.texture;
    }
    source_ctxes.clear();
    if (running)
        glDeleteTextures(len, textures);
}

void video_mixer_base::set_sources(const FunctionCallbackInfo<Value>& args)
{
    if (args.Length() != 1 || !args[0]->IsArray()) {
        isolate->ThrowException(Exception::TypeError(
            String::NewFromUtf8(isolate, "Expected an array")));
        return;
    }
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

    std::vector<video_source_context_full> new_ctxes;
    for (uint32_t i = 0; i < len; i++) {
        auto val = arr->Get(i);
        if (!val->IsObject()) {
            isolate->ThrowException(Exception::TypeError(
                String::NewFromUtf8(isolate, "Expected only objects in the array")));
            return;
        }
        auto obj = val.As<Object>();

        auto source_obj = obj->Get(l_source_sym);
        if (!source_obj->IsObject())
            continue;
        auto *source = ObjectWrap::Unwrap<video_source>(source_obj.As<Object>());

        new_ctxes.emplace_back(this, source);
        auto &ctx = new_ctxes.back();

        ctx.x1 = obj->Get(l_x1_sym)->NumberValue();
        ctx.y1 = obj->Get(l_y1_sym)->NumberValue();
        ctx.x2 = obj->Get(l_x2_sym)->NumberValue();
        ctx.y2 = obj->Get(l_y2_sym)->NumberValue();
        ctx.u1 = obj->Get(l_u1_sym)->NumberValue();
        ctx.v1 = obj->Get(l_v1_sym)->NumberValue();
        ctx.u2 = obj->Get(l_u2_sym)->NumberValue();
        ctx.v2 = obj->Get(l_v2_sym)->NumberValue();
    }

    len = new_ctxes.size();

    // Parameters checked, from here on we no longer throw exceptions.
    lock_handle lock(*this);

    if (!running || !activate_gl())
        return;

    GLenum gl_err;
    GLuint textures[len];
    glGenTextures(len, textures);
    if ((gl_err = glGetError()) != GL_NO_ERROR) {
        buffer.emitf(EV_LOG_ERROR, "OpenGL error 0x%x", gl_err);
        return;
    }

    clear_sources();
    source_ctxes = new_ctxes;

    for (uint32_t i = 0; i < len; i++) {
        auto &ctx = source_ctxes[i];
        ctx.texture = textures[i];
        ctx.source()->link_video_source(ctx);
    }
}

void video_mixer_base::tick(frame_time_t time)
{
    if (!running)
        return;

    bool ok = activate_gl();

    // Render.
    GLenum gl_err;

    if (ok) {
        glClear(GL_COLOR_BUFFER_BIT);
        for (auto &ctx : source_ctxes) {
            glBindTexture(GL_TEXTURE_RECTANGLE, ctx.texture);
            ctx.source()->produce_video_frame(ctx);
        }
        glFinish();
        if (!(ok = ((gl_err = glGetError()) == GL_NO_ERROR)))
            buffer.emitf(EV_LOG_ERROR, "OpenGL error 0x%x", gl_err);
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
            buffer.emitf(EV_LOG_ERROR, "clEnqueueAcquireGLObjects error 0x%x", cl_err);
    }

    if (ok) {
        cl_err = clEnqueueNDRangeKernel(clq, yuv_kernel, 2, NULL, yuv_work_size, NULL, 0, NULL, NULL);
        if (!(ok = (cl_err == CL_SUCCESS)))
            buffer.emitf(EV_LOG_ERROR, "clEnqueueNDRangeKernel error 0x%x", cl_err);
    }

    if (ok) {
        cl_err = clEnqueueReleaseGLObjects(clq, 1, &tex_mem, 0, NULL, NULL);
        if (!(ok = (cl_err == CL_SUCCESS)))
            buffer.emitf(EV_LOG_ERROR, "clEnqueueReleaseGLObjects error 0x%x", cl_err);
    }

    if (ok) {
        cl_err = clEnqueueReadBuffer(clq, out_mem, CL_FALSE, 0, out_size, out_pic.img.plane[0], 0, NULL, NULL);
        if (!(ok = (cl_err == CL_SUCCESS)))
            buffer.emitf(EV_LOG_ERROR, "clEnqueueReadBuffer error 0x%x", cl_err);
    }

    if (ok) {
        cl_err = clFinish(clq);
        if (!(ok = (cl_err == CL_SUCCESS)))
            buffer.emitf(EV_LOG_ERROR, "clFinish error 0x%x", cl_err);
    }

    // Encode.
    if (ok && enc == NULL) {
        fraction_t fps = clock_ctx->clock()->video_ticks_per_second(*clock_ctx);
        enc_params.i_fps_num = fps.num;
        enc_params.i_fps_den = fps.den;

        enc = x264_encoder_open(&enc_params);
        if (!(ok = (enc != NULL)))
            buffer.emitf(EV_LOG_ERROR, "x264_encoder_open error");

        if (ok) {
            i_ret = x264_encoder_headers(enc, &nals, &nals_len);
            if (!(ok = (i_ret >= 0)))
                buffer.emitf(EV_LOG_ERROR, "x264_encoder_headers error");
            else if (i_ret > 0)
                buffer_nals(EV_VIDEO_HEADERS, nals, nals_len, NULL);
        }
    }

    if (ok) {
        out_pic.i_dts = out_pic.i_pts = time;
        i_ret = x264_encoder_encode(enc, &nals, &nals_len, &out_pic, &enc_pic);
        if (!(ok = (i_ret >= 0)))
            buffer.emitf(EV_LOG_ERROR, "x264_encoder_encode error");
        else if (i_ret > 0)
            buffer_nals(EV_VIDEO_FRAME, nals, nals_len, &enc_pic);
    }
}

void video_mixer_base::buffer_nals(uint32_t id, x264_nal_t *nals, int nals_len, x264_picture_t *pic)
{
    x264_nal_t &last_nal = nals[nals_len - 1];
    uint8_t *start = nals[0].p_payload;
    uint8_t *end = last_nal.p_payload + last_nal.i_payload;
    size_t nals_size = nals_len * sizeof(x264_nal_t);
    size_t payload_size = end - start;
    size_t claim = sizeof(video_frame_data) + nals_size + payload_size;

    auto *ev = buffer.emit(id, claim);
    if (ev == NULL)
        return;

    auto &frame = *(video_frame_data *) ev->data;
    if (pic != NULL) {
        frame.pts = pic->i_pts;
        frame.dts = pic->i_dts;
        frame.keyframe = pic->b_keyframe ? true : false;
    }
    else {
        frame.pts = 0;
        frame.dts = 0;
        frame.keyframe = false;
    }

    frame.nals_len = nals_len;
    memcpy(frame.nals, nals, nals_size);

    uint8_t *p = (uint8_t *) (frame.nals + nals_len);
    memcpy(p, nals[0].p_payload, payload_size);
}

static Local<Value> video_events_transform(Isolate *isolate, event &ev, buffer_slicer &slicer)
{
    switch (ev.id) {
        case EV_VIDEO_HEADERS:
        case EV_VIDEO_FRAME:
            return video_frame_to_js(isolate, *(video_frame_data *) ev.data, slicer);
        default:
            return Undefined(isolate);
    }
}

static Local<Value> video_frame_to_js(Isolate *isolate, video_frame_data &frame, buffer_slicer &slicer)
{
    auto l_type_sym = type_sym.Get(isolate);
    auto l_priority_sym = priority_sym.Get(isolate);
    auto l_buf_sym = buf_sym.Get(isolate);

    auto *nals = frame.nals;
    auto nals_len = frame.nals_len;
    auto nalp = (char *) &nals[nals_len];
    int total_size = 0;

    auto nals_arr = Array::New(isolate, nals_len);
    for (int32_t i_nal = 0; i_nal < nals_len; i_nal++) {
        auto &nal = nals[i_nal];

        auto nal_obj = Object::New(isolate);
        nal_obj->Set(l_type_sym, Integer::New(isolate, nal.i_type));
        nal_obj->Set(l_priority_sym, Integer::New(isolate, nal.i_ref_idc));
        nal_obj->Set(l_buf_sym, slicer.slice(nalp, nal.i_payload));
        nals_arr->Set(i_nal, nal_obj);

        total_size += nal.i_payload;
        nalp += nal.i_payload;
    }

    auto obj = Object::New(isolate);
    obj->Set(pts_sym.Get(isolate), Number::New(isolate, frame.pts));
    obj->Set(dts_sym.Get(isolate), Number::New(isolate, frame.dts));
    obj->Set(keyframe_sym.Get(isolate), frame.keyframe ? True(isolate) : False(isolate));
    obj->Set(nals_sym.Get(isolate), nals_arr);
    obj->Set(l_buf_sym, slicer.slice((char *) &nals[nals_len], total_size));

    return obj;
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
        buffer.emitf(EV_LOG_WARN, "OpenGL shader compile log:\n%s", log);
    }

    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        buffer.emitf(EV_LOG_ERROR, "glCompileShader error 0x%x", err);
        return 0;
    }
    if (success != GL_TRUE) {
        buffer.emitf(EV_LOG_ERROR, "glCompileShader error");
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
        buffer.emitf(EV_LOG_WARN, "OpenGL program link log:\n%s", log);
    }

    GLint success = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        buffer.emitf(EV_LOG_ERROR, "glLinkProgram error 0x%x", err);
        return false;
    }
    if (success != GL_TRUE) {
        buffer.emitf(EV_LOG_ERROR, "glLinkProgram error");
        return false;
    }

    return true;
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

void video_source::link_video_source(video_source_context &ctx)
{
}

void video_source::unlink_video_source(video_source_context &ctx)
{
}

void video_hook::link_video_hook(video_hook_context &ctx)
{
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

static void encoder_log_callback(void *priv, int level, const char *format, va_list ap)
{
    auto &mixer = *(video_mixer_base *) priv;

    char full_format[1024];
    int size = snprintf(full_format, sizeof(full_format), "x264: %s", format);
    if (size <= 0)
        return;

    if (full_format[size - 1] == '\n')
        full_format[size - 1] = '\0';

    uint32_t ev_id;
    switch (level) {
        case X264_LOG_DEBUG:   ev_id = EV_LOG_DEBUG; break;
        case X264_LOG_INFO:    ev_id = EV_LOG_INFO;  break;
        case X264_LOG_WARNING: ev_id = EV_LOG_WARN;  break;
        default:               ev_id = EV_LOG_ERROR; break;
    }
    mixer.buffer.emitv(ev_id, full_format, ap);
}


}  // namespace p1stream
