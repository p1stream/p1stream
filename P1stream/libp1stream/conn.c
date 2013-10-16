#include "p1stream_priv.h"

#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

static const char *default_url = "rtmp://localhost/app/test";

// This is used for RTMP logging.
static P1Object *current_conn = NULL;

// Hardcoded audio parameters.
static const int audio_sample_rate = 44100;
static const int audio_num_channels = 2;
// Hardcoded bitrate.
static const int audio_bit_rate = 128 * 1024;
// Minimum output buffer size per FDK AAC requirements.
static const int audio_out_min_size = 6144 / 8 * audio_num_channels;
// Complete output buffer size, roughly two seconds.
static const int audio_out_size = audio_out_min_size * 128;

static bool p1_conn_parse_x264_param(P1Config *cfg, const char *key, const char *val, void *data);

static bool p1_conn_stream_video_config(P1ConnectionFull *connf);
static bool p1_conn_stream_audio_config(P1ConnectionFull *connf);

static RTMPPacket *p1_conn_new_packet(P1ConnectionFull *connf, uint8_t type, uint32_t body_size);
static bool p1_conn_submit_packet(P1ConnectionFull *connf, RTMPPacket *pkt, int64_t time);

static void *p1_conn_main(void *data);
static bool p1_conn_flush(P1ConnectionFull *connf);

static bool p1_conn_start_audio(P1ConnectionFull *connf);
static void p1_conn_stop_audio(P1ConnectionFull *connf);

static bool p1_conn_start_video(P1ConnectionFull *connf);
static void p1_conn_stop_video(P1ConnectionFull *connf);

static void p1_conn_signal(P1ConnectionFull *connf);
static void p1_conn_clear(P1PacketQueue *q);

static void p1_conn_x264_log_callback(void *data, int level, const char *fmt, va_list args);
static void p1_conn_rtmp_log_callback(int level, const char *fmt, va_list);


bool p1_conn_init(P1ConnectionFull *connf, P1Context *ctx)
{
    P1Object *connobj = (P1Object *) connf;
    int ret;

    if (!p1_object_init(connobj, P1_OTYPE_CONNECTION, ctx))
        goto fail_object;

    ret = pthread_cond_init(&connf->cond, NULL);
    if (ret != 0) {
        p1_log(connobj, P1_LOG_ERROR, "Failed to initialize condition variable: %s", strerror(ret));
        goto fail_cond;
    }

    ret = pthread_mutex_init(&connf->audio_lock, NULL);
    if (ret != 0) {
        p1_log(connobj, P1_LOG_ERROR, "Failed to initialize mutex: %s", strerror(ret));
        goto fail_audio_lock;
    }

    ret = pthread_mutex_init(&connf->video_lock, NULL);
    if (ret != 0) {
        p1_log(connobj, P1_LOG_ERROR, "Failed to initialize mutex: %s", strerror(ret));
        goto fail_video_lock;
    }

    return true;

fail_params:
    ret = pthread_mutex_destroy(&connf->video_lock);
    if (ret != 0)
        p1_log(connobj, P1_LOG_ERROR, "Failed to destroy mutex: %s", strerror(ret));

fail_video_lock:
    ret = pthread_mutex_destroy(&connf->audio_lock);
    if (ret != 0)
        p1_log(connobj, P1_LOG_ERROR, "Failed to destroy mutex: %s", strerror(ret));

fail_audio_lock:
    ret = pthread_cond_destroy(&connf->cond);
    if (ret != 0)
        p1_log(connobj, P1_LOG_ERROR, "Failed to destroy condition variable: %s", strerror(ret));

fail_cond:
    p1_object_destroy(connobj);

fail_object:
    return false;
}

void p1_conn_destroy(P1ConnectionFull *connf)
{
    P1Object *connobj = (P1Object *) connf;
    int ret;

    ret = pthread_mutex_destroy(&connf->audio_lock);
    if (ret != 0)
        p1_log(connobj, P1_LOG_ERROR, "Failed to destroy mutex: %s", strerror(ret));

    ret = pthread_mutex_destroy(&connf->video_lock);
    if (ret != 0)
        p1_log(connobj, P1_LOG_ERROR, "Failed to destroy mutex: %s", strerror(ret));

    ret = pthread_cond_destroy(&connf->cond);
    if (ret != 0)
        p1_log(connobj, P1_LOG_ERROR, "Failed to destroy condition variable: %s", strerror(ret));

    p1_object_destroy(connobj);
}

void p1_conn_config(P1ConnectionFull *connf, P1Config *cfg)
{
    P1Object *connobj = (P1Object *) connf;
    x264_param_t *vp = &connf->video_params;
    char s_tmp[128];
    int i_tmp;
    float f_tmp;

    p1_object_reset_config_flags(connobj);

    x264_param_default(&connf->video_params);

    if (!cfg->get_string(cfg, "url", connf->url, sizeof(connf->url)))
        strcpy(connf->url, default_url);

    // x264 already logs errors, except for x264_param_parse.

    // Apply presets.
    if (cfg->get_string(cfg, "x264-preset", s_tmp, sizeof(s_tmp)))
        x264_param_default_preset(vp, s_tmp, NULL);
    if (cfg->get_string(cfg, "x264-tune", s_tmp, sizeof(s_tmp)))
        x264_param_default_preset(vp, NULL, s_tmp);

    // For convenience, we provide some short-hands.
    if (cfg->get_int(cfg, "x264-bitrate", &i_tmp)) {
        vp->rc.i_rc_method = X264_RC_ABR;
        vp->rc.i_bitrate = i_tmp;
        vp->rc.i_vbv_max_bitrate = i_tmp;
        vp->rc.i_vbv_buffer_size = i_tmp;
    }
    if (cfg->get_float(cfg, "x264-keyint-sec", &f_tmp))
        connf->keyint_sec = f_tmp;
    else
        connf->keyint_sec = 0;

    // Specific user overrides.
    cfg->each_string(cfg, "x264-x-", p1_conn_parse_x264_param, connf);

    // Apply presets.
    x264_param_apply_fastfirstpass(vp);
    if (cfg->get_string(cfg, "x264-profile", s_tmp, sizeof(s_tmp)))
        x264_param_apply_profile(vp, s_tmp);

    p1_object_notify(connobj);
}

static bool p1_conn_parse_x264_param(P1Config *cfg, const char *key, const char *val, void *data)
{
    P1Object *connobj = (P1Object *) data;
    P1ConnectionFull *connf = (P1ConnectionFull *) data;
    int ret;

    ret = x264_param_parse(&connf->video_params, key + 7, val);
    if (ret != 0) {
        if (ret == X264_PARAM_BAD_NAME)
            p1_log(connobj, P1_LOG_ERROR, "Invalid x264 parameter name '%s'", key);
        else if (ret == X264_PARAM_BAD_VALUE)
            p1_log(connobj, P1_LOG_ERROR, "Invalid value for x264 parameter '%s'", key);
    }

    return true;
}

void p1_conn_notify(P1ConnectionFull *connf, P1Notification *n)
{
    P1Object *connobj = (P1Object *) connf;
    P1Context *ctx = connobj->ctx;
    P1Object *audioobj = (P1Object *) ctx->audio;
    P1Object *videoobj = (P1Object *) ctx->video;
    P1Object *vclockobj = (P1Object *) ctx->video->clock;

    p1_object_reset_notify_flags(connobj);

    if (audioobj->state.current  != P1_STATE_RUNNING ||
        videoobj->state.current  != P1_STATE_RUNNING ||
        vclockobj->state.current != P1_STATE_RUNNING) {
        p1_object_clear_flag(connobj, P1_FLAG_CAN_START);

        if (connobj->state.current == P1_STATE_STARTING ||
            connobj->state.current == P1_STATE_RUNNING)
            p1_conn_stop(connf);
    }

    p1_object_notify(connobj);
}

void p1_conn_start(P1ConnectionFull *connf)
{
    P1Object *connobj = (P1Object *) connf;

    int ret = pthread_create(&connf->thread, NULL, p1_conn_main, connf);
    if (ret != 0) {
        p1_log(connobj, P1_LOG_ERROR, "Failed to start connection thread: %s", strerror(ret));
        connobj->state.current = P1_STATE_IDLE;
        connobj->state.flags |= P1_FLAG_ERROR;
    }
    else {
        // Thread will continue start, and set state to running
        connobj->state.current = P1_STATE_STARTING;
    }

    p1_object_notify(connobj);
}

void p1_conn_stop(P1ConnectionFull *connf)
{
    P1Object *connobj = (P1Object *) connf;

    // If we acquired the lock during the starting state, that means we're in
    // the middle of a connection attempt. Abort it.
    if (connobj->state.current == P1_STATE_STARTING)
        RTMP_Close(&connf->rtmp);

    connobj->state.current = P1_STATE_STOPPING;
    p1_object_notify(connobj);

    p1_conn_signal(connf);
}


// Send video configuration. This happens during the starting state, so we
// don't have to worry about locking.
static bool p1_conn_stream_video_config(P1ConnectionFull *connf)
{
    P1Object *connobj = (P1Object *) connf;
    x264_nal_t *nals;
    int len;
    int ret;
    int i;

    ret = x264_encoder_headers(connf->video_enc, &nals, &len);
    if (ret < 0) {
        p1_log(connobj, P1_LOG_ERROR, "Failed to get H.264 headers");
        return false;
    }

    x264_nal_t *nal_sps = NULL, *nal_pps = NULL;
    for (i = 0; i < len; i++) {
        x264_nal_t * const nal = &nals[i];
        switch (nal->i_type) {
            case NAL_SPS:
                if (!nal_sps)
                    nal_sps = nal;
                break;
            case NAL_PPS:
                if (!nal_pps)
                    nal_pps = nal;
                break;
        }
    }
    if (nal_sps == NULL || nal_pps == NULL) {
        p1_log(connobj, P1_LOG_ERROR, "Failed to build video config packet");
        return false;
    }

    int sps_size = nal_sps->i_payload-4;
    int pps_size = nal_pps->i_payload-4;
    uint32_t tag_size = 16 + sps_size + pps_size;

    RTMPPacket *pkt = p1_conn_new_packet(connf, RTMP_PACKET_TYPE_VIDEO, tag_size);
    if (pkt == NULL)
        return false;
    char * const body = pkt->m_body;

    body[0] = 0x10 | 0x07; // keyframe, AVC
    body[1] = 0; // AVC header
    // skip composition time

    uint8_t * const sps = nal_sps->p_payload + 4;
    body[5] = 1; // version
    body[6] = sps[1]; // profile
    body[7] = sps[2]; // compatibility
    body[8] = sps[3]; // level
    body[9] = 0xfc | (4 - 1); // size of NAL length prefix

    i = 10;

    body[i++] = 0xe0 | 1; // number of SPSs
    *(uint16_t *) (body+i) = htons(sps_size);
    memcpy(body+i+2, nal_sps->p_payload+4, sps_size);

    i += sps_size + 2;

    body[i++] = 1; // number of PPSs
    *(uint16_t *) (body+i) = htons(pps_size);
    memcpy(body+i+2, nal_pps->p_payload+4, pps_size);

    // It's crucial this packet gets queued.
    return p1_conn_submit_packet(connf, pkt, 0);
}

// Encode and send video data
void p1_conn_stream_video(P1ConnectionFull *connf, int64_t time, x264_picture_t *pic)
{
    P1Object *connobj = (P1Object *) connf;

    // Encode and build packet using fine-grained lock.
    p1_lock(connobj, &connf->video_lock);

    if (connobj->state.current != P1_STATE_RUNNING) {
        p1_unlock(connobj, &connf->video_lock);
        return;
    }

    pic->i_dts = time;
    pic->i_pts = time;

    x264_nal_t *nals;
    int len;
    x264_picture_t out_pic;
    int ret = x264_encoder_encode(connf->video_enc, &nals, &len, pic, &out_pic);
    if (ret < 0) {
        p1_log(connobj, P1_LOG_ERROR, "Failed to H.264 encode frame");
        goto fail;
    }

    time = out_pic.i_dts;

    uint32_t size = 0;
    for (int i = 0; i < len; i++)
        size += nals[i].i_payload;
    if (size == 0) {
        p1_unlock(connobj, &connf->video_lock);
        return;
    }

    const uint32_t tag_size = size + 5;
    RTMPPacket *pkt = p1_conn_new_packet(connf, RTMP_PACKET_TYPE_VIDEO, tag_size);
    if (pkt == NULL)
        goto fail;
    char * const body = pkt->m_body;

    body[0] = (out_pic.b_keyframe ? 0x10 : 0x20) | 0x07; // keyframe/IDR, AVC
    body[1] = 1; // AVC NALU
    // skip composition time

    memcpy(body + 5, nals[0].p_payload, size);

    p1_unlock(connobj, &connf->video_lock);

    // Stream using full lock.
    p1_object_lock(connobj);

    if (connobj->state.current == P1_STATE_RUNNING)
        p1_conn_submit_packet(connf, pkt, time);
    else
        free(pkt);

    p1_object_unlock(connobj);

    return;

fail:
    p1_unlock(connobj, &connf->video_lock);

    p1_object_lock(connobj);
    if (connobj->state.current == P1_STATE_RUNNING) {
        connobj->state.current = P1_STATE_STOPPING;
        connobj->state.flags |= P1_FLAG_ERROR;
        p1_object_notify(connobj);
        p1_conn_signal(connf);
    }
    p1_object_unlock(connobj);
}



// Send audio configuration. This happens during the starting state, so we
// don't have to worry about locking.
static bool p1_conn_stream_audio_config(P1ConnectionFull *connf)
{
    const uint32_t tag_size = 2 + 2;

    RTMPPacket *pkt = p1_conn_new_packet(connf, RTMP_PACKET_TYPE_AUDIO, tag_size);
    if (pkt == NULL)
        return false;
    char * const body = pkt->m_body;

    body[0] = 0xa0 | 0x0c | 0x02 | 0x01; // AAC, 44.1kHz, 16-bit, Stereo
    body[1] = 0; // AAC config

    // Low Complexity profile, 44.1kHz, Stereo
    body[2] = 0x10 | 0x02;
    body[3] = 0x10;

    // It's crucial this packet gets queued.
    return p1_conn_submit_packet(connf, pkt, 0);
}

// Encode and send audio data
size_t p1_conn_stream_audio(P1ConnectionFull *connf, int64_t time, int16_t *buf, size_t samples)
{
    P1Object *connobj = (P1Object *) connf;

    // Encode and build packet using fine-grained lock.
    p1_lock(connobj, &connf->audio_lock);

    if (connobj->state.current != P1_STATE_RUNNING) {
        p1_unlock(connobj, &connf->audio_lock);
        return samples;     // Consume all
    }

    AACENC_BufDesc in_desc = {
        .numBufs           = 1,
        .bufs              = (void *[]) { buf },
        .bufferIdentifiers = (INT []) { IN_AUDIO_DATA },
        .bufSizes          = (INT []) { (INT) (samples * sizeof(int16_t)) },
        .bufElSizes        = (INT []) { sizeof(int16_t) }
    };
    AACENC_BufDesc out_desc = {
        .numBufs           = 1,
        .bufs              = (void *[]) { connf->audio_out },
        .bufferIdentifiers = (INT []) { OUT_BITSTREAM_DATA },
        .bufSizes          = (INT []) { audio_out_size },
        .bufElSizes        = (INT []) { sizeof(UCHAR) }
    };
    AACENC_InArgs in_args = {
        .numInSamples = (INT) samples,
        .numAncBytes  = 0
    };
    AACENC_OutArgs out_args;
    AACENC_ERROR err;

    // Encode a frame if we can.
    err = aacEncEncode(connf->audio_enc, &in_desc, &out_desc, &in_args, &out_args);
    if (err != AACENC_OK) {
        p1_log(connobj, P1_LOG_ERROR, "Failed to AAC encode audio: FDK AAC error %d", err);
        goto fail;
    }
    if (out_args.numOutBytes == 0) {
        p1_unlock(connobj, &connf->audio_lock);
        return out_args.numInSamples;
    }

    // Build the packet.
    const uint32_t tag_size = (uint32_t) (2 + out_args.numOutBytes);
    RTMPPacket *pkt = p1_conn_new_packet(connf, RTMP_PACKET_TYPE_AUDIO, tag_size);
    if (pkt == NULL)
        goto fail;
    char * const body = pkt->m_body;

    body[0] = 0xa0 | 0x0c | 0x02 | 0x01; // AAC, 44.1kHz, 16-bit, Stereo
    body[1] = 1; // AAC raw

    // FIXME: Do the extra work to avoid this copy.
    memcpy(body + 2, out_desc.bufs[0], out_args.numOutBytes);

    p1_unlock(connobj, &connf->audio_lock);

    // Stream using full lock.
    p1_object_lock(connobj);

    if (connobj->state.current == P1_STATE_RUNNING) {
        p1_conn_submit_packet(connf, pkt, time);
    }
    else {
        free(pkt);

        // Consume all.
        out_args.numInSamples = (INT) samples;
    }

    p1_object_unlock(connobj);

    return out_args.numInSamples;

fail:
    p1_unlock(connobj, &connf->audio_lock);

    p1_object_lock(connobj);
    if (connobj->state.current == P1_STATE_RUNNING) {
        connobj->state.current = P1_STATE_STOPPING;
        connobj->state.flags |= P1_FLAG_ERROR;
        p1_object_notify(connobj);
        p1_conn_signal(connf);
    }
    p1_object_unlock(connobj);

    // Consume all.
    return samples;
}


// Allocate a new packet and set header fields.
static RTMPPacket *p1_conn_new_packet(P1ConnectionFull *connf, uint8_t type, uint32_t body_size)
{
    P1Object *connobj = (P1Object *) connf;
    const size_t prelude_size = sizeof(RTMPPacket) + RTMP_MAX_HEADER_SIZE;

    // Allocate packet memory.
    RTMPPacket *pkt = calloc(1, prelude_size + body_size);
    if (pkt == NULL) {
        p1_log(connobj, P1_LOG_ERROR, "Packet allocation failed, dropping packet!");
        return NULL;
    }

    // Fill basic fields.
    pkt->m_packetType = type;
    pkt->m_nChannel = 0x04;
    pkt->m_nBodySize = body_size;
    pkt->m_body = (char *)pkt + prelude_size;

    return pkt;
}

// Submit a packet to the queue. Caller must ensure proper locking.
static bool p1_conn_submit_packet(P1ConnectionFull *connf, RTMPPacket *pkt, int64_t time)
{
    P1Object *connobj = (P1Object *) connf;
    P1Context *ctx = connobj->ctx;
    P1ContextFull *ctxf = (P1ContextFull *) ctx;

    // Determine which queue to use.
    P1PacketQueue *q;
    switch (pkt->m_packetType) {
        case RTMP_PACKET_TYPE_AUDIO:
            q = &connf->audio_queue;
            if (q->length == UINT8_MAX) {
                if (connf->video_queue.length == 0)
                    goto fail_video_lag;
                else
                    goto fail_conn_lag;
            }
            break;
        case RTMP_PACKET_TYPE_VIDEO:
            q = &connf->video_queue;
            if (q->length == UINT8_MAX) {
                if (connf->audio_queue.length == 0)
                    goto fail_audio_lag;
                else
                    goto fail_conn_lag;
            }
            break;
        default: abort();
    }

    // Set timestamp, if one was given.
    if (time) {
        // Relative time.
        time -= connf->start;
        // Convert to milliseconds.
        time = time * ctxf->timebase_num / ctxf->timebase_den / 1000000;
        // x264 may have a couple of frames with negative time.
        if (time < 0) time = 0;
        // Wrap when we exceed 32-bits.
        pkt->m_nTimeStamp = (uint32_t) (time & 0x7fffffff);
    }

    // Start stream with a large header.
    pkt->m_headerType = time ? RTMP_PACKET_SIZE_MEDIUM : RTMP_PACKET_SIZE_LARGE;
    pkt->m_hasAbsTimestamp = pkt->m_headerType == RTMP_PACKET_SIZE_LARGE;

    // Queue the packet.
    q->head[q->write++] = pkt;  // Deliberate overflow of q->write.
    q->length++;
    p1_conn_signal(connf);

    return true;

fail_audio_lag:
    p1_log(connobj, P1_LOG_WARNING, "Audio stream lagging, dropping packet!");
    goto fail;

fail_video_lag:
    p1_log(connobj, P1_LOG_WARNING, "Video stream lagging, dropping packet!");
    goto fail;

fail_conn_lag:
    p1_log(connobj, P1_LOG_WARNING, "Connection lagging, dropping packet!");
    goto fail;

fail:
    free(pkt);
    return false;
}


// The main loop of the streaming thread.
static void *p1_conn_main(void *data)
{
    P1ConnectionFull *connf = (P1ConnectionFull *) data;
    P1Object *connobj = (P1Object *) data;
    char url_copy[2048];
    RTMP *r;
    int ret;

    p1_object_lock(connobj);
    r = &connf->rtmp;

    // This locking is to make cleanup easier; we can assume locked at the
    // fail_* labels, but not at the cleanup label.
    p1_lock(connobj, &connf->audio_lock);
    p1_lock(connobj, &connf->video_lock);

    if (!p1_conn_start_audio(connf)) {
        connobj->state.flags |= P1_FLAG_ERROR;
        goto fail_audio;
    }

    if (!p1_conn_start_video(connf)) {
        connobj->state.flags |= P1_FLAG_ERROR;
        goto fail_video;
    }

    p1_unlock(connobj, &connf->video_lock);
    p1_unlock(connobj, &connf->audio_lock);

    // Connection setup
    if (current_conn == NULL) {
        current_conn = connobj;
        RTMP_LogSetLevel(RTMP_LOGINFO);
        RTMP_LogSetCallback(p1_conn_rtmp_log_callback);
    }
    else {
        p1_log(connobj, P1_LOG_WARNING, "Cannot log for multiple connections.");
    }

    RTMP_Init(r);

    strcpy(url_copy, connf->url);
    ret = RTMP_SetupURL(r, url_copy);
    if (!ret) {
        p1_log(connobj, P1_LOG_ERROR, "Failed to parse URL.");
        connobj->state.flags |= P1_FLAG_ERROR;
        goto cleanup;
    }

    RTMP_EnableWrite(r);

    // Make the connection. Release the lock while we do.
    p1_log(connobj, P1_LOG_INFO, "Connecting to '%.*s:%d' ...",
           r->Link.hostname.av_len, r->Link.hostname.av_val, r->Link.port);
    p1_object_unlock(connobj);
    ret = RTMP_Connect(r, NULL);
    if (ret) {
        int tmp = 1;
        ret = setsockopt(r->m_sb.sb_socket, SOL_SOCKET, SO_NOSIGPIPE, &tmp, sizeof(int));
        ret = (ret == 0);
        if (ret)
            ret = RTMP_ConnectStream(r, 0);
    }
    p1_object_lock(connobj);

    // State can change to stopping from p1_conn_stop. In that case, disregard
    // connection errors and simply clean up.
    if (connobj->state.current == P1_STATE_STOPPING) {
        p1_log(connobj, P1_LOG_INFO, "Connection interrupted.");
        goto cleanup;
    }
    if (!ret) {
        p1_log(connobj, P1_LOG_ERROR, "Connection failed.");
        connobj->state.flags |= P1_FLAG_ERROR;
        goto cleanup;
    }
    p1_log(connobj, P1_LOG_INFO, "Connected.");

    // Queue configuration packets
    if (!p1_conn_stream_audio_config(connf)
        || !p1_conn_stream_video_config(connf)) {
        connobj->state.flags |= P1_FLAG_ERROR;
        goto cleanup;
    }

    // Connection event loop
    connf->start = p1_get_time();

    connobj->state.current = P1_STATE_RUNNING;
    p1_object_notify(connobj);

    do {
        ret = pthread_cond_wait(&connf->cond, &connobj->lock);
        if (ret != 0) {
            p1_log(connobj, P1_LOG_ERROR, "Failed to wait on condition: %s", strerror(ret));
            connobj->state.flags |= P1_FLAG_ERROR;
            goto cleanup;
        }

        if (connobj->state.current != P1_STATE_RUNNING)
            break;

        if (!p1_conn_flush(connf)) {
            connobj->state.flags |= P1_FLAG_ERROR;
            goto cleanup;
        }

    } while (connobj->state.current == P1_STATE_RUNNING);
    p1_log(connobj, P1_LOG_INFO, "Disconnected.");

cleanup:
    p1_lock(connobj, &connf->audio_lock);
    p1_lock(connobj, &connf->video_lock);

    p1_conn_stop_video(connf);

fail_video:
    p1_conn_stop_audio(connf);

fail_audio:
    RTMP_Close(r);
    if (current_conn == connobj)
        current_conn = NULL;

    p1_conn_clear(&connf->audio_queue);
    p1_conn_clear(&connf->video_queue);

    connobj->state.current = P1_STATE_IDLE;
    p1_object_notify(connobj);

    p1_unlock(connobj, &connf->video_lock);
    p1_unlock(connobj, &connf->audio_lock);

    p1_object_unlock(connobj);

    return NULL;
}

// Flush as many queued packets as we can to the connection.
// This is called with the stream lock held.
static bool p1_conn_flush(P1ConnectionFull *connf)
{
    P1Object *connobj = (P1Object *) connf;
    P1PacketQueue *aq = &connf->audio_queue;
    P1PacketQueue *vq = &connf->video_queue;
    int ret;

    // We release the lock while writing, but that means another thread may
    // have signalled in the meantime. Thus we loop until exhausted.
    do {
        // Gather a list of packets to send.

        // We need to chronologically order packets, but they arrive separately.
        // Make sure there is at least one video and audio packet to compare.

        // (In other words, if we don't have one of either, it's possible the
        // other stream will generate a packet with an earlier timestamp.)

        RTMPPacket *list[UINT8_MAX * 2];
        RTMPPacket **i = list;
        while (aq->length != 0 && vq->length != 0) {
            RTMPPacket *ap = aq->head[aq->read];
            RTMPPacket *vp = vq->head[vq->read];
            if (ap->m_nTimeStamp < vp->m_nTimeStamp) {
                *(i++) = ap;
                aq->read++;
                aq->length--;
            }
            else {
                *(i++) = vp;
                vq->read++;
                vq->length--;
            }
        };

        // Now write out the list. Release the lock so blocking doesn't affect
        // other threads queuing new packets.

        if (i == list)
            break;

        p1_object_unlock(connobj);

        RTMP *r = &connf->rtmp;
        RTMPPacket **end = i;
        for (i = list; i != end; i++) {
            RTMPPacket *pkt = *i;

            pkt->m_nInfoField2 = r->m_stream_id;

            ret = RTMP_SendPacket(r, pkt, FALSE);
            free(pkt);
            if (!ret) {
                p1_log(connobj, P1_LOG_ERROR, "Failed to send packet.");
                p1_object_lock(connobj);
                return false;
            }
        }

        p1_object_lock(connobj);
    } while (connobj->state.current == P1_STATE_RUNNING);

    return true;
}


// Audio encoder setup
static bool p1_conn_start_audio(P1ConnectionFull *connf)
{
    P1Object *connobj = (P1Object *) connf;
    AACENC_ERROR err;
    HANDLE_AACENCODER *ae = &connf->audio_enc;

    connf->audio_out = malloc(audio_out_size);
    if (connf->audio_out == NULL)
        goto fail_alloc;

    err = aacEncOpen(ae, 0x01, 2);
    if (err != AACENC_OK) goto fail_open;

    err = aacEncoder_SetParam(*ae, AACENC_AOT, AOT_AAC_LC);
    if (err != AACENC_OK) goto fail_params;
    err = aacEncoder_SetParam(*ae, AACENC_SAMPLERATE, audio_sample_rate);
    if (err != AACENC_OK) goto fail_params;
    err = aacEncoder_SetParam(*ae, AACENC_CHANNELMODE, MODE_2);
    if (err != AACENC_OK) goto fail_params;
    err = aacEncoder_SetParam(*ae, AACENC_BITRATE, audio_bit_rate);
    if (err != AACENC_OK) goto fail_params;
    err = aacEncoder_SetParam(*ae, AACENC_TRANSMUX, TT_MP4_RAW);
    if (err != AACENC_OK) goto fail_params;

    err = aacEncEncode(*ae, NULL, NULL, NULL, NULL);
    if (err != AACENC_OK) goto fail_params;

    return true;

fail_params:
    p1_log(connobj, P1_LOG_ERROR, "Failed to setup audio encoder: FDK AAC error %d", err);

    err = aacEncClose(&connf->audio_enc);
    if (err != AACENC_OK)
        p1_log(connobj, P1_LOG_ERROR, "Failed to close audio encoder: FDK AAC error %d", err);

    goto fail_enc;

fail_open:
    p1_log(connobj, P1_LOG_ERROR, "Failed to open audio encoder: FDK AAC error %d", err);

fail_enc:
    free(connf->audio_out);

fail_alloc:
    return false;
}

static void p1_conn_stop_audio(P1ConnectionFull *connf)
{
    P1Object *connobj = (P1Object *) connf;
    AACENC_ERROR err;

    err = aacEncClose(&connf->audio_enc);
    if (err != AACENC_OK)
        p1_log(connobj, P1_LOG_ERROR, "Failed to close audio encoder: FDK AAC error %d", err);

    free(connf->audio_out);
}


// Video encoder setup
static bool p1_conn_start_video(P1ConnectionFull *connf)
{
    P1Object *connobj = (P1Object *) connf;
    P1Context *ctx = connobj->ctx;
    P1ContextFull *ctxf = (P1ContextFull *) ctx;
    P1Video *video = ctx->video;
    P1VideoClock *vclock = video->clock;
    x264_param_t *vp = &connf->video_params;

    vp->pf_log = p1_conn_x264_log_callback;
    vp->p_log_private = connobj;
    vp->i_log_level = X264_LOG_DEBUG;

    vp->i_timebase_num = ctxf->timebase_num;
    vp->i_timebase_den = ctxf->timebase_den * 1000000000;

    vp->b_aud = 1;
    vp->b_annexb = 0;

    vp->i_width = video->width;
    vp->i_height = video->height;

    vp->i_fps_num = vclock->fps_num;
    vp->i_fps_den = vclock->fps_den;

    if (connf->keyint_sec) {
        double keyint = vclock->fps_num * connf->keyint_sec / vclock->fps_den;
        vp->i_keyint_max = (int) round(keyint);
    }

    connf->video_enc = x264_encoder_open(vp);
    if (connf->video_enc == NULL) {
        p1_log(connobj, P1_LOG_ERROR, "Failed to open x264 encoder");
        return false;
    }

    return true;
}

static void p1_conn_stop_video(P1ConnectionFull *connf)
{
    x264_encoder_close(connf->video_enc);
}


// Condition helper with logging.
static void p1_conn_signal(P1ConnectionFull *connf)
{
    P1Object *connobj = (P1Object *) connf;
    int ret;

    ret = pthread_cond_signal(&connf->cond);
    if (ret != 0)
        p1_log(connobj, P1_LOG_ERROR, "Failed to signal connection thread: %s", strerror(ret));
}

// Clear a packet queue.
static void p1_conn_clear(P1PacketQueue *q)
{
    while (q->length--) {
        RTMPPacket *pkt = q->head[q->read++];
        free(pkt);
    }
    q->read = q->write = q->length = 0;
}


static void p1_conn_x264_log_callback(void *data, int level, const char *fmt, va_list args)
{
    P1Object *videobj = (P1Object *) data;

    // Strip the newline.
    size_t i = strlen(fmt) - 1;
    char fmt2[i + 1];
    if (fmt[i] == '\n') {
        memcpy(fmt2, fmt, i);
        fmt2[i] = '\0';
        fmt = fmt2;
    }

    p1_logv(videobj, (P1LogLevel) level, fmt, args);
}

static void p1_conn_rtmp_log_callback(int level, const char *fmt, va_list args)
{
    P1LogLevel p1_level;
    switch (level) {
        case RTMP_LOGCRIT:      p1_level = P1_LOG_ERROR;    break;
        case RTMP_LOGERROR:     p1_level = P1_LOG_ERROR;    break;
        case RTMP_LOGWARNING:   p1_level = P1_LOG_WARNING;  break;
        case RTMP_LOGINFO:      p1_level = P1_LOG_INFO;     break;
        default:                p1_level = P1_LOG_DEBUG;    break;
    }

    p1_logv(current_conn, p1_level, fmt, args);
}
