#include "p1stream_priv.h"

#include <unistd.h>
#include <sys/poll.h>

static void p1_log_default(P1Context *ctx, P1LogLevel level, const char *fmt, va_list args, void *user_data);
static void *p1_ctrl_main(void *data);
static void p1_ctrl_progress(P1Context *ctx);
static void p1_ctrl_progress_source(P1Context *ctx, P1Source *src);
static void p1_ctrl_comm(P1ContextFull *ctxf);
static void p1_ctrl_log_notification(P1Context *ctx, P1Notification *notification);


P1Context *p1_create(P1Config *cfg, P1ConfigSection *sect)
{
    P1ConfigSection *video_sect = cfg->get_section(cfg, sect, "video");
    P1ConfigSection *audio_sect = cfg->get_section(cfg, sect, "audio");
    P1ConfigSection *stream_sect = cfg->get_section(cfg, sect, "stream");

    P1ContextFull *ctxf = calloc(1,
        sizeof(P1ContextFull) + sizeof(P1VideoFull) +
        sizeof(P1AudioFull) + sizeof(P1ConnectionFull));
    P1VideoFull *videof = (P1VideoFull *) (ctxf + 1);
    P1AudioFull *audiof = (P1AudioFull *) (videof + 1);
    P1ConnectionFull *connf = (P1ConnectionFull *) (audiof + 1);

    P1Context *ctx = (P1Context *) ctxf;
    P1Video *video = (P1Video *) videof;
    P1Audio *audio = (P1Audio *) audiof;
    P1Connection *conn = (P1Connection *) connf;

    ctx->video = video;
    ctx->audio = audio;
    ctx->conn = conn;

    video->ctx = ctx;
    audio->ctx = ctx;
    conn->ctx = ctx;

    ctx->log_level = P1_LOG_INFO;
    ctx->log_fn = p1_log_default;

    int ret;

    ret = pipe(ctxf->ctrl_pipe);
    assert(ret == 0);
    ret = pipe(ctxf->user_pipe);
    assert(ret == 0);

    mach_timebase_info(&ctxf->timebase);

    p1_video_init(videof, cfg, video_sect);
    p1_audio_init(audiof, cfg, audio_sect);
    p1_conn_init(connf, cfg, stream_sect);

    return ctx;
}

void p1_free(P1Context *ctx, P1FreeOptions options)
{
    // FIXME
}

void p1_start(P1Context *_ctx)
{
    P1ContextFull *ctx = (P1ContextFull *) _ctx;

    if (_ctx->state != P1_STATE_IDLE)
        return;

    p1_set_state(_ctx, P1_OTYPE_CONTEXT, _ctx, P1_STATE_STARTING);

    int ret = pthread_create(&ctx->ctrl_thread, NULL, p1_ctrl_main, ctx);
    assert(ret == 0);
}

void p1_stop(P1Context *_ctx)
{
    P1ContextFull *ctx = (P1ContextFull *) _ctx;

    if (_ctx->state != P1_STATE_RUNNING)
        return;

    // FIXME: Lock for this? Especially if the context thread can eventually
    // stop itself for whatever reason.
    p1_set_state(_ctx, P1_OTYPE_CONTEXT, _ctx, P1_STATE_STOPPING);

    int ret = pthread_join(ctx->ctrl_thread, NULL);
    assert(ret == 0);

    // FIXME: stop streaming.
}

void p1_read(P1Context *_ctx, P1Notification *out)
{
    P1ContextFull *ctx = (P1ContextFull *) _ctx;

    ssize_t size = sizeof(P1Notification);
    ssize_t ret = read(ctx->user_pipe[0], out, size);
    assert(ret == size);
}

int p1_fd(P1Context *_ctx)
{
    P1ContextFull *ctx = (P1ContextFull *) _ctx;

    return ctx->user_pipe[0];
}

void _p1_notify(P1Context *_ctx, P1Notification notification)
{
    P1ContextFull *ctx = (P1ContextFull *) _ctx;

    ssize_t size = sizeof(P1Notification);
    ssize_t ret = write(ctx->ctrl_pipe[1], &notification, size);
    assert(ret == size);
}

void p1_log(P1Context *ctx, P1LogLevel level, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    p1_logv(ctx, level, fmt, args);
    va_end(args);
}

void p1_logv(P1Context *ctx, P1LogLevel level, const char *fmt, va_list args)
{
    if (level <= ctx->log_level)
        ctx->log_fn(ctx, level, fmt, args, ctx->log_user_data);
}

static void p1_log_default(P1Context *ctx, P1LogLevel level, const char *fmt, va_list args, void *user_data)
{
    const char *pre;
    switch (level) {
        case P1_LOG_ERROR:      pre = "error";      break;
        case P1_LOG_WARNING:    pre = "warning";    break;
        case P1_LOG_INFO:       pre = "info";       break;
        case P1_LOG_DEBUG:      pre = "debug";      break;
        default: pre = "error"; break;
    }
    fprintf(stderr, "[%s] ", pre);
    vfprintf(stderr, fmt, args);
}

static void *p1_ctrl_main(void *data)
{
    P1Context *ctx = (P1Context *) data;
    P1ContextFull *ctxf = (P1ContextFull *) ctx;

    p1_set_state(ctx, P1_OTYPE_CONTEXT, ctx, P1_STATE_RUNNING);

    do {
        p1_ctrl_comm(ctxf);
        p1_ctrl_progress(ctx);

        // FIXME: handle stop
    } while (true);

    p1_set_state(ctx, P1_OTYPE_CONTEXT, ctx, P1_STATE_IDLE);
    p1_ctrl_comm(ctxf);

    return NULL;
}

static void p1_ctrl_comm(P1ContextFull *ctxf)
{
    P1Context *ctx = (P1Context *) ctxf;
    int i_ret;
    struct pollfd fd = {
        .fd = ctxf->ctrl_pipe[0],
        .events = POLLIN
    };

    P1Notification notification;
    ssize_t size = sizeof(P1Notification);
    ssize_t s_ret;

    // Wait indefinitely for the next notification.
    do {
        i_ret = poll(&fd, 1, -1);
        assert(i_ret != -1);
    } while (i_ret == 0);

    do {
        // Read the notification.
        s_ret = read(fd.fd, &notification, size);
        assert(s_ret == size);

        // Log notification.
        p1_ctrl_log_notification(ctx, &notification);

        // Pass it on to the user.
        s_ret = write(ctxf->user_pipe[1], &notification, size);
        assert(s_ret == size);

        // Flush other notifications.
        i_ret = poll(&fd, 1, 0);
        assert(i_ret != -1);
    } while (i_ret == 1);
}

static void p1_ctrl_progress(P1Context *ctx)
{
    P1Video *video = ctx->video;
    P1VideoFull *videof = (P1VideoFull *) video;
    P1Audio *audio = ctx->audio;
    P1AudioFull *audiof = (P1AudioFull *) audio;
    P1Connection *conn = ctx->conn;
    P1ConnectionFull *connf = (P1ConnectionFull *) conn;
    P1ListNode *head;
    P1ListNode *node;

    // Progress clock.
    P1VideoClock *clock = ctx->video->clock;
    if (clock->state == P1_STATE_IDLE) {
        clock->ctx = ctx;
        clock->start(clock);
    }
    // Clock must be running before we can make anything else happen.
    if (clock->state != P1_STATE_RUNNING)
        return;

    pthread_mutex_lock(&video->lock);
    // Progress video sources.
    head = &video->sources;
    p1_list_iterate(head, node) {
        P1Source *src = (P1Source *) node;
        p1_ctrl_progress_source(ctx, src);
    }
    pthread_mutex_unlock(&video->lock);

    pthread_mutex_lock(&audio->lock);
    // Progress audio sources.
    head = &audio->sources;
    p1_list_iterate(head, node) {
        P1Source *src = (P1Source *) node;
        p1_ctrl_progress_source(ctx, src);
    }
    pthread_mutex_unlock(&audio->lock);

    // Progress internal components.
    if (audio->target == P1_TARGET_RUNNING && audio->state == P1_STATE_IDLE)
        p1_audio_start(audiof);
    else if (audio->target != P1_TARGET_RUNNING && audio->state == P1_STATE_RUNNING)
        p1_audio_stop(audiof);

    if (video->target == P1_TARGET_RUNNING && video->state == P1_STATE_IDLE)
        p1_video_start(videof);
    else if (video->target != P1_TARGET_RUNNING && video->state == P1_STATE_RUNNING)
        p1_video_stop(videof);

    // FIXME: We may want to delay until sources are running.
    if (conn->target == P1_TARGET_RUNNING && conn->state == P1_STATE_IDLE)
        p1_conn_start(connf);
    else if (conn->target != P1_TARGET_RUNNING && conn->state == P1_STATE_RUNNING)
        p1_conn_stop(connf);
}

static void p1_ctrl_progress_source(P1Context *ctx, P1Source *src)
{
    if (src->target == P1_TARGET_RUNNING) {
        if (src->state == P1_STATE_IDLE) {
            src->ctx = ctx;
            src->start(src);
        }
    }
    else {
        if (src->state == P1_STATE_RUNNING) {
            src->stop(src);
            src->ctx = NULL;
        }
        if (src->target == P1_TARGET_REMOVE && src->state == P1_TARGET_IDLE) {
            p1_list_remove(src);
            src->free(src);
        }
    }
}

static void p1_ctrl_log_notification(P1Context *ctx, P1Notification *notification)
{
    const char *action;
    switch (notification->type) {
        case P1_NTYPE_STATE_CHANGE:
            switch (notification->state_change.state) {
                case P1_STATE_IDLE:     action = "is idle";     break;
                case P1_STATE_STARTING: action = "is starting"; break;
                case P1_STATE_RUNNING:  action = "is running";  break;
                case P1_STATE_STOPPING: action = "is stopping"; break;
                default: return;
            }
            break;
        case P1_NTYPE_TARGET_CHANGE:
            switch (notification->target_change.target) {
                case P1_TARGET_RUNNING: action = "target running";  break;
                case P1_TARGET_IDLE:    action = "target idle";     break;
                case P1_TARGET_REMOVE:  action = "target remove";   break;
                default: return;
            }
            break;
        default: return;
    }

    const char *obj_descr;
    switch (notification->object_type) {
        case P1_OTYPE_CONTEXT:      obj_descr = "context";      break;
        case P1_OTYPE_VIDEO:        obj_descr = "video mixer";  break;
        case P1_OTYPE_AUDIO:        obj_descr = "audio mixer";  break;
        case P1_OTYPE_CONNECTION:   obj_descr = "connection";   break;
        case P1_OTYPE_VIDEO_CLOCK:  obj_descr = "video clock";  break;
        case P1_OTYPE_VIDEO_SOURCE: obj_descr = "video source"; break;
        case P1_OTYPE_AUDIO_SOURCE: obj_descr = "audio source"; break;
        default: return;
    }

    p1_log(ctx, P1_LOG_INFO, "%s %p %s\n", obj_descr, notification->object, action);
}
