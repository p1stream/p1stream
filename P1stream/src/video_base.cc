#include "video_base.h"

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

void video_mixer_callback::async_cb(uv_async_t *handle, int status)
{
    if (status) return;
    auto *callback = (video_mixer_callback *) handle;
    callback->fn();
}


Handle<Value> video_mixer_base::init(const Arguments &args)
{
    bool ok;
    Handle<Value> ret;

    Handle<Value> val;
    cl_device_id device_id;
    cl_int cl_err;
    GLenum gl_err;
    int i_ret;
    size_t size;

    Handle<Object> params;
    x264_param_t enc_params;
    cl_program yuv_program;

    Wrap(args.This());

    source_sym = NODE_PSYMBOL("source");
    x1_sym = NODE_PSYMBOL("x1");
    y1_sym = NODE_PSYMBOL("y1");
    x2_sym = NODE_PSYMBOL("x2");
    y2_sym = NODE_PSYMBOL("y2");
    u1_sym = NODE_PSYMBOL("u1");
    v1_sym = NODE_PSYMBOL("v1");
    u2_sym = NODE_PSYMBOL("u2");
    v2_sym = NODE_PSYMBOL("v2");
    data_sym = NODE_PSYMBOL("data");
    type_sym = NODE_PSYMBOL("type");
    offset_sym = NODE_PSYMBOL("offset");
    size_sym = NODE_PSYMBOL("size");

    if (!(ok = (args.Length() == 1)))
        ret = ThrowException(Exception::TypeError(
            String::New("Expected one argument")));

    if (ok) {
        if (!(ok = args[0]->IsObject()))
            ret = ThrowException(Exception::TypeError(
                String::New("Expected an object")));
    }

    if (ok) {
        params = Local<Object>::Cast(args[0]);

        val = params->Get(String::NewSymbol("width"));
        if (!(ok = val->IsUint32()))
            ret = ThrowException(Exception::TypeError(
                String::New("Invalid or missing width")));
    }

    if (ok) {
        out_dimensions.width = val->Uint32Value();

        val = params->Get(String::NewSymbol("height"));
        if (!(ok = val->IsUint32()))
            ret = ThrowException(Exception::TypeError(
                String::New("Invalid or missing height")));
    }

    if (ok) {
        out_dimensions.height = val->Uint32Value();

        val = params->Get(String::NewSymbol("onFrame"));
        if (!(ok = val->IsFunction()))
            ret = ThrowException(Exception::TypeError(
                String::New("Invalid or missing onFrame handler")));
    }

    if (ok) {
        on_frame = Persistent<Function>::New(Handle<Function>::Cast(val));

        val = params->Get(String::NewSymbol("onError"));
        if (!(ok = val->IsFunction()))
            ret = ThrowException(Exception::TypeError(
                String::New("Invalid or missing onError handler")));
    }

    if (ok) {
        on_error = Persistent<Function>::New(Handle<Function>::Cast(val));

        ret = platform_init(params);
        ok = ret.IsEmpty();
    }

    if (ok) {
        out_size = out_dimensions.width * out_dimensions.height * 1.5;
        yuv_work_size[0] = out_dimensions.width / 2;
        yuv_work_size[1] = out_dimensions.height / 2;

        cl_err = clGetContextInfo(cl, CL_CONTEXT_DEVICES, sizeof(cl_device_id), &device_id, &size);
        if (!(ok = (cl_err == CL_SUCCESS)))
            sprintf(last_error, "clGetContextInfo error %d", cl_err);
    }

    if (ok) {
        if (!(ok = (size != 0)))
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
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
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

        tex_mem = clCreateFromGLTexture(cl, CL_MEM_READ_ONLY, GL_TEXTURE_RECTANGLE, 0, tex, &cl_err);
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
        glActiveTexture(GL_TEXTURE0);
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
        callback.fn = std::bind(&video_mixer_base::emit_last, this);
        uv_err_code err = callback.init();
        if (!(ok = (err == UV_OK)))
            ret = UVException(err, "uv_async_init");
    }

    if (ok) {
        x264_param_default(&enc_params);

        val = params->Get(String::NewSymbol("x264Preset"));
        if (val->IsString()) {
            String::AsciiValue v(val);
            if (!(ok = (*v != NULL &&
                        x264_param_default_preset(&enc_params, *v, NULL) != 0)))
                strcpy(last_error, "Invalid x264 preset");
        }
    }

    if (ok) {
        val = params->Get(String::NewSymbol("x264Tuning"));
        if (val->IsString()) {
            String::AsciiValue v(val);
            if (!(ok = (*v != NULL &&
                        x264_param_default_preset(&enc_params, NULL, *v) != 0)))
                strcpy(last_error, "Invalid x264 tuning");
        }
    }

    if (ok) {
        val = params->Get(String::NewSymbol("x264Params"));
        if (val->IsObject()) {
            auto obj = Handle<Object>::Cast(val);
            auto arr = obj->GetPropertyNames();
            uint32_t len = arr->Length();
            for (uint32_t i = 0; i < len; i++) {
                auto key = arr->Get(i);
                String::AsciiValue k(key);
                String::AsciiValue v(obj->Get(key));
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
        val = params->Get(String::NewSymbol("x264Profile"));
        if (val->IsString()) {
            String::AsciiValue v(val);
            if (!(ok = (*v != NULL &&
                        x264_param_apply_profile(&enc_params, *v) != 0)))
                strcpy(last_error, "Invalid x264 profile");
        }
    }

    if (ok) {
        enc_params.i_log_level = X264_LOG_DEBUG;
        enc_params.i_width = out_dimensions.width;
        enc_params.i_height = out_dimensions.height;
        video_enc = x264_encoder_open(&enc_params);
        if (!(ok = (video_enc != NULL)))
            strcpy(last_error, "x264_encoder_open error");
    }

    if (ok) {
        i_ret = x264_encoder_headers(video_enc, &nals, &nals_len);
        if (!(ok = (i_ret >= 0)))
            strcpy(last_error, "x264_encoder_headers error");
    }

    if (ok) {
        handle_->Set(String::NewSymbol("headers"), nals_to_js());

        return handle_;
    }
    else {
        destroy();

        if (ret.IsEmpty())
            ret = pop_last_error();
        return ThrowException(ret);
    }
}

void video_mixer_base::destroy()
{
    lock_handle lock(clock);
    cl_int cl_err;

    clear_clock();
    clear_sources();

    callback.close();

    if (video_enc != NULL) {
        x264_encoder_close(video_enc);
        video_enc = NULL;
    }

    if (yuv_kernel != NULL) {
        cl_err = clReleaseKernel(yuv_kernel);
        if (cl_err != CL_SUCCESS)
            fprintf(stderr, "clReleaseKernel error %d", cl_err);
        yuv_kernel = NULL;
    }

    if (out_mem != NULL) {
        cl_err = clReleaseMemObject(out_mem);
        if (cl_err != CL_SUCCESS)
            fprintf(stderr, "clReleaseMemObject error %d", cl_err);
        out_mem = NULL;
    }

    if (tex_mem != NULL) {
        cl_err = clReleaseMemObject(tex_mem);
        if (cl_err != CL_SUCCESS)
            fprintf(stderr, "clReleaseMemObject error %d", cl_err);
        tex_mem = NULL;
    }

    x264_picture_clean(&out_pic);

    if (clq != NULL) {
        cl_err = clReleaseCommandQueue(clq);
        if (cl_err != CL_SUCCESS)
            fprintf(stderr, "clReleaseCommandQueue error %d", cl_err);
        clq = NULL;
    }

    platform_destroy();

    if (!on_frame.IsEmpty()) {
        on_frame.Dispose();
        on_frame.Clear();
    }

    if (!on_error.IsEmpty()) {
        on_error.Dispose();
        on_error.Clear();
    }

    source_sym.Dispose();
    x1_sym.Dispose();
    y1_sym.Dispose();
    x2_sym.Dispose();
    y2_sym.Dispose();
    u1_sym.Dispose();
    v1_sym.Dispose();
    u2_sym.Dispose();
    v2_sym.Dispose();
    data_sym.Dispose();
    type_sym.Dispose();
    offset_sym.Dispose();
    size_sym.Dispose();
}

Handle<Value> video_mixer_base::pop_last_error()
{
    Handle<Value> ret;
    if (last_error[0] != '\0') {
        ret = Exception::Error(String::New(last_error));
        last_error[0] = '\0';
    }
    return ret;
}

void video_mixer_base::clear_clock()
{
    if (clock != nullptr) {
        clock->unref_mixer(this);
        clock = nullptr;
        Unref();
    }
}

void video_mixer_base::clear_sources()
{
    for (auto &entry : sources) {
        entry.source->unref_mixer(this);
        glDeleteTextures(1, &entry.texture);
        Unref();
    }
    sources.clear();
}

Handle<Value> video_mixer_base::set_clock(const Arguments &args)
{
    if (args.Length() != 1)
        return ThrowException(Exception::TypeError(
            String::New("Expected one argument")));

    video_clock *new_clock = nullptr;
    if (!args[0]->IsNull()) {
        if (!args[0]->IsObject())
            return ThrowException(Exception::TypeError(
                String::New("Expected an object")));

        auto obj = Local<Object>::Cast(args[0]);
        new_clock = ObjectWrap::Unwrap<video_clock>(obj);
    }

    lock_handle lock(clock);
    lock_handle lock2(new_clock);

    clear_clock();

    if (new_clock) {
        Ref();
        clock = new_clock;
        clock->ref_mixer(this);
    }

    return Undefined();
}

Handle<Value> video_mixer_base::set_sources(const Arguments &args)
{
    if (args.Length() != 1)
        return ThrowException(Exception::TypeError(
            String::New("Expected one argument")));

    if (!args[0]->IsArray())
        return ThrowException(Exception::TypeError(
            String::New("Expected an array")));

    lock_handle lock(clock);

    bool ok = activate_gl();

    Handle<Value> val;
    GLenum gl_err;

    std::vector<video_source_entry> new_sources;

    // FIXME: error handling

    if (ok) {
        auto arr = Handle<Array>::Cast(args[0]);
        uint32_t len = arr->Length();
        new_sources.resize(len);

        for (uint32_t i = 0; i < len; i++) {
            auto val = arr->Get(i);
            if (!val->IsObject())
                return ThrowException(Exception::TypeError(
                    String::New("Expected only objects in the array")));

            auto obj = Handle<Object>::Cast(val);
            auto &entry = new_sources[i];

            val = obj->Get(source_sym);
            if (!val->IsObject())
                return ThrowException(Exception::TypeError(
                    String::New("Invalid or missing source")));

            auto source = Handle<Object>::Cast(val);
            entry.source = ObjectWrap::Unwrap<video_source>(source);

            val = obj->Get(x1_sym);
            if (!val->IsNumber())
                return ThrowException(Exception::TypeError(
                    String::New("Invalid or missing coordinate x1")));
            entry.x1 = val->NumberValue();

            val = obj->Get(y1_sym);
            if (!val->IsNumber())
                return ThrowException(Exception::TypeError(
                    String::New("Invalid or missing coordinate y1")));
            entry.y1 = val->NumberValue();

            val = obj->Get(x2_sym);
            if (!val->IsNumber())
                return ThrowException(Exception::TypeError(
                    String::New("Invalid or missing coordinate x2")));
            entry.x2 = val->NumberValue();

            val = obj->Get(y2_sym);
            if (!val->IsNumber())
                return ThrowException(Exception::TypeError(
                    String::New("Invalid or missing coordinate y2")));
            entry.y2 = val->NumberValue();

            val = obj->Get(u1_sym);
            if (!val->IsNumber())
                return ThrowException(Exception::TypeError(
                    String::New("Invalid or missing coordinate u1")));
            entry.u1 = val->NumberValue();

            val = obj->Get(v1_sym);
            if (!val->IsNumber())
                return ThrowException(Exception::TypeError(
                    String::New("Invalid or missing coordinate v1")));
            entry.v1 = val->NumberValue();

            val = obj->Get(u2_sym);
            if (!val->IsNumber())
                return ThrowException(Exception::TypeError(
                    String::New("Invalid or missing coordinate u2")));
            entry.u2 = val->NumberValue();

            val = obj->Get(v2_sym);
            if (!val->IsNumber())
                return ThrowException(Exception::TypeError(
                    String::New("Invalid or missing coordinate v2")));
            entry.v2 = val->NumberValue();

            glGenTextures(1, &entry.texture);
            if (!(ok = ((gl_err = glGetError()) == GL_NO_ERROR))) {
                sprintf(last_error, "OpenGL error %d", gl_err);
                for (uint32_t j = 0; j < i; j++)
                    glDeleteTextures(1, &new_sources[j].texture);
                break;
            }
        }
    }

    if (ok) {
        clear_sources();

        sources = new_sources;
        for (auto &entry : sources) {
            Ref();
            entry.source->ref_mixer(this);
        }

        return Undefined();
    }
    else {
        return ThrowException(pop_last_error());
    }
}

void video_mixer_base::tick(frame_time_t time)
{
    bool ok = (last_error[0] != '\0');

    // Render.
    GLenum gl_err;

    if (ok) {
        ok = activate_gl();
    }

    if (ok) {
        glClear(GL_COLOR_BUFFER_BIT);
        for (auto &entry : sources) {
            glBindTexture(GL_TEXTURE_RECTANGLE, entry.texture);
            current_source = &entry;
            entry.source->frame();
        }
        current_source = nullptr;
        glFinish();
        if (!(ok = ((gl_err = glGetError()) == GL_NO_ERROR)))
            sprintf(last_error, "OpenGL error %d", gl_err);
    }

    // Convert colorspace.
    cl_int cl_err;

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
    if (ok) {
        out_pic.i_dts = out_pic.i_pts = time;
        int i_ret = x264_encoder_encode(video_enc, &nals, &nals_len, &out_pic, &enc_pic);
        if (!(ok = (i_ret >= 0)))
            strcpy(last_error, "x264_encoder_encode error");
    }

    // Signal main thread.
    if (uv_async_send(&callback.async)) {
        const char *err = uv_strerror(uv_last_error(uv_default_loop()));
        fprintf(stderr, "uv_async_send error: %s", err);
    }
}

void video_mixer_base::render_texture()
{
    video_source_entry &e = *current_source;
    glBufferData(GL_ARRAY_BUFFER, vbo_size, (GLfloat []) {
        e.x1, e.y1, e.u1, e.v1,
        e.x1, e.y2, e.u1, e.v2,
        e.x2, e.y1, e.u2, e.v1,
        e.x2, e.y2, e.u2, e.v2
    }, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void video_mixer_base::render_buffer(dimensions_t dimensions, void *data)
{
    glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA8, dimensions.width, dimensions.height, 0,
                 GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, data);
    render_texture();
}

void video_mixer_base::emit_last()
{
    HandleScope scope;
    Handle<Function> listener;
    Handle<Value> arg;

    {
        lock_handle lock(clock);
        arg = pop_last_error();
        if (arg.IsEmpty()) {
            listener = on_frame;
            arg = nals_to_js();
            if (arg.IsEmpty())
                return;
        }
        else {
            listener = on_error;
        }
    }

    listener->Call(handle_, 1, &arg);
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
        if (log) {
            glGetShaderInfoLog(shader, log_size, NULL, log);
            fprintf(stderr, "%s", log);
        }
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
        if (log) {
            glGetProgramInfoLog(program, log_size, NULL, log);
            fprintf(stderr, "%s", log);
        }
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

Handle<Object> video_mixer_base::nals_to_js()
{
    if (nals_len == 0)
        return Handle<Object>();

    auto &last_nal = nals[nals_len - 1];
    auto start = nals[0].p_payload;
    auto end = last_nal.p_payload + last_nal.i_payload;
    size_t size = end - start;
    if (size == 0)
        return Handle<Object>();

    auto ret = Object::New();
    Handle<Value> val;

    val = Buffer::New((const char *) start, size)->handle_;
    ret->Set(data_sym, val);

    auto array = Array::New(nals_len);
    for (int i = 0; i < nals_len; i++) {
        auto &nal = nals[i];
        auto obj = Object::New();

        val = Integer::New(nal.i_type);
        obj->Set(type_sym, val);

        val = Integer::NewFromUnsigned(nal.p_payload - start);
        obj->Set(offset_sym, val);

        val = Integer::New(nal.i_payload);
        obj->Set(size_sym, val);

        array->Set(i, obj);
    }

    return ret;
}

void video_mixer_base::init_prototype(Handle<FunctionTemplate> func)
{
    NODE_DEFINE_CONSTANT(func, NAL_UNKNOWN);
    NODE_DEFINE_CONSTANT(func, NAL_SLICE);
    NODE_DEFINE_CONSTANT(func, NAL_SLICE_DPA);
    NODE_DEFINE_CONSTANT(func, NAL_SLICE_DPB);
    NODE_DEFINE_CONSTANT(func, NAL_SLICE_DPC);
    NODE_DEFINE_CONSTANT(func, NAL_SLICE_IDR);
    NODE_DEFINE_CONSTANT(func, NAL_SEI);
    NODE_DEFINE_CONSTANT(func, NAL_SPS);
    NODE_DEFINE_CONSTANT(func, NAL_PPS);
    NODE_DEFINE_CONSTANT(func, NAL_AUD);
    NODE_DEFINE_CONSTANT(func, NAL_FILLER);

    SetPrototypeMethod(func, "destroy", [](const Arguments &args) -> Handle<Value> {
        auto mixer = ObjectWrap::Unwrap<video_mixer_base>(args.This());
        mixer->destroy();
        return Undefined();
    });
    SetPrototypeMethod(func, "setClock", [](const Arguments &args) -> Handle<Value> {
        auto mixer = ObjectWrap::Unwrap<video_mixer_base>(args.This());
        return mixer->set_clock(args);
    });
    SetPrototypeMethod(func, "setSources", [](const Arguments &args) -> Handle<Value> {
        auto mixer = ObjectWrap::Unwrap<video_mixer_base>(args.This());
        return mixer->set_sources(args);
    });
}

}  // namespace p1stream
