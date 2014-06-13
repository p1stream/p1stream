#ifndef p1_core_h
#define p1_core_h

#include <node.h>
#include <functional>

#if __APPLE__
#   include <TargetConditionals.h>
#   if TARGET_OS_MAC
#       include <IOSurface/IOSurface.h>
#       define HAVE_IOSURFACE
#   endif
#endif

namespace p1stream {


using namespace v8;
using namespace node;

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

// Simple locking helper.
class lockable {
    friend class lock_handle;
    uv_mutex_t mutex;

protected:
    lockable();

public:
    virtual ~lockable();
};

class lock_handle {
    lockable *object;

public:
    lock_handle(lockable *object);
    ~lock_handle();
};


// ----- Video types ----

// As far as the video mixer is concerned, there's only two threads: the main
// libuv thread and the video clock thread.

// The clock is responsible for the video thread and lock; all of the following
// methods are called with video clock locked if there is one. If there is no
// video clock to lock, there is nothing but the main libuv thread.

// Sources may introduce their own threads, but will have to manage them on
// their own as well.

// Interface for the video mixer.
class video_mixer : public ObjectWrap {
public:
    // Tick callback that should be called by the clock.
    virtual void tick(frame_time_t time) = 0;

    // Render callback that should be called by sources from video().
    virtual void render_texture() = 0;

    // Alternative render callback that reads from a buffer.
    virtual void render_buffer(dimensions_t dimensions, void *data) = 0;

#ifdef HAVE_IOSURFACE
    // Alternative render callback that uses an IOSurface.
    virtual void render_iosurface(IOSurfaceRef surface) = 0;
#endif
};

// Base for video clocks. The clock should start a thread and call back
// once per frame. All video processing will happen on this thread.
class video_clock : public ObjectWrap, public lockable {
public:
    // Get the clock rate. The clock should not call back on tick() unless it
    // can report the video frame rate.
    virtual fraction_t ticks_per_second() = 0;

    // Reference a mixer, and call back on its tick() method.
    virtual void ref_mixer(video_mixer *mixer) = 0;

    // Unreference a mixer, and stop calling back.
    virtual void unref_mixer(video_mixer *mixer) = 0;
};

// Base for video sources. Video sources produce an image on each tick.
class video_source : public ObjectWrap {
public:
    // Called when the mixer is rendering a frame. Should call one of the
    // render_*() callbacks of the mixer.
    virtual void frame(video_mixer *mixer) = 0;

    // Reference a mixer, it will start calling frame().
    virtual void ref_mixer(video_mixer *mixer) = 0;

    // Unreference a mixer, it will no longer call frame().
    virtual void unref_mixer(video_mixer *mixer) = 0;
};


// ----- Inline implementations -----

inline lockable::lockable()
{
    if (uv_mutex_init(&mutex))
        abort();
}

inline lockable::~lockable()
{
    uv_mutex_destroy(&mutex);
}

inline lock_handle::lock_handle(lockable *object)
    : object(object)
{
    if (object != nullptr)
        uv_mutex_lock(&object->mutex);
}

inline lock_handle::~lock_handle()
{
    if (object != nullptr)
        uv_mutex_unlock(&object->mutex);
}


}  // namespace p1stream

#endif  // p1_core_h
