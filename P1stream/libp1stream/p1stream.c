#include "p1stream_priv.h"

#include <unistd.h>
#include <sys/poll.h>

typedef enum _P1Action P1Action;

static bool p1_init(P1ContextFull *ctxf);
static void p1_destroy(P1ContextFull *ctxf);
static void p1_close_pipe(P1Object *ctxobj, int fd);
static void p1_log_default(P1Object *obj, P1LogLevel level, const char *fmt, va_list args, void *user_data);
static void *p1_ctrl_main(void *data);
static bool p1_ctrl_progress(P1Context *ctx);
static P1Action p1_ctrl_determine_action(P1Object *obj, P1TargetState target, bool can_interrupt);
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
    pthread_mutexattr_t attr;
    int ret;

    obj->type = type;

    ret = pthread_mutexattr_init(&attr);
    if (ret != 0) {
        p1_log(obj, P1_LOG_ERROR, "Failed to initialize mutex attributes: %s", strerror(ret));
        goto fail;
    }

    ret = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    if (ret != 0) {
        p1_log(obj, P1_LOG_ERROR, "Failed to set mutex attributes: %s", strerror(ret));
        goto fail_attr;
    }

    ret = pthread_mutex_init(&obj->lock, &attr);
    if (ret != 0) {
        p1_log(obj, P1_LOG_ERROR, "Failed to initialize mutex: %s", strerror(ret));
        goto fail_attr;
    }

    ret = pthread_mutexattr_destroy(&attr);
    if (ret != 0)
        p1_log(obj, P1_LOG_ERROR, "Failed to destroy mutex attributes: %s", strerror(ret));

    return true;

fail_attr:
    ret = pthread_mutexattr_destroy(&attr);
    if (ret != 0)
        p1_log(obj, P1_LOG_ERROR, "Failed to destroy mutex attributes: %s", strerror(ret));

fail:
    return false;
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

P1Context *p1_create()
{
    P1Context *ctx = calloc(1,
        sizeof(P1ContextFull) + sizeof(P1VideoFull) +
        sizeof(P1AudioFull) + sizeof(P1ConnectionFull));

    P1ContextFull *ctxf = (P1ContextFull *) ctx;
    P1VideoFull *videof = (P1VideoFull *) (ctxf + 1);
    P1AudioFull *audiof = (P1AudioFull *) (videof + 1);
    P1ConnectionFull *connf = (P1ConnectionFull *) (audiof + 1);

    if (!ctx)
        goto fail_alloc;

    if (!p1_init(ctxf))
        goto fail_context;

    if (!p1_video_init(videof))
        goto fail_video;
    ctx->video = (P1Video *) videof;
    ((P1Object *) videof)->ctx = ctx;

    if (!p1_audio_init(audiof))
        goto fail_audio;
    ctx->audio = (P1Audio *) audiof;
    ((P1Object *) audiof)->ctx = ctx;

    if (!p1_conn_init(connf))
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

static bool p1_init(P1ContextFull *ctxf)
{
    P1Context *ctx = (P1Context *) ctxf;
    P1Object *ctxobj = (P1Object *) ctxf;
    int ret;

    if (!p1_object_init(ctxobj, P1_OTYPE_CONTEXT))
        goto fail_object;

    if (!p1_init_platform(ctxf))
        goto fail_platform;

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
    p1_destroy_platform(ctxf);

fail_platform:
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

    p1_destroy_platform(ctxf);

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
        while ((node = head->next) != head) {
            P1Source *src = p1_list_get_container(node, P1Source, link);
            p1_list_remove(node);
            p1_plugin_free((P1Plugin *) src);
        }
    }

    if (options & P1_FREE_AUDIO_SOURCES) {
        head = &ctx->audio->sources;
        while ((node = head->next) != head) {
            P1Source *src = p1_list_get_container(node, P1Source, link);
            p1_list_remove(node);
            p1_plugin_free((P1Plugin *) src);
        }
    }

    p1_conn_destroy((P1ConnectionFull *) ctx->conn);
    p1_audio_destroy((P1AudioFull *) ctx->audio);
    p1_video_destroy((P1VideoFull *) ctx->video);
    p1_destroy(ctxf);

    free(ctxf);
}

void p1_config(P1Context *ctx, P1Config *cfg)
{
    p1_audio_config((P1AudioFull *) ctx->audio, cfg);
    p1_video_config((P1VideoFull *) ctx->video, cfg);
    p1_conn_config((P1ConnectionFull *) ctx->conn, cfg);
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

    ctxobj->state.target = P1_TARGET_RUNNING;

    // Bootstrap.
    if (ctxobj->state.current == P1_STATE_IDLE) {
        int ret = pthread_create(&ctxf->ctrl_thread, NULL, p1_ctrl_main, ctx);
        if (ret != 0) {
            p1_log(ctxobj, P1_LOG_ERROR, "Failed to start control thread: %s", strerror(ret));
            result = false;
        }
        else {
            ctxobj->state.current = P1_STATE_STARTING;
        }
    }

    p1_object_notify(ctxobj);

    p1_object_unlock(ctxobj);

    return result;
}

void p1_stop(P1Context *ctx, P1StopOptions options)
{
    P1Object *ctxobj = (P1Object *) ctx;
    P1ContextFull *ctxf = (P1ContextFull *) ctx;
    bool is_idle;

    p1_object_lock(ctxobj);

    ctxobj->state.target = P1_TARGET_IDLE;
    is_idle = (ctxobj->state.current == P1_STATE_IDLE);

    // Set to stopping immediately, if possible.
    if (ctxobj->state.current == P1_STATE_RUNNING)
        ctxobj->state.current = P1_STATE_STOPPING;

    p1_object_notify(ctxobj);

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

        out->object = NULL;
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
    void *user_data = NULL;

    if (ctx) {
        if (level > ctx->log_level)
            return;
        if (ctx->log_fn) {
            fn = ctx->log_fn;
            user_data = ctx->log_user_data;
        }
    }

    fn(obj, level, fmt, args, user_data);
}

// Default log function.
static void p1_log_default(P1Object *obj, P1LogLevel level, const char *fmt, va_list args, void *user_data)
{
    char prefix;
    switch (level) {
        case P1_LOG_ERROR:   prefix = 'E'; break;
        case P1_LOG_WARNING: prefix = 'W'; break;
        case P1_LOG_INFO:    prefix = 'I'; break;
        case P1_LOG_DEBUG:   prefix = 'D'; break;
        default:             prefix = 'X'; break;
    }

    char *name, name_buf[32];
    switch (obj->type) {
        case P1_OTYPE_CONTEXT:
            name = "context";
            break;
        case P1_OTYPE_VIDEO:
            name = "video";
            break;
        case P1_OTYPE_AUDIO:
            name = "audio";
            break;
        case P1_OTYPE_CONNECTION:
            name = "conn";
            break;
        case P1_OTYPE_VIDEO_CLOCK:
            name = "vclock";
            break;
        case P1_OTYPE_VIDEO_SOURCE:
            name = name_buf;
            snprintf(name_buf, sizeof(name_buf), "vsrc %p", obj);
            break;
        case P1_OTYPE_AUDIO_SOURCE:
            name = name_buf;
            snprintf(name_buf, sizeof(name_buf), "asrc %p", obj);
            break;
    }

    char out[2048];
    int ret;

    char *p_out = out;
    size_t buf_size = sizeof(out);
    size_t length;

    ret = snprintf(p_out, buf_size, "%c [%s]: ", prefix, name);
    if (ret < 0)
        return;

    p_out += ret;
    length = ret;

    ret = vsnprintf(p_out, buf_size - ret, fmt, args);
    if (ret < 0)
        return;

    p_out += ret;
    length += ret;

    *p_out = '\n';
    fwrite(out, length + 1, 1, stderr);
}

// The control thread main loop.
static void *p1_ctrl_main(void *data)
{
    P1Context *ctx = (P1Context *) data;
    P1Object *ctxobj = (P1Object *) data;
    P1ContextFull *ctxf = (P1ContextFull *) ctx;
    bool wait;

    p1_object_lock(ctxobj);

    ctxobj->state.current = P1_STATE_RUNNING;
    p1_object_notify(ctxobj);

    while (true) {
        // Deal with the notification channels. Don't hold the lock during this.
        p1_object_unlock(ctxobj);
        p1_ctrl_comm(ctxf);
        p1_object_lock(ctxobj);

        // Go into stopping state if that's our goal.
        if (ctxobj->state.target != P1_TARGET_RUNNING && ctxobj->state.current == P1_STATE_RUNNING) {
            ctxobj->state.current = P1_STATE_STOPPING;
            p1_object_notify(ctxobj);
        }

        // Progress state of our objects.
        wait = p1_ctrl_progress(ctx);

        // If there's nothing left to wait on, and we're stopping.
        if (!wait && ctxobj->state.current == P1_STATE_STOPPING) {
            // Restart if our target is no longer to idle.
            if (ctxobj->state.target == P1_TARGET_RUNNING) {
                ctxobj->state.current = P1_STATE_RUNNING;
                p1_object_notify(ctxobj);
            }
            // Otherwise, we're done.
            else {
                break;
            }
        }
    };

    ctxobj->state.current = P1_STATE_IDLE;
    p1_object_notify(ctxobj);

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

// Before an action, check special targets.
#define P1_CHECK_TARGET(_obj)                               \
    if ((_obj)->state.target == P1_TARGET_RESTART &&        \
        (_obj)->state.current == P1_STATE_IDLE) {           \
        (_obj)->state.target = P1_TARGET_RUNNING;           \
        p1_object_notify(_obj);                             \
    }

// After an action, check if we need to wait.
#define P1_CHECK_WAIT(_obj)                                 \
    if ((_obj)->state.current == P1_STATE_STARTING ||       \
        (_obj)->state.current == P1_STATE_STOPPING)         \
        wait = true;

// Common action handling for fixed elements.
#define P1_RUN_ACTION(_action, _obj, _full, _start, _stop)  \
    switch (action) {                                       \
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
        break;                                              \
    default:                                                \
        break;                                              \
    }

    fixed = (P1Object *) video;
    p1_object_lock(fixed);

    // Progress clock.
    P1CurrentState vclock_state;
    {
        P1VideoClock *vclock = ctx->video->clock;
        P1Plugin *pel = (P1Plugin *) vclock;
        P1Object *obj = (P1Object *) vclock;

        p1_object_lock(obj);
        obj->ctx = ctx;

        // Clock target state is tied to the context.
        obj->state.target = (ctxobj->state.current == P1_STATE_STOPPING)
                            ? P1_TARGET_IDLE : P1_TARGET_RUNNING;

        P1Action action = p1_ctrl_determine_action(obj, obj->state.target, false);
        P1_RUN_ACTION(action, obj, pel, pel->start, pel->stop);

        vclock_state = obj->state.current;

        p1_object_unlock(obj);
    }

    // Progress video mixer.
    P1CurrentState video_state;
    {
        P1_CHECK_TARGET(fixed);

        P1Action action = p1_ctrl_determine_action(fixed, fixed->state.target, false);
        P1_RUN_ACTION(action, fixed, videof, p1_video_start, p1_video_stop);

        video_state = fixed->state.current;
    }

    // Progress video sources.
    head = &video->sources;
    p1_list_iterate(head, node) {
        P1Source *src = p1_list_get_container(node, P1Source, link);
        P1VideoSource *vsrc = (P1VideoSource *) src;
        P1Plugin *pel = (P1Plugin *) src;
        P1Object *obj = (P1Object *) src;

        p1_object_lock(obj);
        obj->ctx = ctx;

        P1_CHECK_TARGET(obj);

        P1Action action = p1_ctrl_determine_action(obj, obj->state.target, false);

        if (action == P1_ACTION_START) {
            if (video_state == P1_STATE_RUNNING) {
                if (!p1_video_link_source(vsrc)) {
                    obj->state.flags |= P1_FLAG_ERROR;
                    p1_object_notify(obj);
                    action = P1_ACTION_NONE;
                }
            }
        }

        P1_RUN_ACTION(action, obj, pel, pel->start, pel->stop);

        if (action == P1_ACTION_STOP) {
            if (video_state == P1_STATE_RUNNING)
                p1_video_unlink_source(vsrc);
        }

        p1_object_unlock(obj);

        if (action == P1_ACTION_REMOVE) {
            node = node->prev;
            p1_list_remove(&src->link);
            p1_plugin_free(pel);
        }
    }

    p1_object_unlock(fixed);

    fixed = (P1Object *) audio;
    p1_object_lock(fixed);

    // Progress audio mixer.
    P1CurrentState audio_state;
    {
        P1_CHECK_TARGET(fixed);

        P1Action action = p1_ctrl_determine_action(fixed, fixed->state.target, false);
        P1_RUN_ACTION(action, fixed, audiof, p1_audio_start, p1_audio_stop);

        audio_state = fixed->state.current;
    }

    // Progress audio sources.
    head = &audio->sources;
    p1_list_iterate(head, node) {
        P1Source *src = p1_list_get_container(node, P1Source, link);
        P1Plugin *pel = (P1Plugin *) src;
        P1Object *obj = (P1Object *) src;

        p1_object_lock(obj);
        obj->ctx = ctx;

        P1_CHECK_TARGET(obj);

        P1Action action = p1_ctrl_determine_action(obj, obj->state.target, false);
        P1_RUN_ACTION(action, obj, pel, pel->start, pel->stop);

        p1_object_unlock(obj);

        if (action == P1_ACTION_REMOVE) {
            node = node->prev;
            p1_list_remove(&src->link);
            p1_plugin_free(pel);
        }
    }

    p1_object_unlock(fixed);

    fixed = (P1Object *) conn;
    p1_object_lock(fixed);

    // Progress stream connnection.
    {
        P1_CHECK_TARGET(fixed);

        // We need a running clock for this.
        P1TargetState target = fixed->state.target;
        if (vclock_state != P1_STATE_RUNNING)
            target = P1_TARGET_IDLE;

        P1Action action = p1_ctrl_determine_action(fixed, target, true);

        // Delay start until everything else is running.
        if (action == P1_ACTION_START && wait)
            action = P1_ACTION_NONE;

        P1_RUN_ACTION(action, fixed, connf, p1_conn_start, p1_conn_stop);
    }

    p1_object_unlock(fixed);

    return wait;

#undef P1_CHECK_WAIT
#undef P1_RUN_ACTION
}

// Determine action to take on an object.
static P1Action p1_ctrl_determine_action(P1Object *obj, P1TargetState target, bool can_interrupt)
{
    P1CurrentState state = obj->state.current;
    P1Context *ctx = obj->ctx;
    P1Object *ctxobj = (P1Object *) ctx;

    // We need to wait on the transition to finish.
    if (state == P1_STATE_STOPPING)
        return P1_ACTION_WAIT;

    // If the context is stopping, override target.
    // Make sure we preserve special targets.
    if (target == P1_TARGET_RUNNING && ctxobj->state.current == P1_STATE_STOPPING)
        target = P1_TARGET_IDLE;

    // Take steps towards target.
    if (target == P1_TARGET_RUNNING) {
        if (state == P1_STATE_IDLE && !(obj->state.flags & P1_FLAG_ERROR))
            return P1_ACTION_START;

        if (state == P1_STATE_STARTING)
            return P1_ACTION_WAIT;

        return P1_ACTION_NONE;
    }
    else {
        if (state == P1_STATE_RUNNING)
            return P1_ACTION_STOP;

        if (state == P1_STATE_STARTING) {
            if (can_interrupt)
                return P1_ACTION_STOP;
            else
                return P1_ACTION_WAIT;
        }

        if (target == P1_TARGET_REMOVE && state == P1_TARGET_IDLE)
            return P1_ACTION_REMOVE;

        return P1_ACTION_NONE;
    }
}

// Log a notification.
static void p1_ctrl_log_notification(P1Notification *n)
{
    P1Object *obj = n->object;

    if (n->state.target != n->last_state.target) {
        const char *target;
        switch (n->state.target) {
            case P1_TARGET_RUNNING: target = "running"; break;
            case P1_TARGET_IDLE:    target = "idle";    break;
            case P1_TARGET_RESTART: target = "restart"; break;
            case P1_TARGET_REMOVE:  target = "remove";  break;
            default: return;
        }
        p1_log(obj, P1_LOG_INFO, "target -> %s", target);
    }

    if (n->state.current != n->last_state.current) {
        const char *state;
        switch (n->state.current) {
            case P1_STATE_IDLE:     state = "idle";     break;
            case P1_STATE_STARTING: state = "starting"; break;
            case P1_STATE_RUNNING:  state = "running";  break;
            case P1_STATE_STOPPING: state = "stopping"; break;
            default: return;
        }
        p1_log(obj, P1_LOG_INFO, "state -> %s", state);
    }
}
