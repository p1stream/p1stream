#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <IOSurface/IOSurface.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#include <OpenCL/opencl.h>
#include <x264.h>

#include "video.h"
#include "conf.h"
#include "stream.h"

static bool p1_video_frame_prep(P1VideoSource *src);
static void p1_video_frame_finish();
static GLuint p1_build_shader(GLuint type, const char *source);
static void p1_video_build_program(GLuint program, const char *vertexShader, const char *fragmentShader);

static struct {
    P1VideoClock *clock;
    P1VideoSource *src;

    size_t skip_counter;

    CGLContextObj gl;

    cl_context cl;
    cl_command_queue clq;

    GLuint vao;
    GLuint vbo;
    GLuint rbo;
    GLuint fbo;
    GLuint tex;
    GLuint program;
    GLuint tex_u;

    cl_mem rbo_mem;
    cl_mem out_mem;
    cl_kernel yuv_kernel;

    x264_t *enc;
    x264_picture_t enc_pic;

    bool sent_config;
} state;

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


void p1_video_init()
{
    CGLError cgl_err;
    cl_int cl_err;
    int err;
    size_t size;

    CGLPixelFormatObj pixel_format;
    const CGLPixelFormatAttribute attribs[] = {
        kCGLPFAOpenGLProfile, (CGLPixelFormatAttribute) kCGLOGLPVersion_3_2_Core,
        0
    };
    GLint npix;
    cgl_err = CGLChoosePixelFormat(attribs, &pixel_format, &npix);
    assert(cgl_err == kCGLNoError);

    cgl_err = CGLCreateContext(pixel_format, NULL, &state.gl);
    CGLReleasePixelFormat(pixel_format);
    assert(cgl_err == kCGLNoError);

    CGLSetCurrentContext(state.gl);

    CGLShareGroupObj share_group = CGLGetShareGroup(state.gl);
    cl_context_properties props[] = {
        CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE, (cl_context_properties) share_group,
        0
    };
    state.cl = clCreateContext(props, 0, NULL, clLogMessagesToStdoutAPPLE, NULL, NULL);
    assert(state.cl != NULL);

    cl_device_id device_id;
    cl_err = clGetContextInfo(state.cl, CL_CONTEXT_DEVICES, sizeof(cl_device_id), &device_id, &size);
    assert(cl_err == CL_SUCCESS);
    assert(size != 0);
    state.clq = clCreateCommandQueue(state.cl, device_id, 0, NULL);
    assert(state.clq != NULL);

    p1_conf.encoder.i_width = output_width;
    p1_conf.encoder.i_height = output_height;
    p1_conf.encoder.i_fps_num = out_fps;
    p1_conf.encoder.i_fps_den = 1;
    state.enc = x264_encoder_open(&p1_conf.encoder);
    assert(state.enc != NULL);

    err = x264_picture_alloc(&state.enc_pic, X264_CSP_I420, output_width, output_height);
    assert(err == 0);

    glGenVertexArrays(1, &state.vao);
    glGenBuffers(1, &state.vbo);
    glGenRenderbuffers(1, &state.rbo);
    glGenFramebuffers(1, &state.fbo);
    glGenTextures(1, &state.tex);
    assert(glGetError() == GL_NO_ERROR);

    glBindRenderbuffer(GL_RENDERBUFFER, state.fbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, output_width, output_height);
    assert(glGetError() == GL_NO_ERROR);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, state.fbo);
    glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, state.rbo);
    assert(glGetError() == GL_NO_ERROR);

    glBindBuffer(GL_ARRAY_BUFFER, state.vbo);
    glBufferData(GL_ARRAY_BUFFER, vbo_size, vbo_data, GL_DYNAMIC_DRAW);
    assert(glGetError() == GL_NO_ERROR);

    state.program = glCreateProgram();
    glBindAttribLocation(state.program, 0, "a_Position");
    glBindAttribLocation(state.program, 1, "a_TexCoords");
    glBindFragDataLocation(state.program, 0, "o_FragColor");
    p1_video_build_program(state.program, simple_vertex_shader, simple_fragment_shader);
    state.tex_u = glGetUniformLocation(state.program, "u_Texture");

    state.rbo_mem = clCreateFromGLRenderbuffer(state.cl, CL_MEM_READ_ONLY, state.rbo, NULL);
    assert(state.rbo_mem != NULL);

    state.out_mem = clCreateBuffer(state.cl, CL_MEM_WRITE_ONLY, output_yuv_size, NULL, NULL);
    assert(state.out_mem != NULL);

    cl_program yuv_program = clCreateProgramWithSource(state.cl, 1, &yuv_kernel_source, NULL, NULL);
    assert(yuv_program != NULL);
    cl_err = clBuildProgram(yuv_program, 0, NULL, NULL, NULL, NULL);
    assert(cl_err == CL_SUCCESS);
    state.yuv_kernel = clCreateKernel(yuv_program, "yuv", NULL);
    assert(state.yuv_kernel != NULL);
    clReleaseProgram(yuv_program);

    /* State init. This is only up here because we can. */
    glViewport(0, 0, output_width, output_height);
    glClearColor(0, 0, 0, 1);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_RECTANGLE, state.tex);
    glUseProgram(state.program);
    glUniform1i(state.tex_u, 0);
    glBindVertexArray(state.vao);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, vbo_stride, 0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, vbo_stride, vbo_tex_coord_offset);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    assert(glGetError() == GL_NO_ERROR);

    cl_err = clSetKernelArg(state.yuv_kernel, 0, sizeof(cl_mem), &state.rbo_mem);
    assert(cl_err == CL_SUCCESS);
    cl_err = clSetKernelArg(state.yuv_kernel, 1, sizeof(cl_mem), &state.out_mem);
    assert(cl_err == CL_SUCCESS);
}

void p1_video_set_clock(P1VideoClock *clock)
{
    state.clock = clock;
}

void p1_video_add_source(P1VideoSource *src)
{
    assert(state.src == NULL);
    state.src = src;
}

void p1_video_clock_tick(P1VideoClock *clock, int64_t time)
{
    assert(clock == state.clock);

    if (state.skip_counter >= fps_div)
        state.skip_counter = 0;
    if (state.skip_counter++ != 0)
        return;

    CGLSetCurrentContext(state.gl);
    glClear(GL_COLOR_BUFFER_BIT);

    state.src->frame(state.src);

    p1_video_frame_finish(time);
}

void p1_video_frame_raw(P1VideoSource *src, int width, int height, void *data)
{
    assert(src == state.src);

    glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA8, width, height, 0,
                 GL_BGRA, GL_UNSIGNED_BYTE, data);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glFinish();
    assert(glGetError() == GL_NO_ERROR);
}

void p1_video_frame_iosurface(P1VideoSource *src, IOSurfaceRef buffer)
{
    assert(src == state.src);

    GLsizei width = (GLsizei) IOSurfaceGetWidth(buffer);
    GLsizei height = (GLsizei) IOSurfaceGetHeight(buffer);
    CGLError err = CGLTexImageIOSurface2D(
        state.gl, GL_TEXTURE_RECTANGLE,
        GL_RGBA8, width, height,
        GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, buffer, 0);
    assert(err == kCGLNoError);
}

static void p1_video_frame_finish(int64_t time)
{
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glFinish();
    assert(glGetError() == GL_NO_ERROR);

    cl_int cl_err;

    cl_err = clEnqueueAcquireGLObjects(state.clq, 1, &state.rbo_mem, 0, NULL, NULL);
    assert(cl_err == CL_SUCCESS);
    cl_err = clEnqueueNDRangeKernel(state.clq, state.yuv_kernel, 2, NULL, yuv_work_size, NULL, 0, NULL, NULL);
    assert(cl_err == CL_SUCCESS);
    cl_err = clEnqueueReleaseGLObjects(state.clq, 1, &state.rbo_mem, 0, NULL, NULL);
    assert(cl_err == CL_SUCCESS);
    cl_err = clEnqueueReadBuffer(state.clq, state.out_mem, CL_FALSE, 0, output_yuv_size, state.enc_pic.img.plane[0], 0, NULL, NULL);
    assert(cl_err == CL_SUCCESS);
    cl_err = clFinish(state.clq);
    assert(cl_err == CL_SUCCESS);

    x264_nal_t *nals;
    int len;
    int ret;

    if (!state.sent_config) {
        state.sent_config = true;
        ret = x264_encoder_headers(state.enc, &nals, &len);
        assert(ret >= 0);
        p1_stream_video_config(nals, len);
    }

    x264_picture_t out_pic;
    state.enc_pic.i_dts = time;
    state.enc_pic.i_pts = time;
    ret = x264_encoder_encode(state.enc, &nals, &len, &state.enc_pic, &out_pic);
    assert(ret >= 0);
    if (len)
        p1_stream_video(nals, len, &out_pic);
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
