#include "p1stream_priv.h"

#include <string.h>
#include <assert.h>

static bool p1_video_init_encoder_params(P1VideoFull *videof, P1Config *cfg, P1ConfigSection *sect);
static bool p1_video_parse_encoder_param(P1Config *cfg, const char *key, char *val, void *data);
static void p1_video_encoder_log_callback(void *data, int level, const char *fmt, va_list args);
static GLuint p1_build_shader(P1Object *videoobj, GLuint type, const char *source);
static void p1_video_build_program(P1Object *videoobj, GLuint program, const char *vertexShader, const char *fragmentShader);

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

    if (!p1_video_init_encoder_params(videof, cfg, sect))
        goto fail_params;

    return true;

fail_params:
    p1_object_destroy(videoobj);

fail_object:
    return false;
}

void p1_video_start(P1VideoFull *videof)
{
    P1Object *videoobj = (P1Object *) videof;
    P1Video *video = (P1Video *) videof;
    P1VideoClock *vclock = video->clock;
    x264_param_t *params = &videof->params;
    CGLError cgl_err;
    cl_int cl_err;
    int i_err;
    size_t size;

    CGLPixelFormatObj pixel_format;
    const CGLPixelFormatAttribute attribs[] = {
        kCGLPFAOpenGLProfile, (CGLPixelFormatAttribute) kCGLOGLPVersion_3_2_Core,
        0
    };
    GLint npix;
    cgl_err = CGLChoosePixelFormat(attribs, &pixel_format, &npix);
    assert(cgl_err == kCGLNoError);

    cgl_err = CGLCreateContext(pixel_format, NULL, &videof->gl);
    CGLReleasePixelFormat(pixel_format);
    assert(cgl_err == kCGLNoError);

    CGLSetCurrentContext(videof->gl);

    CGLShareGroupObj share_group = CGLGetShareGroup(videof->gl);
    cl_context_properties props[] = {
        CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE, (cl_context_properties) share_group,
        0
    };
    videof->cl = clCreateContext(props, 0, NULL, clLogMessagesToStdoutAPPLE, NULL, NULL);
    assert(videof->cl != NULL);

    cl_device_id device_id;
    cl_err = clGetContextInfo(videof->cl, CL_CONTEXT_DEVICES, sizeof(cl_device_id), &device_id, &size);
    assert(cl_err == CL_SUCCESS);
    assert(size != 0);
    videof->clq = clCreateCommandQueue(videof->cl, device_id, 0, NULL);
    assert(videof->clq != NULL);

    params->pf_log = p1_video_encoder_log_callback;
    params->p_log_private = videoobj;
    params->i_log_level = X264_LOG_DEBUG;

    mach_timebase_info_data_t timebase;
    mach_timebase_info(&timebase);
    params->i_timebase_num = timebase.numer;
    params->i_timebase_den = timebase.denom * 1000000000;

    params->b_aud = 1;
    params->b_annexb = 0;

    params->i_width = output_width;
    params->i_height = output_height;

    params->i_fps_num = vclock->fps_num;
    params->i_fps_den = vclock->fps_den;

    videof->enc = x264_encoder_open(params);
    assert(videof->enc != NULL);

    i_err = x264_picture_alloc(&videof->enc_pic, X264_CSP_I420, output_width, output_height);
    assert(i_err == 0);

    glGenVertexArrays(1, &videof->vao);
    glGenBuffers(1, &videof->vbo);
    glGenRenderbuffers(1, &videof->rbo);
    glGenFramebuffers(1, &videof->fbo);
    assert(glGetError() == GL_NO_ERROR);

    glBindRenderbuffer(GL_RENDERBUFFER, videof->fbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, output_width, output_height);
    assert(glGetError() == GL_NO_ERROR);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, videof->fbo);
    glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, videof->rbo);
    assert(glGetError() == GL_NO_ERROR);

    videof->program = glCreateProgram();
    glBindAttribLocation(videof->program, 0, "a_Position");
    glBindAttribLocation(videof->program, 1, "a_TexCoords");
    glBindFragDataLocation(videof->program, 0, "o_FragColor");
    p1_video_build_program(videoobj, videof->program, simple_vertex_shader, simple_fragment_shader);
    videof->tex_u = glGetUniformLocation(videof->program, "u_Texture");

    videof->rbo_mem = clCreateFromGLRenderbuffer(videof->cl, CL_MEM_READ_ONLY, videof->rbo, NULL);
    assert(videof->rbo_mem != NULL);

    videof->out_mem = clCreateBuffer(videof->cl, CL_MEM_WRITE_ONLY, output_yuv_size, NULL, NULL);
    assert(videof->out_mem != NULL);

    cl_program yuv_program = clCreateProgramWithSource(videof->cl, 1, &yuv_kernel_source, NULL, NULL);
    assert(yuv_program != NULL);
    cl_err = clBuildProgram(yuv_program, 0, NULL, NULL, NULL, NULL);
    assert(cl_err == CL_SUCCESS);
    videof->yuv_kernel = clCreateKernel(yuv_program, "yuv", NULL);
    assert(videof->yuv_kernel != NULL);
    clReleaseProgram(yuv_program);

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
    assert(glGetError() == GL_NO_ERROR);

    cl_err = clSetKernelArg(videof->yuv_kernel, 0, sizeof(cl_mem), &videof->rbo_mem);
    assert(cl_err == CL_SUCCESS);
    cl_err = clSetKernelArg(videof->yuv_kernel, 1, sizeof(cl_mem), &videof->out_mem);
    assert(cl_err == CL_SUCCESS);

    p1_object_set_state(videoobj, P1_STATE_RUNNING);
}

void p1_video_stop(P1VideoFull *videof)
{
    P1Object *videoobj = (P1Object *) videof;

    clReleaseKernel(videof->yuv_kernel);
    clReleaseMemObject(videof->out_mem);
    clReleaseMemObject(videof->rbo_mem);
    clReleaseCommandQueue(videof->clq);
    clReleaseContext(videof->cl);
    CGLReleaseContext(videof->gl);

    p1_object_set_state(videoobj, P1_STATE_IDLE);
}

static bool p1_video_init_encoder_params(P1VideoFull *videof, P1Config *cfg, P1ConfigSection *sect)
{
    x264_param_t *params = &videof->params;
    char tmp[128];
    int ret;

    // x264 already logs errors, except for x264_param_parse.

    x264_param_default(params);

    if (cfg->get_string(cfg, sect, "encoder.preset", tmp, sizeof(tmp))) {
        ret = x264_param_default_preset(params, tmp, NULL);
        if (ret != 0)
            return false;
    }

    if (cfg->get_string(cfg, sect, "encoder.tune", tmp, sizeof(tmp))) {
        ret = x264_param_default_preset(params, NULL, tmp);
        if (ret != 0)
            return false;
    }

    if (!cfg->each_string(cfg, sect, "encoder", p1_video_parse_encoder_param, videof))
        return false;

    x264_param_apply_fastfirstpass(params);

    if (cfg->get_string(cfg, sect, "encoder.profile", tmp, sizeof(tmp))) {
        ret = x264_param_apply_profile(params, tmp);
        if (ret != 0)
            return false;
    }

    return true;
}

static bool p1_video_parse_encoder_param(P1Config *cfg, const char *key, char *val, void *data)
{
    P1Object *videoobj = (P1Object *) data;
    P1VideoFull *videof = (P1VideoFull *) data;
    int ret;

    if (strcmp(key, "preset") == 0 ||
        strcmp(key, "profile") == 0 ||
        strcmp(key, "tune") == 0)
        return true;

    ret = x264_param_parse(&videof->params, key, val);
    if (ret != 0) {
        if (ret == X264_PARAM_BAD_NAME)
            p1_log(videoobj, P1_LOG_ERROR, "Invalid x264 parameter name '%s'", key);
        else if (ret == X264_PARAM_BAD_VALUE)
            p1_log(videoobj, P1_LOG_ERROR, "Invalid value for x264 parameter '%s'", key);
        return false;
    }

    return true;
}

static void p1_video_encoder_log_callback(void *data, int level, const char *fmt, va_list args)
{
    P1Object *videobj = (P1Object *) data;

    // Strip the newline.
    size_t i = strlen(fmt) - 1;
    char fmt2[i + 1];
    if (fmt[i] == '\n') {
        memcpy(fmt2, fmt, i);
        fmt2[i] = '\0';
        fmt = fmt2;
    }

    p1_logv(videobj, (P1LogLevel) level, fmt, args);
}

void p1_video_clock_tick(P1VideoClock *vclock, int64_t time)
{
    P1Object *obj = (P1Object *) vclock;
    P1Context *ctx = obj->ctx;
    P1Video *video = ctx->video;
    P1VideoFull *videof = (P1VideoFull *) video;
    P1Object *videoobj = (P1Object *) video;
    P1ConnectionFull *connf = (P1ConnectionFull *) ctx->conn;
    P1ListNode *head;
    P1ListNode *node;
    cl_int cl_err;
    x264_nal_t *nals;
    int len;
    int ret;

    p1_object_lock(videoobj);

    if (videoobj->state != P1_STATE_RUNNING) {
        p1_object_unlock(videoobj);
        return;
    }

    CGLSetCurrentContext(videof->gl);
    glClear(GL_COLOR_BUFFER_BIT);

    head = &video->sources;
    p1_list_iterate(head, node) {
        P1Source *src = p1_list_get_container(node, P1Source, link);
        P1Object *el = (P1Object *) src;
        P1VideoSource *vsrc = (P1VideoSource *) src;

        p1_object_lock(el);
        if (el->state == P1_STATE_RUNNING) {
            if (vsrc->texture == 0)
                glGenTextures(1, &vsrc->texture);

            glBindTexture(GL_TEXTURE_RECTANGLE, vsrc->texture);
            vsrc->frame(vsrc);

            glBufferData(GL_ARRAY_BUFFER, vbo_size, (GLfloat []) {
                vsrc->x1, vsrc->y1, vsrc->u1, vsrc->v1,
                vsrc->x1, vsrc->y2, vsrc->u1, vsrc->v2,
                vsrc->x2, vsrc->y1, vsrc->u2, vsrc->v1,
                vsrc->x2, vsrc->y2, vsrc->u2, vsrc->v2
            }, GL_DYNAMIC_DRAW);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }
        p1_object_unlock(el);
    }

    glFinish();
    assert(glGetError() == GL_NO_ERROR);

    cl_err = clEnqueueAcquireGLObjects(videof->clq, 1, &videof->rbo_mem, 0, NULL, NULL);
    assert(cl_err == CL_SUCCESS);
    cl_err = clEnqueueNDRangeKernel(videof->clq, videof->yuv_kernel, 2, NULL, yuv_work_size, NULL, 0, NULL, NULL);
    assert(cl_err == CL_SUCCESS);
    cl_err = clEnqueueReleaseGLObjects(videof->clq, 1, &videof->rbo_mem, 0, NULL, NULL);
    assert(cl_err == CL_SUCCESS);
    cl_err = clEnqueueReadBuffer(videof->clq, videof->out_mem, CL_FALSE, 0, output_yuv_size, videof->enc_pic.img.plane[0], 0, NULL, NULL);
    assert(cl_err == CL_SUCCESS);
    cl_err = clFinish(videof->clq);
    assert(cl_err == CL_SUCCESS);

    if (!videof->sent_config) {
        videof->sent_config = true;
        ret = x264_encoder_headers(videof->enc, &nals, &len);
        assert(ret >= 0);
        p1_conn_video_config(connf, nals, len);
    }

    x264_picture_t out_pic;
    videof->enc_pic.i_dts = time;
    videof->enc_pic.i_pts = time;
    ret = x264_encoder_encode(videof->enc, &nals, &len, &videof->enc_pic, &out_pic);
    assert(ret >= 0);
    if (len)
        p1_conn_video(connf, nals, len, &out_pic);

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
                 GL_BGRA, GL_UNSIGNED_BYTE, data);
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
    assert(success == GL_TRUE);
    assert(glGetError() == GL_NO_ERROR);

    return shader;
}

static void p1_video_build_program(P1Object *videoobj, GLuint program, const char *vertex_source, const char *fragment_source)
{
    GLuint vertex_shader = p1_build_shader(videoobj, GL_VERTEX_SHADER, vertex_source);
    GLuint fragment_shader = p1_build_shader(videoobj, GL_FRAGMENT_SHADER, fragment_source);

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
    assert(success == GL_TRUE);
    assert(glGetError() == GL_NO_ERROR);
}
