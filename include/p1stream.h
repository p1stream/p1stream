#ifndef p1stream_h
#define p1stream_h

#include <uv.h>
#include <node.h>
#include <node_object_wrap.h>
#include <functional>

#if __APPLE__
#   include <TargetConditionals.h>
#   if TARGET_OS_MAC
#       include <IOSurface/IOSurface.h>
#       define HAVE_IOSURFACE
#   endif
#endif

namespace p1stream {


// ----- PODs----

typedef int64_t frame_time_t;
typedef uint32_t fourcc_t;

struct fraction_t {
    uint32_t num;
    uint32_t den;
};

struct dimensions_t {
    uint32_t width;
    uint32_t height;
};


// ----- Utility types ----

// Access high resolution, monotonic system clock.
int64_t system_time();

// Base for objects that provide a lock. The lock() method can return another
// lockable, if the object is proxying, or nullptr if no lock was necessary.
class lockable {
protected:
    lockable();

public:
    virtual lockable *lock() = 0;
    virtual void unlock();
};

// RAII lock acquisition.
class lock_handle {
private:
    lockable *object_;

public:
    lock_handle(lockable &object);
    lock_handle(lockable *object);
    ~lock_handle();
};

// Lockable implemented with a mutex.
class lockable_mutex : public lockable {
protected:
    uv_mutex_t mutex;

public:
    lockable_mutex();
    ~lockable_mutex();

    virtual lockable *lock() final;
    virtual void unlock() final;
};

// Wrap a thread with its own loop. The loop should call wait() to pause,
// which will return true if destroy() is waiting for the thread to exit.
class threaded_loop : public lockable_mutex {
private:
    uv_thread_t thread;
    uv_cond_t cond;
    std::function<void ()> fn;

    static void thread_cb(void* arg);

public:
    void init(std::function<void ()> fn_);
    void destroy();

    bool wait(uint64_t timeout);
};

// Wrap uv_async with std::function.
class main_loop_callback {
private:
    uv_async_t async;
    std::function<void ()> fn;

    static void async_cb(uv_async_t* handle);

public:
    void init(std::function<void ()> fn_);
    void destroy();

    void send();
};


// ----- Video types ----

class video_clock_context;
class video_source_context;

// As far as the video mixer is concerned, there's only two threads: the main
// libuv thread and the video clock thread.

// The clock is responsible for the video thread and lock; all of the following
// methods are called with video clock locked if there is one. If there is no
// video clock to lock, there is nothing but the main libuv thread.

// For convenience, the mixer implements lockable as a proxy to the clock if
// there is one, otherwise it's a no-op.

// Sources may introduce their own threads, but will have to manage them on
// their own as well.

class video_mixer : public node::ObjectWrap, public lockable {
protected:
    video_mixer();
};

// Base for video clocks. The clock should start a thread and call back
// once per frame. All video processing will happen on this thread.
class video_clock : public node::ObjectWrap, public lockable {
public:
    // When a clock is linked, it should start calling tick().
    // If the link method throws, it should return false.
    virtual bool link_video_clock(video_clock_context &ctx) = 0;
    virtual void unlink_video_clock(video_clock_context &ctx) = 0;

    // Get the clock rate. The clock should not call back on tick() unless it
    // can report the video frame rate.
    virtual fraction_t video_ticks_per_second(video_clock_context &ctx) = 0;
};

// Context object passed to the clock.
class video_clock_context {
protected:
    video_clock_context();

    video_clock *clock_;
    video_mixer *mixer_;

public:
    // Accessors.
    video_clock *clock();
    video_mixer *mixer();

    // Tick callback that should be called by the clock.
    void tick(frame_time_t time);
};

// Base for video sources.
class video_source : public node::ObjectWrap {
public:
    // When a clock is linked, it will receive produce_video_frame() calls.
    // If the link method throws, it should return false.
    virtual bool link_video_source(video_source_context &ctx);
    virtual void unlink_video_source(video_source_context &ctx);

    // Called when the mixer is rendering a frame. Should call one of the
    // render_*() callbacks.
    virtual void produce_video_frame(video_source_context &ctx) = 0;
};

// Context object passed to the source.
class video_source_context {
protected:
    video_source_context();

    video_source *source_;
    video_mixer *mixer_;

public:
    // Accessors.
    video_source *source();
    video_mixer *mixer();

    // Render callbacks that should be called by sources from within
    // produce_video_frame().
    void render_texture();
    void render_buffer(dimensions_t dimensions, void *data);
#ifdef HAVE_IOSURFACE
    void render_iosurface(IOSurfaceRef surface);
#endif
};


// ----- Audio types -----

class audio_source_context;

// The audio mixer starts a thread timed with the system clock to do encoding.
// Sources all run their own thread, and can call into `render_buffer()`
// without locking.

class audio_mixer : public node::ObjectWrap, public lockable {
protected:
    audio_mixer();
};

// Base for audio sources.
class audio_source : public node::ObjectWrap {
public:
    // When a source is linked, it should start calling render_buffer().
    // If the link method throws, it should return false.
    virtual bool link_audio_source(audio_source_context &ctx) = 0;
    virtual void unlink_audio_source(audio_source_context &ctx) = 0;
};

// Context object passed to audio sources.
class audio_source_context {
protected:
    audio_source_context();

    audio_source *source_;
    audio_mixer *mixer_;

public:
    // Accessors.
    audio_source *source();
    audio_mixer *mixer();

    // Render callback that should be called by sources.
    void render_buffer(int64_t time, float *in, size_t samples);
};


// ----- Inline implementations -----

inline lockable::lockable()
{
}

inline lock_handle::lock_handle(lockable &object)
{
    object_ = object.lock();
}

inline lock_handle::lock_handle(lockable *object)
{
    object_ = object ? object->lock() : nullptr;
}

inline lock_handle::~lock_handle()
{
    if (object_) object_->unlock();
}

inline lockable_mutex::lockable_mutex()
{
    if (uv_mutex_init(&mutex))
        abort();
}

inline lockable_mutex::~lockable_mutex()
{
    uv_mutex_destroy(&mutex);
}

inline void threaded_loop::init(std::function<void ()> fn_)
{
    fn = fn_;
    if (uv_cond_init(&cond))
        abort();
    if (uv_thread_create(&thread, thread_cb, this))
        abort();
}

inline void threaded_loop::destroy()
{
    uv_cond_signal(&cond);
    if (uv_thread_join(&thread))
        abort();
    uv_cond_destroy(&cond);
}

inline bool threaded_loop::wait(uint64_t timeout)
{
    return uv_cond_timedwait(&cond, &mutex, timeout) == 0;
}

inline void main_loop_callback::init(std::function<void ()> fn_)
{
    fn = fn_;
    auto loop = uv_default_loop();
    if (uv_async_init(loop, &async, async_cb))
        abort();
}

inline void main_loop_callback::destroy()
{
    uv_close((uv_handle_t *) &async, NULL);
    async.loop = NULL;
}

inline void main_loop_callback::send()
{
    if (uv_async_send(&async))
        abort();
}

inline video_mixer::video_mixer()
{
}

inline video_clock_context::video_clock_context()
{
}

inline video_clock *video_clock_context::clock()
{
    return clock_;
}

inline video_mixer *video_clock_context::mixer()
{
    return mixer_;
}

inline video_source_context::video_source_context()
{
}

inline video_source *video_source_context::source()
{
    return source_;
}

inline video_mixer *video_source_context::mixer()
{
    return mixer_;
}

inline audio_mixer::audio_mixer()
{
}

inline audio_source_context::audio_source_context()
{
}

inline audio_source *audio_source_context::source()
{
    return source_;
}

inline audio_mixer *audio_source_context::mixer()
{
    return mixer_;
}


}  // namespace p1stream

#endif  // p1stream_h
