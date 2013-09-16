#include "p1stream_priv.h"

#include <unistd.h>
#include <sys/poll.h>
#include <mach/mach_error.h>

typedef enum _P1Action P1Action;

static bool p1_init(P1ContextFull *ctxf, P1Config *cfg, P1ConfigSection *sect);
static void p1_destroy(P1ContextFull *ctxf);
static void p1_close_pipe(P1Object *ctxobj, int fd);
static void p1_log_default(P1Object *obj, P1LogLevel level, const char *fmt, va_list args);
static void *p1_ctrl_main(void *data);
static bool p1_ctrl_progress(P1Context *ctx);
static P1Action p1_ctrl_determine_action(P1Context *ctx, P1State state, P1TargetState target);
static void p1_ctrl_comm(P1ContextFull *ctxf);
static void p1_ctrl_log_notification(P1Notification *notification);

// Based on state and target, one of these actions is taken.
enum _P1Action {
    P1_ACTION_NONE      = 0,
    P1_ACTION_WAIT      = 1,    // Same as none, but don't exit yet if stopping.
    P1_ACTION_START     = 2,
    P1_ACTION_STOP      = 3,
    P1_ACTION_REMOVE    = 4
};


bool p1_object_init(P1Object *obj, P1ObjectType type)
{
    obj->type = type;

    int ret = pthread_mutex_init(&obj->lock, NULL);
    if (ret != 0) {
        p1_log(obj, P1_LOG_ERROR, "Failed to initialize mutex: %s", strerror(ret));
        return false;
    }

    return true;
}

void p1_object_destroy(P1Object *obj)
{
    int ret = pthread_mutex_destroy(&obj->lock);
    if (ret != 0)
        p1_log(obj, P1_LOG_ERROR, "Failed to destroy mutex: %s", strerror(ret));
}

void p1_plugin_free(P1Plugin *pel)
{
    P1Object *obj = (P1Object *) pel;

    p1_object_destroy(obj);

    if (pel->free)
        pel->free(pel);
    else
        free(pel);
}

P1Context *p1_create(P1Config *cfg, P1ConfigSection *sect)
{
    P1ConfigSection *video_sect = cfg->get_section(cfg, sect, "video");
    P1ConfigSection *audio_sect = cfg->get_section(cfg, sect, "audio");
    P1ConfigSection *stream_sect = cfg->get_section(cfg, sect, "stream");

    P1Context *ctx = calloc(1,
        sizeof(P1ContextFull) + sizeof(P1VideoFull) +
        sizeof(P1AudioFull) + sizeof(P1ConnectionFull));

    P1ContextFull *ctxf = (P1ContextFull *) ctx;
    P1VideoFull *videof = (P1VideoFull *) (ctxf + 1);
    P1AudioFull *audiof = (P1AudioFull *) (videof + 1);
    P1ConnectionFull *connf = (P1ConnectionFull *) (audiof + 1);

    if (!ctx)
        goto fail_alloc;

    if (!p1_init(ctxf, cfg, sect))
        goto fail_context;

    if (!p1_video_init(videof, cfg, video_sect))
        goto fail_video;
    ctx->video = (P1Video *) videof;
    ((P1Object *) videof)->ctx = ctx;

    if (!p1_audio_init(audiof, cfg, audio_sect))
        goto fail_audio;
    ctx->audio = (P1Audio *) audiof;
    ((P1Object *) audiof)->ctx = ctx;

    if (!p1_conn_init(connf, cfg, stream_sect))
        goto fail_conn;
    ctx->conn = (P1Connection *) connf;
    ((P1Object *) connf)->ctx = ctx;

    return ctx;

fail_conn:
    p1_audio_destroy(audiof);

fail_audio:
    p1_video_destroy(videof);

fail_video:
    p1_destroy(ctxf);

fail_context:
    free(ctx);

fail_alloc:
    return NULL;
}

static bool p1_init(P1ContextFull *ctxf, P1Config *cfg, P1ConfigSection *sect)
{
    P1Context *ctx = (P1Context *) ctxf;
    P1Object *ctxobj = (P1Object *) ctxf;
    int ret;

    if (!p1_object_init(ctxobj, P1_OTYPE_CONTEXT))
        goto fail_object;

    ret = pipe(ctxf->ctrl_pipe);
    if (ret != 0) {
        p1_log(ctxobj, P1_LOG_ERROR, "Failed to open pipe: %s", strerror(errno));
        goto fail_ctrl;
    }

    ret = pipe(ctxf->user_pipe);
    if (ret != 0) {
        p1_log(ctxobj, P1_LOG_ERROR, "Failed to open pipe: %s", strerror(errno));
        goto fail_user;
    }

    ret = mach_timebase_info(&ctxf->timebase);
    if (ret != 0) {
        p1_log(ctxobj, P1_LOG_ERROR, "Failed to get timebase: %s", mach_error_string(errno));
        goto fail_timebase;
    }

    ctxobj->ctx = ctx;
    ctx->log_level = P1_LOG_INFO;

    return true;

fail_timebase:
    p1_close_pipe(ctxobj, ctxf->user_pipe[0]);
    p1_close_pipe(ctxobj, ctxf->user_pipe[1]);

fail_user:
    p1_close_pipe(ctxobj, ctxf->ctrl_pipe[0]);
    p1_close_pipe(ctxobj, ctxf->ctrl_pipe[1]);

fail_ctrl:
    p1_object_destroy(ctxobj);

fail_object:
    return false;
}

static void p1_destroy(P1ContextFull *ctxf)
{
    P1Object *ctxobj = (P1Object *) ctxf;

    p1_close_pipe(ctxobj, ctxf->ctrl_pipe[0]);
    p1_close_pipe(ctxobj, ctxf->ctrl_pipe[1]);

    p1_close_pipe(ctxobj, ctxf->user_pipe[0]);
    p1_close_pipe(ctxobj, ctxf->user_pipe[1]);

    p1_object_destroy(ctxobj);
}

void p1_free(P1Context *ctx, P1FreeOptions options)
{
    P1ContextFull *ctxf = (P1ContextFull *) ctx;
    P1ListNode *head;
    P1ListNode *node;

    if ((options & P1_FREE_VIDEO_CLOCK) && ctx->video->clock != NULL)
        p1_plugin_free((P1Plugin *) ctx->video->clock);

    if (options & P1_FREE_VIDEO_SOURCES) {
        head = &ctx->video->sources;
        p1_list_iterate(head, node) {
            P1Source *src = p1_list_get_container(node, P1Source, link);
            p1_plugin_free((P1Plugin *) src);
        }
    }

    if (options & P1_FREE_AUDIO_SOURCES) {
        head = &ctx->audio->sources;
        p1_list_iterate(head, node) {
            P1Source *src = p1_list_get_container(node, P1Source, link);
            p1_plugin_free((P1Plugin *) src);
        }
    }

    p1_conn_destroy((P1ConnectionFull *) ctx->conn);
    p1_audio_destroy((P1AudioFull *) ctx->audio);
    p1_video_destroy((P1VideoFull *) ctx->video);
    p1_destroy(ctxf);

    free(ctxf);
}

static void p1_close_pipe(P1Object *ctxobj, int fd)
{
    int ret = close(fd);
    if (ret != 0)
        p1_log(ctxobj, P1_LOG_ERROR, "Failed to close pipe: %s", strerror(errno));
}

bool p1_start(P1Context *ctx)
{
    P1Object *ctxobj = (P1Object *) ctx;
    P1ContextFull *ctxf = (P1ContextFull *) ctx;
    bool result = true;

    p1_object_lock(ctxobj);

    p1_object_set_target(ctxobj, P1_TARGET_RUNNING);

    // Bootstrap.
    if (ctxobj->state == P1_STATE_IDLE) {
        p1_object_set_state(ctxobj, P1_STATE_STARTING);

        int ret = pthread_create(&ctxf->ctrl_thread, NULL, p1_ctrl_main, ctx);
        if (ret != 0) {
            p1_log(ctxobj, P1_LOG_ERROR, "Failed to start control thread: %s", strerror(ret));
            p1_object_set_state(ctxobj, P1_STATE_IDLE);
            result = false;
        }
    }

    p1_object_unlock(ctxobj);

    return result;
}

void p1_stop(P1Context *ctx, P1StopOptions options)
{
    P1Object *ctxobj = (P1Object *) ctx;
    P1ContextFull *ctxf = (P1ContextFull *) ctx;
    bool is_idle;

    p1_object_lock(ctxobj);

    p1_object_set_target(ctxobj, P1_TARGET_IDLE);
    is_idle = (ctxobj->state == P1_STATE_IDLE);

    // Set to stopping immediately, if possible.
    if (ctxobj->state == P1_STATE_RUNNING)
        p1_object_set_state(ctxobj, P1_STATE_STOPPING);

    p1_object_unlock(ctxobj);

    if (!is_idle && (options & P1_STOP_SYNC)) {
        int ret = pthread_join(ctxf->ctrl_thread, NULL);
        if (ret != 0)
            p1_log(ctxobj, P1_LOG_ERROR, "Failed to stop control thread: %s", strerror(ret));
    }
}

void p1_read(P1Context *ctx, P1Notification *out)
{
    P1Object *ctxobj = (P1Object *) ctx;
    P1ContextFull *ctxf = (P1ContextFull *) ctx;

    ssize_t size = sizeof(P1Notification);
    ssize_t ret = read(ctxf->user_pipe[0], out, size);
    if (ret != size) {
        const char *reason = (ret < 0) ? strerror(errno) : "Invalid read";
        p1_log(ctxobj, P1_LOG_ERROR, "Failed to read notification: %s", reason);

        out->type = P1_NTYPE_NULL;
    }
}

int p1_fd(P1Context *_ctx)
{
    P1ContextFull *ctx = (P1ContextFull *) _ctx;

    return ctx->user_pipe[0];
}

void _p1_notify(P1Notification notification)
{
    P1Object *ctxobj = (P1Object *) notification.object->ctx;
    P1ContextFull *ctxf = (P1ContextFull *) ctxobj;

    p1_ctrl_log_notification(&notification);

    ssize_t size = sizeof(P1Notification);
    ssize_t ret = write(ctxf->ctrl_pipe[1], &notification, size);
    if (ret != size) {
        const char *reason = (ret < 0) ? strerror(errno) : "Invalid write";
        p1_log(ctxobj, P1_LOG_ERROR, "Failed to write notification: %s", reason);
    }
}

void p1_log(P1Object *obj, P1LogLevel level, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    p1_logv(obj, level, fmt, args);
    va_end(args);
}

void p1_logv(P1Object *obj, P1LogLevel level, const char *fmt, va_list args)
{
    P1Context *ctx = (obj != NULL) ? obj->ctx : NULL;
    P1LogCallback fn = p1_log_default;

    if (ctx) {
        if (level > ctx->log_level)
            return;
        if (ctx->log_fn)
            fn = ctx->log_fn;
    }

    fn(obj, level, fmt, args);
}

// Default log function.
static void p1_log_default(P1Object *obj, P1LogLevel level, const char *fmt, va_list args)
{
    char out[2048] = "X: ";
    switch (level) {
        case P1_LOG_ERROR:      out[0] = 'E';   break;
        case P1_LOG_WARNING:    out[0] = 'W';   break;
        case P1_LOG_INFO:       out[0] = 'I';   break;
        case P1_LOG_DEBUG:      out[0] = 'D';   break;
        default: break;
    }

    int ret = vsnprintf(out + 3, sizeof(out) - 4, fmt, args);
    if (ret < 0)
        return;
    ret += 3;

    const size_t max_length = sizeof(out) - 2;
    if (ret > max_length)
        ret = max_length;
    out[ret++] = '\n';

    fwrite(out, ret, 1, stderr);
}

// The control thread main loop.
static void *p1_ctrl_main(void *data)
{
    P1Context *ctx = (P1Context *) data;
    P1Object *ctxobj = (P1Object *) data;
    P1ContextFull *ctxf = (P1ContextFull *) ctx;
    bool wait;

    p1_object_lock(ctxobj);

    p1_object_set_state(ctxobj, P1_STATE_RUNNING);

    while (true) {
        // Deal with the notification channels. Don't hold the lock during this.
        p1_object_unlock(ctxobj);
        p1_ctrl_comm(ctxf);
        p1_object_lock(ctxobj);

        // Go into stopping state if that's our goal.
        if (ctxobj->target != P1_TARGET_RUNNING && ctxobj->state == P1_STATE_RUNNING)
            p1_object_set_state(ctxobj, P1_STATE_STOPPING);

        // Progress state of our objects.
        wait = p1_ctrl_progress(ctx);

        // If there's nothing left to wait on, and we're stopping.
        if (!wait && ctxobj->state == P1_STATE_STOPPING) {
            // Restart if our target is no longer to idle.
            if (ctxobj->target == P1_TARGET_RUNNING)
                p1_object_set_state(ctxobj, P1_STATE_RUNNING);
            // Otherwise, we're done.
            else
                break;
        }
    };

    p1_object_set_state(ctxobj, P1_STATE_IDLE);

    p1_object_unlock(ctxobj);

    // One final run to flush notifications.
    p1_ctrl_comm(ctxf);

    return NULL;
}

// Handle communication on pipes.
static void p1_ctrl_comm(P1ContextFull *ctxf)
{
    P1Object *ctxobj = (P1Object *) ctxf;
    struct pollfd fd = {
        .fd = ctxf->ctrl_pipe[0],
        .events = POLLIN
    };
    P1Notification notification;
    ssize_t size = sizeof(P1Notification);
    int i_ret;
    ssize_t s_ret;

    // Wait indefinitely for the next notification.
    do {
        i_ret = poll(&fd, 1, -1);
        if (i_ret == 0)
            p1_log(ctxobj, P1_LOG_WARNING, "Control thread poll interrupted");
        else if (i_ret < 0)
            p1_log(ctxobj, P1_LOG_ERROR, "Control thread failed to poll: %s", strerror(errno));
    } while (i_ret == 0);

    do {
        // Read the notification.
        s_ret = read(fd.fd, &notification, size);
        if (s_ret != size) {
            const char *reason = (s_ret < 0) ? strerror(errno) : "Invalid read";
            p1_log(ctxobj, P1_LOG_ERROR, "Control thread failed to read notification: %s", reason);
            break;
        }

        // Pass it on to the user.
        s_ret = write(ctxf->user_pipe[1], &notification, size);
        if (s_ret != size) {
            const char *reason = (s_ret < 0) ? strerror(errno) : "Invalid write";
            p1_log(ctxobj, P1_LOG_ERROR, "Control thread failed to write notification: %s", reason);
            break;
        }

        // Flush other notifications.
        i_ret = poll(&fd, 1, 0);
        if (i_ret < 0) {
            p1_log(ctxobj, P1_LOG_ERROR, "Control thread failed to poll: %s", strerror(errno));
            break;
        }
    } while (i_ret != 0);
}

// Check all objects and try to progress state.
static bool p1_ctrl_progress(P1Context *ctx)
{
    P1Object *ctxobj = (P1Object *) ctx;
    P1Object *fixed;
    P1Video *video = ctx->video;
    P1VideoFull *videof = (P1VideoFull *) video;
    P1Audio *audio = ctx->audio;
    P1AudioFull *audiof = (P1AudioFull *) audio;
    P1Connection *conn = ctx->conn;
    P1ConnectionFull *connf = (P1ConnectionFull *) conn;
    P1ListNode *head;
    P1ListNode *node;
    bool wait = false;

// After an action, check if we need to wait.
#define P1_CHECK_WAIT(_obj)                                 \
    if ((_obj)->state == P1_STATE_STARTING ||               \
        (_obj)->state == P1_STATE_STOPPING ||               \
        (_obj)->state == P1_STATE_HALTING)                  \
        wait = true;

// Common action handling for plugin elements.
#define P1_PLUGIN_ACTIONS(_obj, _pel)                       \
    case P1_ACTION_WAIT:                                    \
        wait = true;                                        \
        break;                                              \
    case P1_ACTION_START:                                   \
        (_obj)->ctx = ctx;                                  \
        (_pel)->start(_pel);                                \
        P1_CHECK_WAIT(_obj);                                \
        break;                                              \
    case P1_ACTION_STOP:                                    \
        (_pel)->stop(_pel);                                 \
        P1_CHECK_WAIT(_obj);                                \
        break;

// Common action handling for fixed elements.
#define P1_FIXED_ACTIONS(_obj, _full, _start, _stop)        \
    case P1_ACTION_WAIT:                                    \
        wait = true;                                        \
        break;                                              \
    case P1_ACTION_START:                                   \
        (_start)(_full);                                    \
        P1_CHECK_WAIT(_obj);                                \
        break;                                              \
    case P1_ACTION_STOP:                                    \
        (_stop)(_full);                                     \
        P1_CHECK_WAIT(_obj);                                \
        break;

// Short-hand for empty default case.
#define P1_EMPTY_DEFAULT                                    \
    default:                                                \
        break;

    fixed = (P1Object *) video;
    p1_object_lock(fixed);

    // Progress clock. Clock target state is tied to the context.
    P1State vclock_state;
    {
        P1VideoClock *vclock = ctx->video->clock;
        P1Plugin *pel = (P1Plugin *) vclock;
        P1Object *obj = (P1Object *) vclock;

        obj->target = (ctxobj->state == P1_STATE_STOPPING)
                    ? P1_TARGET_IDLE : P1_TARGET_RUNNING;

        p1_object_lock(obj);
        switch (p1_ctrl_determine_action(ctx, obj->state, obj->target)) {
            P1_PLUGIN_ACTIONS(obj, pel)
            P1_EMPTY_DEFAULT
        }
        vclock_state = obj->state;
        p1_object_unlock(obj);
    }

    // Progress video sources.
    head = &video->sources;
    p1_list_iterate(head, node) {
        P1Source *src = p1_list_get_container(node, P1Source, link);
        P1Plugin *pel = (P1Plugin *) src;
        P1Object *obj = (P1Object *) src;

        p1_object_lock(obj);
        P1Action action = p1_ctrl_determine_action(ctx, obj->state, obj->target);
        switch (action) {
            P1_PLUGIN_ACTIONS(obj, pel)
            P1_EMPTY_DEFAULT
        }
        p1_object_unlock(obj);
        if (action == P1_ACTION_REMOVE) {
            p1_list_remove(&src->link);
            p1_plugin_free(pel);
        }
    }

    // Progress video mixer. We need a running clock for this.
    {
        P1TargetState target = (vclock_state != P1_STATE_RUNNING)
                             ? P1_TARGET_IDLE : fixed->target;

        switch (p1_ctrl_determine_action(ctx, fixed->state, target)) {
            P1_FIXED_ACTIONS(fixed, videof, p1_video_start, p1_video_stop)
            P1_EMPTY_DEFAULT
        }
    }

    p1_object_unlock(fixed);

    fixed = (P1Object *) audio;
    p1_object_lock(fixed);

    // Progress audio sources.
    head = &audio->sources;
    p1_list_iterate(head, node) {
        P1Source *src = p1_list_get_container(node, P1Source, link);
        P1Plugin *pel = (P1Plugin *) src;
        P1Object *obj = (P1Object *) src;

        p1_object_lock(obj);
        P1Action action = p1_ctrl_determine_action(ctx, obj->state, obj->target);
        switch (action) {
            P1_PLUGIN_ACTIONS(obj, pel)
            P1_EMPTY_DEFAULT
        }
        p1_object_unlock(obj);
        if (action == P1_ACTION_REMOVE) {
            p1_list_remove(&src->link);
            p1_plugin_free(pel);
        }
    }

    // Progress audio mixer.
    {
        switch (p1_ctrl_determine_action(ctx, fixed->state, fixed->target)) {
            P1_FIXED_ACTIONS(fixed, audiof, p1_audio_start, p1_audio_stop)
            P1_EMPTY_DEFAULT
        }
    }

    p1_object_unlock(fixed);

    // Progress stream connnection. Delay until everything else is running.
    if (!wait) {
        fixed = (P1Object *) conn;
        p1_object_lock(fixed);

        switch (p1_ctrl_determine_action(ctx, fixed->state, fixed->target)) {
            P1_FIXED_ACTIONS(fixed, connf, p1_conn_start, p1_conn_stop)
            P1_EMPTY_DEFAULT
        }

        p1_object_unlock(fixed);
    }

    return wait;

#undef P1_CHECK_WAIT
#undef P1_PLUGIN_ACTIONS
#undef P1_FIXED_ACTIONS
#undef P1_SOURCE_ACTIONS
#undef P1_EMPTY_DEFAULT
}

// Determine action to take on an object.
static P1Action p1_ctrl_determine_action(P1Context *ctx, P1State state, P1TargetState target)
{
    P1Object *ctxobj = (P1Object *) ctx;

    // We need to wait on the transition to finish.
    if (state == P1_STATE_STARTING || state == P1_STATE_STOPPING || state == P1_STATE_HALTING)
        return P1_ACTION_WAIT;

    // If the context is stopping, override target.
    // Make sure we preserve remove targets.
    if (target == P1_TARGET_RUNNING && ctxobj->state == P1_STATE_STOPPING)
        target = P1_TARGET_IDLE;

    // Take steps towards target.
    if (target == P1_TARGET_RUNNING) {
        if (state == P1_STATE_IDLE)
            return P1_ACTION_START;

        return P1_ACTION_NONE;
    }
    else {
        if (state == P1_STATE_RUNNING)
            return P1_ACTION_STOP;

        if (target == P1_TARGET_REMOVE && state == P1_TARGET_IDLE)
            return P1_ACTION_REMOVE;

        return P1_ACTION_NONE;
    }
}

// Log a notification.
static void p1_ctrl_log_notification(P1Notification *notification)
{
    P1Object *obj = notification->object;

    const char *action;
    switch (notification->type) {
        case P1_NTYPE_STATE_CHANGE:
            switch (notification->state_change.state) {
                case P1_STATE_IDLE:     action = "idle";     break;
                case P1_STATE_STARTING: action = "starting"; break;
                case P1_STATE_RUNNING:  action = "running";  break;
                case P1_STATE_STOPPING: action = "stopping"; break;
                case P1_STATE_HALTING:  action = "halting";  break;
                case P1_STATE_HALTED:   action = "halted";   break;
            }
            break;
        case P1_NTYPE_TARGET_CHANGE:
            switch (notification->target_change.target) {
                case P1_TARGET_RUNNING: action = "target running";  break;
                case P1_TARGET_IDLE:    action = "target idle";     break;
                case P1_TARGET_REMOVE:  action = "target remove";   break;
            }
            break;
        default: return;
    }

    const char *obj_descr;
    switch (obj->type) {
        case P1_OTYPE_CONTEXT:      obj_descr = "context";      break;
        case P1_OTYPE_VIDEO:        obj_descr = "video mixer";  break;
        case P1_OTYPE_AUDIO:        obj_descr = "audio mixer";  break;
        case P1_OTYPE_CONNECTION:   obj_descr = "connection";   break;
        case P1_OTYPE_VIDEO_CLOCK:  obj_descr = "video clock";  break;
        case P1_OTYPE_VIDEO_SOURCE: obj_descr = "video source"; break;
        case P1_OTYPE_AUDIO_SOURCE: obj_descr = "audio source"; break;
    }

    p1_log(obj, P1_LOG_INFO, "%s %p is %s", obj_descr, obj, action);
}