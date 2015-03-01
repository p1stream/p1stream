#ifndef p1stream_priv_h
#define p1stream_priv_h

#include "p1stream.h"

#include <vector>
#include <list>

extern "C" {

#undef DEBUG  // libSYS redefines this
#include <aacenc_lib.h>
#undef DEBUG
#define DEBUG 1

#include <x264.h>

#if __APPLE__
#   include <OpenGL/OpenGL.h>
#   include <OpenCL/opencl.h>
#else
#   define GL_GLEXT_PROTOTYPES
#   include <CL/opencl.h>
#endif

}

namespace p1stream {

using namespace v8;
using namespace node;

extern Eternal<String> source_sym;
extern Eternal<String> on_event_sym;

extern Eternal<String> width_sym;
extern Eternal<String> height_sym;
extern Eternal<String> x264_preset_sym;
extern Eternal<String> x264_tuning_sym;
extern Eternal<String> x264_params_sym;
extern Eternal<String> x264_profile_sym;
extern Eternal<String> clock_sym;
extern Eternal<String> x1_sym;
extern Eternal<String> y1_sym;
extern Eternal<String> x2_sym;
extern Eternal<String> y2_sym;
extern Eternal<String> u1_sym;
extern Eternal<String> v1_sym;
extern Eternal<String> u2_sym;
extern Eternal<String> v2_sym;
extern Eternal<String> buf_sym;
extern Eternal<String> slice_sym;
extern Eternal<String> length_sym;
extern Eternal<String> parent_sym;
extern Eternal<String> frames_sym;
extern Eternal<String> pts_sym;
extern Eternal<String> dts_sym;
extern Eternal<String> keyframe_sym;
extern Eternal<String> nals_sym;
extern Eternal<String> type_sym;
extern Eternal<String> priority_sym;

extern Eternal<String> numerator_sym;
extern Eternal<String> denominator_sym;

extern Eternal<String> volume_sym;

#define EV_VIDEO_HEADERS 'vhdr'
#define EV_VIDEO_FRAME   'vfrm'
#define EV_AUDIO_HEADERS 'ahdr'
#define EV_AUDIO_FRAME   'afrm'


// ----- Video types ----

class video_clock_context_full;
class video_source_context_full;
class video_hook_context_full;

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

// Lockable is a proxy for the video clock.
class video_mixer_base : public video_mixer {
public:
    video_mixer_base();

    Persistent<Function> on_event;

    bool running;

    video_clock_context_full *clock_ctx;
    std::vector<video_source_context_full> source_ctxes;
    std::vector<video_hook_context_full> hook_ctxes;

    // Objects set up by platform_init. The GL and CL contexts must be in the
    // same share group. The CL context is cleaned up by common code and
    // doesn't need handling in platform_destroy. After platform_init, the GL
    // context should be active and the output texture bound.
    cl_context cl;
    // Additional fields defined in the public header:
    // GLuint texture_;
    // IOSurfaceRef surface_;  // OS X only

    // Render output.
    size_t out_size;
    dimensions_t out_dimensions;
    x264_picture_t out_pic;

    // OpenGL objects.
    GLuint fbo;
    GLuint vao;
    GLuint vbo;
    GLuint program;
    GLuint tex_u;

    // OpenCL objects.
    size_t yuv_work_size[2];
    cl_command_queue clq;
    cl_mem tex_mem;
    cl_mem out_mem;
    cl_kernel yuv_kernel;

    // Video encoding.
    x264_param_t enc_params;
    x264_t *enc;

    // Callback.
    event_buffer buffer;
    main_loop_callback callback;
    Isolate *isolate;
    Persistent<Context> context;

    // Internal.
    void emit_events();
    void clear_sources();
    void clear_hooks();
    void tick(frame_time_t time);
    GLuint build_shader(GLuint type, const char *source);
    bool build_program();
    void buffer_nals(uint32_t id, x264_nal_t *nals, int nals_len, x264_picture_t *pic);

    // Lockable implementation.
    virtual lockable *lock() final;

    // Platform hooks.
    virtual bool platform_init(Handle<Object> params) = 0;
    virtual void platform_destroy() = 0;
    virtual bool activate_gl() = 0;

    // Public JavaScript methods.
    void init(const FunctionCallbackInfo<Value>& args);
    void destroy();

    void set_sources(const FunctionCallbackInfo<Value>& args);
    void set_hooks(const FunctionCallbackInfo<Value>& args);

    // Module init.
    static void init_prototype(Handle<FunctionTemplate> func);
};

class video_clock_context_full : public video_clock_context {
public:
    video_clock_context_full(video_mixer *mixer, video_clock *clock);
};

class video_source_context_full : public video_source_context {
public:
    video_source_context_full(video_mixer *mixer, video_source *source);

    // Texture name.
    GLuint texture;

    // Top left and bottom right coordinates of where to place the image in the
    // output. These are in the range [-1, +1].
    GLfloat x1, y1, x2, y2;

    // Top left and bottom right coordinates of the image area to grab, used to
    // achieve clipping. These are in the range [0, 1].
    GLfloat u1, v1, u2, v2;
};

class video_hook_context_full : public video_hook_context {
public:
    video_hook_context_full(video_mixer *mixer, video_hook *hook);
};


// ----- Software video clock -----

class software_clock : public video_clock {
public:
    software_clock();

    fraction_t rate;

    threaded_loop thread;
    bool running;

    std::list<video_clock_context *> ctxes;

    // Internal.
    void loop();

    // Public JavaScript methods.
    void init(const FunctionCallbackInfo<Value>& args);
    void destroy();

    // Lockable implementation.
    virtual lockable *lock() final;

    // Video clock implementation.
    virtual void link_video_clock(video_clock_context &ctx) final;
    virtual void unlink_video_clock(video_clock_context &ctx) final;
    virtual fraction_t video_ticks_per_second(video_clock_context &ctx) final;

    // Module init.
    static void init_prototype(Handle<FunctionTemplate> func);
};


// ----- Audio types -----

// Audio frame event. One of these is created per aacEncEncode call. The struct
// is directly followed by the payload.
struct audio_frame_data {
    int64_t pts;

    size_t size;
    uint8_t data[0];
};

class audio_source_context_full;

class audio_mixer_full : public audio_mixer {
public:
    audio_mixer_full();

    std::vector<audio_source_context_full> source_ctxes;

    Persistent<Function> on_event;

    // Mix buffer.
    float *mix_buffer;
    int64_t mix_time;

    // Encoder.
    HANDLE_AACENCODER enc;

    // Mix thread.
    threaded_loop thread;
    bool running;

    // Callback.
    event_buffer buffer;
    main_loop_callback callback;
    Isolate *isolate;
    Persistent<Context> context;

    // Internal.
    void emit_events();
    void clear_sources();
    void loop();
    size_t time_to_samples(int64_t time);
    int64_t samples_to_time(size_t samples);

    // Lockable implementation.
    virtual lockable *lock() final;

    // Public JavaScript methods.
    void init(const FunctionCallbackInfo<Value>& args);
    void destroy();

    void set_sources(const FunctionCallbackInfo<Value>& args);

    // Module init.
    static void init_prototype(Handle<FunctionTemplate> func);
};

class audio_source_context_full : public audio_source_context {
public:
    audio_source_context_full(audio_mixer *mixer, audio_source *source);

    // Volume in range [0, 1].
    float volume;
};


// ----- Inline implementations -----

inline video_mixer_base::video_mixer_base() :
    running(), clock_ctx(), cl(), out_pic(), clq(), tex_mem(), out_mem(), yuv_kernel(), enc(),
    buffer(1048576)  // 1 MiB event buffer
{
}

inline video_clock_context_full::video_clock_context_full(video_mixer *mixer, video_clock *clock)
{
    mixer_ = mixer;
    clock_ = clock;
}

inline video_source_context_full::video_source_context_full(video_mixer *mixer, video_source *source)
{
    mixer_ = mixer;
    source_ = source;
}

inline video_hook_context_full::video_hook_context_full(video_mixer *mixer, video_hook *hook)
{
    mixer_ = mixer;
    hook_ = hook;
}

inline software_clock::software_clock() :
    running()
{
}

inline audio_mixer_full::audio_mixer_full() :
    mix_buffer(), enc(), running(),
    buffer(196608)  // 192 KiB event buffer
{
}

inline audio_source_context_full::audio_source_context_full(audio_mixer *mixer, audio_source *source)
{
    mixer_ = mixer;
    source_ = source;
}


}  // namespace p1stream

#endif  // p1stream_priv_h
