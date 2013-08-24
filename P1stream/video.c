#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "p1stream_priv.h"


static void p1_video_init_encoder(P1ContextFull *ctx, P1Config *cfg, P1ConfigSection *sect);
static bool p1_video_parse_encoder_param(P1Config *cfg, const char *key, char *val, void *data);
static void p1_video_finish(P1ContextFull *ctx, int64_t time);
static GLuint p1_build_shader(GLuint type, const char *source);
static void p1_video_build_program(GLuint program, const char *vertexShader, const char *fragmentShader);

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

static GLfloat vbo_data[4 * 4] = {
    -1, -1, 0, 0,
    -1, +1, 0, 1,
    +1, -1, 1, 0,
    +1, +1, 1, 1
};
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

static const size_t in_fps = 60;
static const size_t fps_div = 2;
static const size_t out_fps = in_fps / fps_div;


void p1_video_init(P1ContextFull *ctx, P1Config *cfg, P1ConfigSection *sect)
{
    P1Context *_ctx = (P1Context *) ctx;

    p1_list_init(&_ctx->video_sources);

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

    cgl_err = CGLCreateContext(pixel_format, NULL, &ctx->gl);
    CGLReleasePixelFormat(pixel_format);
    assert(cgl_err == kCGLNoError);

    CGLSetCurrentContext(ctx->gl);

    CGLShareGroupObj share_group = CGLGetShareGroup(ctx->gl);
    cl_context_properties props[] = {
        CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE, (cl_context_properties) share_group,
        0
    };
    ctx->cl = clCreateContext(props, 0, NULL, clLogMessagesToStdoutAPPLE, NULL, NULL);
    assert(ctx->cl != NULL);

    cl_device_id device_id;
    cl_err = clGetContextInfo(ctx->cl, CL_CONTEXT_DEVICES, sizeof(cl_device_id), &device_id, &size);
    assert(cl_err == CL_SUCCESS);
    assert(size != 0);
    ctx->clq = clCreateCommandQueue(ctx->cl, device_id, 0, NULL);
    assert(ctx->clq != NULL);

    p1_video_init_encoder(ctx, cfg, sect);
    i_err = x264_picture_alloc(&ctx->enc_pic, X264_CSP_I420, output_width, output_height);
    assert(i_err == 0);

    glGenVertexArrays(1, &ctx->vao);
    glGenBuffers(1, &ctx->vbo);
    glGenRenderbuffers(1, &ctx->rbo);
    glGenFramebuffers(1, &ctx->fbo);
    glGenTextures(1, &ctx->tex);
    assert(glGetError() == GL_NO_ERROR);

    glBindRenderbuffer(GL_RENDERBUFFER, ctx->fbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, output_width, output_height);
    assert(glGetError() == GL_NO_ERROR);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, ctx->fbo);
    glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, ctx->rbo);
    assert(glGetError() == GL_NO_ERROR);

    glBindBuffer(GL_ARRAY_BUFFER, ctx->vbo);
    glBufferData(GL_ARRAY_BUFFER, vbo_size, vbo_data, GL_DYNAMIC_DRAW);
    assert(glGetError() == GL_NO_ERROR);

    ctx->program = glCreateProgram();
    glBindAttribLocation(ctx->program, 0, "a_Position");
    glBindAttribLocation(ctx->program, 1, "a_TexCoords");
    glBindFragDataLocation(ctx->program, 0, "o_FragColor");
    p1_video_build_program(ctx->program, simple_vertex_shader, simple_fragment_shader);
    ctx->tex_u = glGetUniformLocation(ctx->program, "u_Texture");

    ctx->rbo_mem = clCreateFromGLRenderbuffer(ctx->cl, CL_MEM_READ_ONLY, ctx->rbo, NULL);
    assert(ctx->rbo_mem != NULL);

    ctx->out_mem = clCreateBuffer(ctx->cl, CL_MEM_WRITE_ONLY, output_yuv_size, NULL, NULL);
    assert(ctx->out_mem != NULL);

    cl_program yuv_program = clCreateProgramWithSource(ctx->cl, 1, &yuv_kernel_source, NULL, NULL);
    assert(yuv_program != NULL);
    cl_err = clBuildProgram(yuv_program, 0, NULL, NULL, NULL, NULL);
    assert(cl_err == CL_SUCCESS);
    ctx->yuv_kernel = clCreateKernel(yuv_program, "yuv", NULL);
    assert(ctx->yuv_kernel != NULL);
    clReleaseProgram(yuv_program);

    /* State init. This is only up here because we can. */
    glViewport(0, 0, output_width, output_height);
    glClearColor(0, 0, 0, 1);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_RECTANGLE, ctx->tex);
    glUseProgram(ctx->program);
    glUniform1i(ctx->tex_u, 0);
    glBindVertexArray(ctx->vao);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, vbo_stride, 0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, vbo_stride, vbo_tex_coord_offset);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    assert(glGetError() == GL_NO_ERROR);

    cl_err = clSetKernelArg(ctx->yuv_kernel, 0, sizeof(cl_mem), &ctx->rbo_mem);
    assert(cl_err == CL_SUCCESS);
    cl_err = clSetKernelArg(ctx->yuv_kernel, 1, sizeof(cl_mem), &ctx->out_mem);
    assert(cl_err == CL_SUCCESS);
}

static void p1_video_init_encoder(P1ContextFull *ctx, P1Config *cfg, P1ConfigSection *sect)
{
    int i_err;
    char tmp[128];

    x264_param_t params;
    x264_param_default(&params);

    if (cfg->get_string(cfg, sect, "encoder.preset", tmp, sizeof(tmp))) {
        i_err = x264_param_default_preset(&params, tmp, NULL);
        assert(i_err == 0);
    }

    if (!cfg->each_string(cfg, sect, "encoder", p1_video_parse_encoder_param, &params)) {
        abort();
    }

    mach_timebase_info_data_t timebase;
    mach_timebase_info(&timebase);
    params.i_timebase_num = timebase.numer;
    params.i_timebase_den = timebase.denom * 1000000000;

    params.b_aud = 1;
    params.b_annexb = 0;

    params.i_width = output_width;
    params.i_height = output_height;

    params.i_fps_num = out_fps;
    params.i_fps_den = 1;

    x264_param_apply_fastfirstpass(&params);

    if (cfg->get_string(cfg, sect, "encoder.profile", tmp, sizeof(tmp))) {
        i_err = x264_param_apply_profile(&params, tmp);
        assert(i_err == 0);
    }

    ctx->enc = x264_encoder_open(&params);
    assert(ctx->enc != NULL);
}

static bool p1_video_parse_encoder_param(P1Config *cfg, const char *key, char *val, void *data)
{
    x264_param_t *params = (x264_param_t *) data;

    if (strcmp(key, "preset") == 0 || strcmp(key, "profile") == 0)
        return true;

    return x264_param_parse(params, key, val) == 0;
}

void p1_video_output(P1VideoClock *vclock, int64_t time)
{
    P1Context *_ctx = vclock->ctx;
    P1ContextFull *ctx = (P1ContextFull *) _ctx;
    P1ListNode *head;
    P1ListNode *node;

    if (ctx->skip_counter >= fps_div)
        ctx->skip_counter = 0;
    if (ctx->skip_counter++ != 0)
        return;

    CGLSetCurrentContext(ctx->gl);
    glClear(GL_COLOR_BUFFER_BIT);

    head = &_ctx->video_sources;
    p1_list_iterate(head, node) {
        P1Source *src = (P1Source *) node;
        P1VideoSource *vsrc = (P1VideoSource *) node;

        if (src->state == P1StateRunning) {
            vsrc->frame(vsrc);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }
    }

    glFinish();
    assert(glGetError() == GL_NO_ERROR);

    p1_video_finish(ctx, time);
}

void p1_video_frame(P1VideoSource *vsrc, int width, int height, void *data)
{
    glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA8, width, height, 0,
                 GL_BGRA, GL_UNSIGNED_BYTE, data);
}

static void p1_video_finish(P1ContextFull *ctx, int64_t time)
{
    cl_int cl_err;

    cl_err = clEnqueueAcquireGLObjects(ctx->clq, 1, &ctx->rbo_mem, 0, NULL, NULL);
    assert(cl_err == CL_SUCCESS);
    cl_err = clEnqueueNDRangeKernel(ctx->clq, ctx->yuv_kernel, 2, NULL, yuv_work_size, NULL, 0, NULL, NULL);
    assert(cl_err == CL_SUCCESS);
    cl_err = clEnqueueReleaseGLObjects(ctx->clq, 1, &ctx->rbo_mem, 0, NULL, NULL);
    assert(cl_err == CL_SUCCESS);
    cl_err = clEnqueueReadBuffer(ctx->clq, ctx->out_mem, CL_FALSE, 0, output_yuv_size, ctx->enc_pic.img.plane[0], 0, NULL, NULL);
    assert(cl_err == CL_SUCCESS);
    cl_err = clFinish(ctx->clq);
    assert(cl_err == CL_SUCCESS);

    x264_nal_t *nals;
    int len;
    int ret;

    if (!ctx->sent_video_config) {
        ctx->sent_video_config = true;
        ret = x264_encoder_headers(ctx->enc, &nals, &len);
        assert(ret >= 0);
        p1_stream_video_config(ctx, nals, len);
    }

    x264_picture_t out_pic;
    ctx->enc_pic.i_dts = time;
    ctx->enc_pic.i_pts = time;
    ret = x264_encoder_encode(ctx->enc, &nals, &len, &ctx->enc_pic, &out_pic);
    assert(ret >= 0);
    if (len)
        p1_stream_video(ctx, nals, len, &out_pic);
}

static GLuint p1_build_shader(GLuint type, const char *source)
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
            printf("Shader compiler log:\n%s", log);
            free(log);
        }
    }

    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    assert(success == GL_TRUE);
    assert(glGetError() == GL_NO_ERROR);

    return shader;
}

static void p1_video_build_program(GLuint program, const char *vertex_source, const char *fragment_source)
{
    GLuint vertex_shader = p1_build_shader(GL_VERTEX_SHADER, vertex_source);
    GLuint fragment_shader = p1_build_shader(GL_FRAGMENT_SHADER, fragment_source);

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
            printf("Shader linker log:\n%s", log);
            free(log);
        }
    }

    GLint success = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    assert(success == GL_TRUE);
    assert(glGetError() == GL_NO_ERROR);
}
