#include "p1stream_priv.h"

#include <string.h>

static void p1_video_kill_session(P1VideoFull *videof);
static GLuint p1_build_shader(P1Object *videoobj, GLuint type, const char *source);
static bool p1_video_build_program(P1Object *videoobj, GLuint program, const char *vertexShader, const char *fragmentShader);

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

static const GLsizei output_width = 1280;
static const GLsizei output_height = 720;
static const size_t output_yuv_size = output_width * output_height * 1.5;

static const size_t yuv_work_size[] = {
    output_width / 2,
    output_height / 2
};


bool p1_video_init(P1VideoFull *videof, P1Config *cfg, P1ConfigSection *sect)
{
    P1Video *video = (P1Video *) videof;
    P1Object *videoobj = (P1Object *) videof;

    if (!p1_object_init(videoobj, P1_OTYPE_VIDEO))
        goto fail_object;

    p1_list_init(&video->sources);

    return true;

fail_params:
    p1_object_destroy(videoobj);

fail_object:
    return false;
}

void p1_video_start(P1VideoFull *videof)
{
    P1Object *videoobj = (P1Object *) videof;
    cl_int cl_err;
    GLenum gl_err;
    int i_ret;
    bool b_ret;
    size_t size;

    b_ret = p1_video_init_platform(videof);
    if (!b_ret) {
        goto fail;
    }

    cl_device_id device_id;
    cl_err = clGetContextInfo(videof->cl, CL_CONTEXT_DEVICES, sizeof(cl_device_id), &device_id, &size);
    if (cl_err != CL_SUCCESS || size == 0) {
        p1_log(videoobj, P1_LOG_ERROR, "Failed to get CL device info: OpenCL error %d", cl_err);
        goto fail_platform;
    }

    videof->clq = clCreateCommandQueue(videof->cl, device_id, 0, &cl_err);
    if (cl_err != CL_SUCCESS) {
        p1_log(videoobj, P1_LOG_ERROR, "Failed to create CL command queue: OpenCL error %d", cl_err);
        goto fail_platform;
    }

    i_ret = x264_picture_alloc(&videof->out_pic, X264_CSP_I420, output_width, output_height);
    if (i_ret < 0) {
        p1_log(videoobj, P1_LOG_ERROR, "Failed to alloc x264 picture buffer");
        goto fail_clq;
    }

    glGenVertexArrays(1, &videof->vao);
    glGenBuffers(1, &videof->vbo);
    videof->program = glCreateProgram();
    if ((gl_err = glGetError()) != GL_NO_ERROR) {
        p1_log(videoobj, P1_LOG_ERROR, "Failed to create GL objects: OpenGL error %d", gl_err);
        goto fail_out_pic;
    }

    glBindAttribLocation(videof->program, 0, "a_Position");
    glBindAttribLocation(videof->program, 1, "a_TexCoords");
    glBindFragDataLocation(videof->program, 0, "o_FragColor");
    b_ret = p1_video_build_program(videoobj, videof->program, simple_vertex_shader, simple_fragment_shader);
    if (!b_ret)
        goto fail_out_pic;
    videof->tex_u = glGetUniformLocation(videof->program, "u_Texture");

    videof->tex_mem = clCreateFromGLTexture(videof->cl, CL_MEM_READ_ONLY, GL_TEXTURE_RECTANGLE, 0, videof->tex, &cl_err);
    if (cl_err != CL_SUCCESS) {
        p1_log(videoobj, P1_LOG_ERROR, "Failed to create CL input buffer: OpenCL error %d", cl_err);
        goto fail_out_pic;
    }

    videof->out_mem = clCreateBuffer(videof->cl, CL_MEM_WRITE_ONLY, output_yuv_size, NULL, &cl_err);
    if (cl_err != CL_SUCCESS) {
        p1_log(videoobj, P1_LOG_ERROR, "Failed to create CL output buffer: OpenCL error %d", cl_err);
        goto fail_tex_mem;
    }

    cl_program yuv_program = clCreateProgramWithSource(videof->cl, 1, &yuv_kernel_source, NULL, &cl_err);
    if (cl_err != CL_SUCCESS) {
        p1_log(videoobj, P1_LOG_ERROR, "Failed to create CL program: OpenCL error %d", cl_err);
        goto fail_out_mem;
    }
    cl_err = clBuildProgram(yuv_program, 0, NULL, NULL, NULL, NULL);
    if (cl_err != CL_SUCCESS) {
        p1_log(videoobj, P1_LOG_ERROR, "Failed to build CL program: OpenCL error %d", cl_err);
        clReleaseProgram(yuv_program);
        goto fail_out_mem;
    }
    videof->yuv_kernel = clCreateKernel(yuv_program, "yuv", &cl_err);
    clReleaseProgram(yuv_program);
    if (cl_err != CL_SUCCESS) {
        p1_log(videoobj, P1_LOG_ERROR, "Failed to create CL kernel: OpenCL error %d", cl_err);
        goto fail_out_mem;
    }

    /* State init. This is only up here because we can. */
    glViewport(0, 0, output_width, output_height);
    glClearColor(0, 0, 0, 1);
    glActiveTexture(GL_TEXTURE0);
    glBindBuffer(GL_ARRAY_BUFFER, videof->vbo);
    glUseProgram(videof->program);
    glUniform1i(videof->tex_u, 0);
    glBindVertexArray(videof->vao);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, vbo_stride, 0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, vbo_stride, vbo_tex_coord_offset);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    if ((gl_err = glGetError()) != GL_NO_ERROR) {
        p1_log(videoobj, P1_LOG_ERROR, "Failed to initialize GL state: OpenGL error %d", gl_err);
        goto fail_yuv_kernel;
    }

    cl_err = clSetKernelArg(videof->yuv_kernel, 0, sizeof(cl_mem), &videof->tex_mem);
    if (cl_err != CL_SUCCESS)
        goto fail_kernel_arg;
    cl_err = clSetKernelArg(videof->yuv_kernel, 1, sizeof(cl_mem), &videof->out_mem);
    if (cl_err != CL_SUCCESS)
        goto fail_kernel_arg;

    p1_object_set_state(videoobj, P1_STATE_RUNNING);

    return;

fail_kernel_arg:
    p1_log(videoobj, P1_LOG_ERROR, "Failed to set CL kernel arg: OpenCL error %d", cl_err);

fail_yuv_kernel:
    cl_err = clReleaseKernel(videof->yuv_kernel);
    if (cl_err != CL_SUCCESS)
        p1_log(videoobj, P1_LOG_ERROR, "Failed to release CL kernel: OpenCL error %d", cl_err);

fail_out_mem:
    cl_err = clReleaseMemObject(videof->out_mem);
    if (cl_err != CL_SUCCESS)
        p1_log(videoobj, P1_LOG_ERROR, "Failed to release CL output buffer: OpenCL error %d", cl_err);

fail_tex_mem:
    cl_err = clReleaseMemObject(videof->tex_mem);
    if (cl_err != CL_SUCCESS)
        p1_log(videoobj, P1_LOG_ERROR, "Failed to release CL input buffer: OpenCL error %d", cl_err);

fail_out_pic:
    x264_picture_clean(&videof->out_pic);

fail_clq:
    cl_err = clReleaseCommandQueue(videof->clq);
    if (cl_err != CL_SUCCESS)
        p1_log(videoobj, P1_LOG_ERROR, "Failed to release CL command queue: OpenCL error %d", cl_err);

fail_platform:
    p1_video_destroy_platform(videof);

fail:
    p1_object_set_state(videoobj, P1_STATE_HALTED);
}

void p1_video_stop(P1VideoFull *videof)
{
    P1Object *videoobj = (P1Object *) videof;

    p1_object_set_state(videoobj, P1_STATE_STOPPING);
    p1_video_kill_session(videof);
    p1_object_set_state(videoobj, P1_STATE_IDLE);
}

static void p1_video_kill_session(P1VideoFull *videof)
{
    P1Object *videoobj = (P1Object *) videof;
    cl_int cl_err;

    cl_err = clReleaseKernel(videof->yuv_kernel);
    if (cl_err != CL_SUCCESS)
        p1_log(videoobj, P1_LOG_ERROR, "Failed to release CL kernel: OpenCL error %d", cl_err);

    cl_err = clReleaseMemObject(videof->out_mem);
    if (cl_err != CL_SUCCESS)
        p1_log(videoobj, P1_LOG_ERROR, "Failed to release CL output buffer: OpenCL error %d", cl_err);

    cl_err = clReleaseMemObject(videof->tex_mem);
    if (cl_err != CL_SUCCESS)
        p1_log(videoobj, P1_LOG_ERROR, "Failed to release CL input buffer: OpenCL error %d", cl_err);

    x264_picture_clean(&videof->out_pic);

    cl_err = clReleaseCommandQueue(videof->clq);
    if (cl_err != CL_SUCCESS)
        p1_log(videoobj, P1_LOG_ERROR, "Failed to release CL command queue: OpenCL error %d", cl_err);

    p1_video_destroy_platform(videof);
}


bool p1_video_start_source(P1VideoSource *vsrc)
{
    P1Object *obj = (P1Object *) vsrc;
    P1VideoFull *videof = (P1VideoFull *) obj->ctx->video;
    P1Object *videoobj = (P1Object *) videof;
    GLenum err;

    if (!p1_video_activate_gl(videof))
        return false;

    glGenTextures(1, &vsrc->texture);
    err = glGetError();
    if (err != GL_NO_ERROR) {
        p1_log(videoobj, P1_LOG_ERROR, "Failed to create texture: OpenGL error %d", err);
        return false;
    }

    return true;
}

void p1_video_stop_source(P1VideoSource *vsrc)
{
    P1Object *obj = (P1Object *) vsrc;
    P1Object *videoobj = (P1Object *) obj->ctx->video;
    GLenum err;

    // No use if the context is getting destroyed any way.
    if (videoobj->state != P1_STATE_RUNNING)
        return;

    glDeleteTextures(1, &vsrc->texture);
    err = glGetError();
    if (err != GL_NO_ERROR)
        p1_log(videoobj, P1_LOG_ERROR, "Failed to delete texture: OpenGL error %d", err);
}


void p1_video_clock_tick(P1VideoClock *vclock, int64_t time)
{
    P1Object *obj = (P1Object *) vclock;
    P1Context *ctx = obj->ctx;
    P1Video *video = ctx->video;
    P1VideoFull *videof = (P1VideoFull *) video;
    P1Object *videoobj = (P1Object *) video;
    P1Connection *conn = ctx->conn;
    P1Object *connobj = (P1Object *) conn;
    P1ConnectionFull *connf = (P1ConnectionFull *) conn;
    P1ListNode *head;
    P1ListNode *node;
    GLenum gl_err;
    cl_int cl_err;
    bool b_ret;

    p1_object_lock(videoobj);

    if (videoobj->state != P1_STATE_RUNNING) {
        p1_object_unlock(videoobj);
        return;
    }

    // Rendering
    if (!p1_video_activate_gl(videof))
        goto fail;

    glClear(GL_COLOR_BUFFER_BIT);

    head = &video->sources;
    p1_list_iterate(head, node) {
        P1Source *src = p1_list_get_container(node, P1Source, link);
        P1Object *el = (P1Object *) src;
        P1VideoSource *vsrc = (P1VideoSource *) src;
        b_ret = true;

        p1_object_lock(el);
        if (el->state == P1_STATE_RUNNING) {
            glBindTexture(GL_TEXTURE_RECTANGLE, vsrc->texture);
            b_ret = vsrc->frame(vsrc);

            if (b_ret) {
                glBufferData(GL_ARRAY_BUFFER, vbo_size, (GLfloat []) {
                    vsrc->x1, vsrc->y1, vsrc->u1, vsrc->v1,
                    vsrc->x1, vsrc->y2, vsrc->u1, vsrc->v2,
                    vsrc->x2, vsrc->y1, vsrc->u2, vsrc->v1,
                    vsrc->x2, vsrc->y2, vsrc->u2, vsrc->v2
                }, GL_DYNAMIC_DRAW);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            }
        }
        p1_object_unlock(el);

        if (!b_ret)
            goto fail;
    }

    glFinish();
    if ((gl_err = glGetError()) != GL_NO_ERROR) {
        p1_log(videoobj, P1_LOG_ERROR, "Failed to render frame: OpenGL error %d", gl_err);
        goto fail;
    }

    // Preview hook, in platform specific code
    if (video->preview_fn) {
        b_ret = p1_video_preview(videof);
        if (!b_ret)
            goto fail;
    }

    // Streaming. The state test is a preliminary check. The state may change,
    // and the connection code does a final check itself, but checking here as
    // well saves us a bunch of processing.
    if (connobj->state == P1_STATE_RUNNING) {
        // Colorspace conversion
        cl_err = clEnqueueAcquireGLObjects(videof->clq, 1, &videof->tex_mem, 0, NULL, NULL);
        if (cl_err != CL_SUCCESS) goto fail_cl;
        cl_err = clEnqueueNDRangeKernel(videof->clq, videof->yuv_kernel, 2, NULL, yuv_work_size, NULL, 0, NULL, NULL);
        if (cl_err != CL_SUCCESS) goto fail_cl;
        cl_err = clEnqueueReleaseGLObjects(videof->clq, 1, &videof->tex_mem, 0, NULL, NULL);
        if (cl_err != CL_SUCCESS) goto fail_cl;
        cl_err = clEnqueueReadBuffer(videof->clq, videof->out_mem, CL_FALSE, 0, output_yuv_size, videof->out_pic.img.plane[0], 0, NULL, NULL);
        if (cl_err != CL_SUCCESS) goto fail_cl;
        cl_err = clFinish(videof->clq);
        if (cl_err != CL_SUCCESS) goto fail_cl;

        // Hand off to connection
        p1_conn_stream_video(connf, time, &videof->out_pic);
    }

    p1_object_unlock(videoobj);

    return;

fail_cl:
    p1_log(videoobj, P1_LOG_ERROR, "Failure during colorspace conversion: OpenCL error %d", cl_err);

fail:
    p1_object_set_state(videoobj, P1_STATE_HALTING);
    p1_video_kill_session(videof);
    p1_object_set_state(videoobj, P1_STATE_HALTED);

    p1_object_unlock(videoobj);
}

bool p1_video_clock_init(P1VideoClock *vclock, P1Config *cfg, P1ConfigSection *sect)
{
    P1Object *obj = (P1Object *) vclock;

    if (!p1_object_init(obj, P1_OTYPE_VIDEO_CLOCK))
        return false;

    return true;
}

bool p1_video_source_init(P1VideoSource *vsrc, P1Config *cfg, P1ConfigSection *sect)
{
    P1Object *obj = (P1Object *) vsrc;

    if (!p1_object_init(obj, P1_OTYPE_VIDEO_SOURCE))
        return false;

    if (!cfg->get_float(cfg, sect, "x1", &vsrc->x1))
        vsrc->x1 = -1;
    if (!cfg->get_float(cfg, sect, "y1", &vsrc->y1))
        vsrc->y1 = -1;
    if (!cfg->get_float(cfg, sect, "x2", &vsrc->x2))
        vsrc->x2 = +1;
    if (!cfg->get_float(cfg, sect, "y2", &vsrc->y2))
        vsrc->y2 = +1;
    if (!cfg->get_float(cfg, sect, "u1", &vsrc->u1))
        vsrc->u1 = 0;
    if (!cfg->get_float(cfg, sect, "v1", &vsrc->v1))
        vsrc->v1 = 0;
    if (!cfg->get_float(cfg, sect, "u2", &vsrc->u2))
        vsrc->u2 = 1;
    if (!cfg->get_float(cfg, sect, "v2", &vsrc->v2))
        vsrc->v2 = 1;

    return true;
}

void p1_video_source_frame(P1VideoSource *vsrc, int width, int height, void *data)
{
    glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA8, width, height, 0,
                 GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, data);
}

static GLuint p1_build_shader(P1Object *videoobj, GLuint type, const char *source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint log_size = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_size);
    if (log_size) {
        GLchar *log = malloc(log_size);
        if (log) {
            glGetShaderInfoLog(shader, log_size, NULL, log);
            p1_log(videoobj, P1_LOG_INFO, "Shader compiler log:\n%s", log);
            free(log);
        }
    }

    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        p1_log(videoobj, P1_LOG_ERROR, "Failed to build shader: OpenGL error %d", err);
        return 0;
    }
    if (success != GL_TRUE) {
        p1_log(videoobj, P1_LOG_ERROR, "Failed to build shader");
        return 0;
    }

    return shader;
}

static bool p1_video_build_program(P1Object *videoobj, GLuint program, const char *vertex_source, const char *fragment_source)
{
    GLuint vertex_shader = p1_build_shader(videoobj, GL_VERTEX_SHADER, vertex_source);
    if (vertex_shader == 0)
        return false;

    GLuint fragment_shader = p1_build_shader(videoobj, GL_FRAGMENT_SHADER, fragment_source);
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
        GLchar *log = malloc(log_size);
        if (log) {
            glGetProgramInfoLog(program, log_size, NULL, log);
            p1_log(videoobj, P1_LOG_INFO, "Shader linker log:\n%s", log);
            free(log);
        }
    }

    GLint success = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        p1_log(videoobj, P1_LOG_ERROR, "Failed to link shaders: OpenGL error %d", err);
        return false;
    }
    if (success != GL_TRUE) {
        p1_log(videoobj, P1_LOG_ERROR, "Failed to link shaders");
        return false;
    }

    return true;
}

void p1_video_cl_notify_callback(const char *errstr, const void *private_info, size_t cb, void *user_data)
{
    P1Object *videoobj = (P1Object *) user_data;
    p1_log(videoobj, P1_LOG_INFO, "%s", errstr);
}
