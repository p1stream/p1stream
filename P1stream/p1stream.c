#include <unistd.h>
#include <sys/poll.h>

#include "p1stream_priv.h"

static void *p1_ctrl_main(void *data);
static void p1_ctrl_progress(P1ContextFull *ctx);
static void p1_ctrl_progress_clock(P1Context *ctx, P1VideoClock *clock);
static void p1_ctrl_progress_source(P1Context *ctx, P1Source *src);
static void p1_ctrl_comm(P1ContextFull *ctx);


P1Context *p1_create(P1Config *cfg, P1ConfigSection *sect)
{
    P1ContextFull *ctx = calloc(1, sizeof(P1ContextFull));
    P1Context *_ctx = (P1Context *) ctx;

    int ret;

    ret = pthread_mutex_init(&_ctx->lock, NULL);
    assert(ret == 0);

    ret = pipe(ctx->ctrl_pipe);
    assert(ret == 0);
    ret = pipe(ctx->user_pipe);
    assert(ret == 0);

    mach_timebase_info(&ctx->timebase);

    P1ConfigSection *audio_sect = cfg->get_section(cfg, sect, "audio");
    p1_audio_init(ctx, cfg, audio_sect);

    P1ConfigSection *video_sect = cfg->get_section(cfg, sect, "video");
    p1_video_init(ctx, cfg, video_sect);

    P1ConfigSection *stream_sect = cfg->get_section(cfg, sect, "stream");
    p1_stream_init(ctx, cfg, stream_sect);

    return _ctx;
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

static void *p1_ctrl_main(void *data)
{
    P1Context *_ctx = (P1Context *) data;
    P1ContextFull *ctx = (P1ContextFull *) data;

    p1_set_state(_ctx, P1_OTYPE_CONTEXT, _ctx, P1_STATE_RUNNING);

    do {
        p1_ctrl_comm(ctx);
        p1_ctrl_progress(ctx);

        // FIXME: handle stop
    } while (true);

    p1_set_state(_ctx, P1_OTYPE_CONTEXT, _ctx, P1_STATE_IDLE);
    p1_ctrl_comm(ctx);

    return NULL;
}

static void p1_ctrl_comm(P1ContextFull *ctx)
{
    int i_ret;
    struct pollfd fd = {
        .fd = ctx->ctrl_pipe[0],
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

        // Pass it on to the user.
        s_ret = write(ctx->user_pipe[1], &notification, size);
        assert(s_ret == size);

        // Flush other notifications.
        i_ret = poll(&fd, 1, 0);
        assert(i_ret != -1);
    } while (i_ret == 1);
}

static void p1_ctrl_progress(P1ContextFull *ctx)
{
    P1Context *_ctx = (P1Context *) ctx;

    P1ListNode *head;
    P1ListNode *node;

    pthread_mutex_lock(&_ctx->lock);

    // Progress video clock.
    p1_ctrl_progress_clock(_ctx, _ctx->clock);

    // Progress video sources.
    head = &_ctx->video_sources;
    p1_list_iterate(head, node) {
        P1Source *src = (P1Source *) node;
        p1_ctrl_progress_source(_ctx, src);
    }

    // Progress audio sources.
    head = &_ctx->audio_sources;
    p1_list_iterate(head, node) {
        P1Source *src = (P1Source *) node;
        p1_ctrl_progress_source(_ctx, src);
    }

    pthread_mutex_unlock(&_ctx->lock);
}

static void p1_ctrl_progress_clock(P1Context *ctx, P1VideoClock *clock)
{
    if (clock->state == P1_STATE_IDLE) {
        clock->ctx = ctx;
        clock->start(clock);
    }
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
        }
        if (src->target == P1_TARGET_REMOVE && src->state == P1_TARGET_IDLE) {
            p1_list_remove(src);
            src->free(src);
        }
    }
}
