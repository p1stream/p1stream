#ifndef p1_video_base_h
#define p1_video_base_h

#include "core_types.h"
#include "core_util.h"

#include <vector>

extern "C" {
#include <x264.h>
}

#if __APPLE__
#   include <TargetConditionals.h>
#   if TARGET_OS_MAC
#       include <OpenGL/OpenGL.h>
#       include <OpenGL/gl3.h>
#       include <OpenCL/opencl.h>
#   endif
#endif

namespace p1stream {


// Struct for storing a video source and parameters.
struct video_source_entry {
    video_source *source;

    // Texture name. The source need not touch this.
    GLuint texture;

    // Top left and bottom right coordinates of where to place the image in the
    // output. These are in the range [-1, +1].
    GLfloat x1, y1, x2, y2;

    // Top left and bottom right coordinates of the image area to grab, used to
    // achieve clipping. These are in the range [0, 1].
    GLfloat u1, v1, u2, v2;
};

struct video_mixer_callback {
    uv_async_t async;
    std::function<void ()> fn;

    uv_err_code init();
    void close();

    static void async_cb(uv_async_t* handle, int status);
};

// Video mixer.
class video_mixer_base : public video_mixer {
    video_clock *clock;
    std::vector<video_source_entry> sources;
    video_source_entry *current_source;

    Persistent<Function> on_frame;
    Persistent<Function> on_error;

    void clear_clock();
    void clear_sources();
    GLuint build_shader(GLuint type, const char *source);
    bool build_program();
    Handle<Object> nals_to_js();

protected:
    // These are initialized by platform support
    cl_context cl;
    GLuint tex;
    GLuint fbo;

    // Render output
    size_t out_size;
    dimensions_t out_dimensions;
    x264_picture_t out_pic;

    // GL objects
    GLuint vao;
    GLuint vbo;
    GLuint program;
    GLuint tex_u;

    // CL objects
    size_t yuv_work_size[2];
    cl_command_queue clq;
    cl_mem tex_mem;
    cl_mem out_mem;
    cl_kernel yuv_kernel;

    // Video encoding
    x264_t *video_enc;
    x264_nal_t *nals;
    int nals_len;
    x264_picture_t enc_pic;

    // Error handling
    char last_error[128];
    Handle<Value> pop_last_error();

    // Callback
    video_mixer_callback callback;
    void emit_last();

    // Platform hooks.
    virtual Handle<Value> platform_init(Handle<Object> params) = 0;
    virtual void platform_destroy() = 0;
    virtual bool activate_gl() = 0;

public:
    Handle<Value> init(const Arguments &args);
    void destroy(bool unref = true);

    Handle<Value> set_clock(const Arguments &args);
    Handle<Value> set_sources(const Arguments &args);

    virtual void tick(frame_time_t time) final;
    virtual void render_texture() final;
    virtual void render_buffer(dimensions_t dimensions, void *data) final;

    static void init_prototype(Handle<FunctionTemplate> func);
};


// ----- Inline implementations -----

inline uv_err_code video_mixer_callback::init()
{
    auto loop = uv_default_loop();
    if (uv_async_init(loop, &async, async_cb))
        return uv_last_error(loop).code;
    else
        return UV_OK;
}

inline void video_mixer_callback::close()
{
    if (async.loop != NULL) {
        uv_close((uv_handle_t *) &async, NULL);
        async.loop = NULL;
    }
}


}  // namespace p1stream

#endif  // p1_video_base_h
