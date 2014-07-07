#ifndef p1_core_priv_h
#define p1_core_priv_h

#include "core.h"

#include <list>
#include <memory>

extern "C" {

#undef DEBUG  // libSYS redefines this
#include <aacenc_lib.h>
#undef DEBUG
#define DBEUG 1

#include <x264.h>

#if __APPLE__
#   include <TargetConditionals.h>
#   if TARGET_OS_MAC
#       include <OpenGL/OpenGL.h>
#       include <OpenGL/gl3.h>
#       include <OpenCL/opencl.h>
#   endif
#endif

}

namespace p1stream {

extern Persistent<String> source_sym;
extern Persistent<String> on_data_sym;
extern Persistent<String> on_error_sym;

extern Persistent<String> buffer_size_sym;
extern Persistent<String> width_sym;
extern Persistent<String> height_sym;
extern Persistent<String> x264_preset_sym;
extern Persistent<String> x264_tuning_sym;
extern Persistent<String> x264_params_sym;
extern Persistent<String> x264_profile_sym;
extern Persistent<String> x1_sym;
extern Persistent<String> y1_sym;
extern Persistent<String> x2_sym;
extern Persistent<String> y2_sym;
extern Persistent<String> u1_sym;
extern Persistent<String> v1_sym;
extern Persistent<String> u2_sym;
extern Persistent<String> v2_sym;
extern Persistent<String> buf_sym;
extern Persistent<String> frames_sym;
extern Persistent<String> pts_sym;
extern Persistent<String> dts_sym;
extern Persistent<String> keyframe_sym;
extern Persistent<String> nals_sym;
extern Persistent<String> type_sym;
extern Persistent<String> priority_sym;
extern Persistent<String> start_sym;
extern Persistent<String> end_sym;

extern Persistent<String> volume_sym;


// ----- Video types ----

class video_clock_context_full;
class video_source_context_full;

// Data we pass between threads. One of these is created per
// x264_encoder_{headers|encode} call.
//
// In memory, this struct lives in the mixer buffer, and is followed by
// an array of x264_nals_t, and then the sequential payloads.
struct video_mixer_frame {
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

    std::unique_ptr<video_clock_context_full> clock_ctx;
    std::list<video_source_context_full> source_ctxes;

    Persistent<Function> on_data;
    Persistent<Function> on_error;

    // These are initialized by platform support.
    cl_context cl;
    GLuint tex;
    GLuint fbo;

    // Render output.
    size_t out_size;
    dimensions_t out_dimensions;
    x264_picture_t out_pic;

    // OpenGL objects.
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

    // Error handling.
    char last_error[128];
    Handle<Value> pop_last_error();

    // Callback.
    uint8_t *buffer;
    uint8_t *buffer_pos;
    uint32_t buffer_size;
    main_loop_callback callback;

    // Internal.
    void emit_last();
    void clear_clock();
    void clear_sources();
    void tick(frame_time_t time);
    GLuint build_shader(GLuint type, const char *source);
    bool build_program();
    bool buffer_nals(x264_nal_t *nals, int nals_len, x264_picture_t *pic);

    static void free_callback(char *data, void *hint);

    // Lockable implementation.
    virtual lockable *lock() final;

    // Platform hooks.
    virtual Handle<Value> platform_init(Handle<Object> params) = 0;
    virtual void platform_destroy() = 0;
    virtual bool activate_gl() = 0;

    // Public JavaScript methods.
    Handle<Value> init(const Arguments &args);
    void destroy(bool unref = true);

    Handle<Value> set_clock(const Arguments &args);
    Handle<Value> set_sources(const Arguments &args);

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


// ----- Audio types -----

// Data we pass between threads. One of these is created per
// aacEncEncode call.
//
// In memory, this struct lives in the mixer buffer, and is followed by
// the encoded payload.
struct audio_mixer_frame {
    int64_t pts;

    size_t size;
    uint8_t data[0];
};

class audio_source_context_full;

class audio_mixer_full : public audio_mixer {
public:
    audio_mixer_full();

    std::list<audio_source_context_full> source_ctxes;

    Persistent<Function> on_data;
    Persistent<Function> on_error;

    // Mix buffer.
    float *mix;
    int64_t mix_time;

    // Encoder.
    HANDLE_AACENCODER enc;

    // Error handling.
    char last_error[128];
    Handle<Value> pop_last_error();

    // Callback.
    uint8_t *buffer;
    uint8_t *buffer_pos;
    main_loop_callback callback;

    // Mix thread.
    threaded_loop thread;
    bool running;

    // Internal.
    void emit_last();
    void clear_sources();
    void loop();
    size_t time_to_samples(int64_t time);
    int64_t samples_to_time(size_t samples);

    static void free_callback(char *data, void *hint);

    // Lockable implementation.
    virtual lockable *lock() final;

    // Public JavaScript methods.
    Handle<Value> init(const Arguments &args);
    void destroy(bool unref = true);

    Handle<Value> set_sources(const Arguments &args);

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

inline video_mixer_base::video_mixer_base()
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

inline audio_mixer_full::audio_mixer_full()
{
}

inline audio_source_context_full::audio_source_context_full(audio_mixer *mixer, audio_source *source)
{
    mixer_ = mixer;
    source_ = source;
}


}  // namespace p1stream

#endif  // p1_core_priv_h
