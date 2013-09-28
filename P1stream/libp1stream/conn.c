#include "p1stream_priv.h"

#include <stdlib.h>
#include <string.h>

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
// Complete output buffer size, also one full second.
static const int audio_out_size = audio_out_min_size * 64;

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

static bool p1_conn_init_x264_params(P1ConnectionFull *videof, P1Config *cfg, P1ConfigSection *sect);
static bool p1_conn_parse_x264_param(P1Config *cfg, const char *key, char *val, void *data);
static void p1_conn_x264_log_callback(void *data, int level, const char *fmt, va_list args);

static void p1_conn_rtmp_log_callback(int level, const char *fmt, va_list);


bool p1_conn_init(P1ConnectionFull *connf, P1Config *cfg, P1ConfigSection *sect)
{
    P1Object *connobj = (P1Object *) connf;
    int ret;

    if (!p1_object_init(connobj, P1_OTYPE_CONNECTION))
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

    if (!p1_conn_init_x264_params(connf, cfg, sect))
        goto fail_params;

    if (!cfg->get_string(cfg, sect, "url", connf->url, sizeof(connf->url)))
        strcpy(connf->url, default_url);

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
    free(connf->audio_out);

fail_audio_out:
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

void p1_conn_start(P1ConnectionFull *connf)
{
    P1Object *connobj = (P1Object *) connf;

    p1_object_set_state(connobj, P1_STATE_STARTING);

    // Thread will continue start, and set state to running
    int ret = pthread_create(&connf->thread, NULL, p1_conn_main, connf);
    if (ret != 0) {
        p1_log(connobj, P1_LOG_ERROR, "Failed to start connection thread: %s", strerror(ret));
        p1_object_set_state(connobj, P1_STATE_HALTED);
    }
}

void p1_conn_stop(P1ConnectionFull *connf)
{
    P1Object *connobj = (P1Object *) connf;

    // If we acquired the lock during the starting state, that means we're in
    // the middle of a connection attempt. Abort it.
    if (connobj->state == P1_STATE_STARTING)
        RTMP_Close(&connf->rtmp);

    p1_object_set_state(connobj, P1_STATE_STOPPING);
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

    if (connobj->state == P1_STATE_RUNNING)
        p1_conn_submit_packet(connf, pkt, time);
    else
        free(pkt);

    p1_object_unlock(connobj);

    return;

fail:
    p1_unlock(connobj, &connf->video_lock);

    p1_object_lock(connobj);
    if (connobj->state == P1_STATE_RUNNING) {
        p1_object_set_state(connobj, P1_STATE_HALTING);
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

    INT el_sizes[] = { sizeof(int16_t) };

    void *in_bufs[] = { buf };
    INT in_identifiers[] = { IN_AUDIO_DATA };
    INT in_sizes[] = { (INT) (samples * sizeof(int16_t)) };
    AACENC_BufDesc in_desc = {
        .numBufs = 1,
        .bufs = in_bufs,
        .bufferIdentifiers = in_identifiers,
        .bufSizes = in_sizes,
        .bufElSizes = el_sizes
    };

    void *out_bufs[] = { connf->audio_out };
    INT out_identifiers[] = { OUT_BITSTREAM_DATA };
    INT out_sizes[] = { audio_out_size };
    AACENC_BufDesc out_desc = {
        .numBufs = 1,
        .bufs = out_bufs,
        .bufferIdentifiers = out_identifiers,
        .bufSizes = out_sizes,
        .bufElSizes = el_sizes
    };

    AACENC_InArgs in_args = {
        .numInSamples = (INT) samples,
        .numAncBytes = 0
    };

    // Encode as much as we can; FDK AAC gives us small batches.
    size_t size = 0;
    size_t samples_read = 0;
    AACENC_ERROR err;
    AACENC_OutArgs out_args = { .numInSamples = 1 };
    while (in_args.numInSamples && out_args.numInSamples && out_desc.bufSizes[0] > audio_out_min_size) {
        err = aacEncEncode(connf->audio_enc, &in_desc, &out_desc, &in_args, &out_args);
        if (err != AACENC_OK) {
            p1_log(connobj, P1_LOG_ERROR, "Failed to AAC encode audio: FDK AAC error %d", err);
            goto fail;
        }

        size_t in_processed = out_args.numInSamples * sizeof(int16_t);
        in_desc.bufs[0] += in_processed;
        in_desc.bufSizes[0] -= in_processed;

        size_t out_bytes = out_args.numOutBytes;
        out_desc.bufs[0] += out_bytes;
        out_desc.bufSizes[0] -= out_bytes;

        in_args.numInSamples -= out_args.numInSamples;

        size += out_args.numOutBytes;
        samples_read += out_args.numInSamples;
    }

    if (size == 0) {
        p1_unlock(connobj, &connf->video_lock);
        return samples_read;
    }

    const uint32_t tag_size = (uint32_t) (2 + size);
    RTMPPacket *pkt = p1_conn_new_packet(connf, RTMP_PACKET_TYPE_AUDIO, tag_size);
    if (pkt == NULL)
        goto fail;
    char * const body = pkt->m_body;

    body[0] = 0xa0 | 0x0c | 0x02 | 0x01; // AAC, 44.1kHz, 16-bit, Stereo
    body[1] = 1; // AAC raw

    // FIXME: Do the extra work to avoid this copy.
    memcpy(body + 2, buf, size);

    p1_unlock(connobj, &connf->audio_lock);

    // Stream using full lock.
    p1_object_lock(connobj);

    if (connobj->state == P1_STATE_RUNNING) {
        p1_conn_submit_packet(connf, pkt, time);
    }
    else {
        free(pkt);

        // Consume all.
        samples_read = samples;
    }

    p1_object_unlock(connobj);

    return samples_read;

fail:
    p1_unlock(connobj, &connf->audio_lock);

    p1_object_lock(connobj);
    if (connobj->state == P1_STATE_RUNNING) {
        p1_object_set_state(connobj, P1_STATE_HALTING);
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
        case RTMP_PACKET_TYPE_AUDIO: q = &connf->audio_queue; break;
        case RTMP_PACKET_TYPE_VIDEO: q = &connf->video_queue; break;
        default: abort();
    }
    if (q->length == UINT8_MAX) {
        free(pkt);
        p1_log(connobj, P1_LOG_WARNING, "Packet queue full, dropping packet!");
        return false;
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
}


// The main loop of the streaming thread.
static void *p1_conn_main(void *data)
{
    P1ConnectionFull *connf = (P1ConnectionFull *) data;
    P1Object *connobj = (P1Object *) data;
    RTMP *r;
    int ret;

    p1_object_lock(connobj);
    r = &connf->rtmp;

    if (!p1_conn_start_audio(connf))
        goto fail_audio;
    if (!p1_conn_start_video(connf))
        goto fail_video;

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

    ret = RTMP_SetupURL(r, connf->url);
    if (!ret) {
        p1_log(connobj, P1_LOG_ERROR, "Failed to parse URL.");
        goto fail_rtmp;
    }

    RTMP_EnableWrite(r);

    // Make the connection. Release the lock while we do.
    p1_log(connobj, P1_LOG_INFO, "Connecting to '%.*s:%d' ...",
           r->Link.hostname.av_len, r->Link.hostname.av_val, r->Link.port);
    p1_object_unlock(connobj);
    ret = RTMP_Connect(r, NULL);
    if (ret)
        ret = RTMP_ConnectStream(r, 0);
    p1_object_lock(connobj);

    // State can change to stopping from p1_conn_stop. In that case, disregard
    // connection errors and simply clean up.
    if (connobj->state == P1_STATE_STOPPING) {
        p1_log(connobj, P1_LOG_INFO, "Connection interrupted.");
        goto cleanup_video;
    }
    if (!ret) {
        p1_log(connobj, P1_LOG_ERROR, "Connection failed.");
        goto fail_rtmp;
    }
    p1_log(connobj, P1_LOG_INFO, "Connected.");

    // Queue configuration packets
    if (!p1_conn_stream_audio_config(connf))
        goto fail_rtmp;
    if (!p1_conn_stream_video_config(connf))
        goto fail_rtmp;

    // Connection event loop
    connf->start = p1_get_time();
    p1_object_set_state(connobj, P1_STATE_RUNNING);

    do {
        ret = pthread_cond_wait(&connf->cond, &connobj->lock);
        if (ret != 0) {
            p1_log(connobj, P1_LOG_ERROR, "Failed to wait on condition: %s", strerror(ret));
            goto fail_rtmp;
        }

        if (connobj->state != P1_STATE_RUNNING)
            break;

        if (!p1_conn_flush(connf))
            goto fail_rtmp;

    } while (connobj->state == P1_STATE_RUNNING);
    p1_log(connobj, P1_LOG_INFO, "Disconnected.");

cleanup_video:
    p1_conn_stop_video(connf);

cleanup_audio:
    p1_conn_stop_audio(connf);

cleanup:
    RTMP_Close(r);
    if (current_conn == connobj)
        current_conn = NULL;

    p1_conn_clear(&connf->video_queue);
    p1_conn_clear(&connf->audio_queue);

    if (connobj->state == P1_STATE_STOPPING)
        p1_object_set_state(connobj, P1_STATE_IDLE);
    else
        p1_object_set_state(connobj, P1_STATE_HALTED);

    p1_object_unlock(connobj);

    return NULL;

fail_rtmp:
    p1_object_set_state(connobj, P1_STATE_HALTING);
    goto cleanup_video;

fail_video:
    p1_object_set_state(connobj, P1_STATE_HALTING);
    goto cleanup_audio;

fail_audio:
    p1_object_set_state(connobj, P1_STATE_HALTING);
    goto cleanup;
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
    } while (connobj->state == P1_STATE_RUNNING);

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
    P1VideoClock *vclock = ctx->video->clock;
    x264_param_t *vp = &connf->video_params;

    vp->pf_log = p1_conn_x264_log_callback;
    vp->p_log_private = connobj;
    vp->i_log_level = X264_LOG_DEBUG;

    vp->i_timebase_num = ctxf->timebase_num;
    vp->i_timebase_den = ctxf->timebase_den * 1000000000;

    vp->b_aud = 1;
    vp->b_annexb = 0;

    vp->i_width = 1280;
    vp->i_height = 720;

    vp->i_fps_num = vclock->fps_num;
    vp->i_fps_den = vclock->fps_den;

    connf->video_enc = x264_encoder_open(vp);
    if (connf->video_enc == NULL) {
        p1_log(connobj, P1_LOG_ERROR, "Failed to open x264 encoder");
        return false;
    }

    return true;
}

static void p1_conn_stop_video(P1ConnectionFull *connf)
{
    P1Object *connobj = (P1Object *) connf;

    p1_lock(connobj, &connf->video_lock);

    x264_encoder_close(connf->video_enc);

    p1_unlock(connobj, &connf->video_lock);
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


static bool p1_conn_init_x264_params(P1ConnectionFull *connf, P1Config *cfg, P1ConfigSection *sect)
{
    x264_param_t *params = &connf->video_params;
    char tmp[128];
    int ret;

    // x264 already logs errors, except for x264_param_parse.

    x264_param_default(params);

    if (cfg->get_string(cfg, sect, "encoder.preset", tmp, sizeof(tmp))) {
        ret = x264_param_default_preset(params, tmp, NULL);
        if (ret != 0)
            return false;
    }

    if (cfg->get_string(cfg, sect, "encoder.tune", tmp, sizeof(tmp))) {
        ret = x264_param_default_preset(params, NULL, tmp);
        if (ret != 0)
            return false;
    }

    if (!cfg->each_string(cfg, sect, "encoder", p1_conn_parse_x264_param, connf))
        return false;

    x264_param_apply_fastfirstpass(params);

    if (cfg->get_string(cfg, sect, "encoder.profile", tmp, sizeof(tmp))) {
        ret = x264_param_apply_profile(params, tmp);
        if (ret != 0)
            return false;
    }

    return true;
}

static bool p1_conn_parse_x264_param(P1Config *cfg, const char *key, char *val, void *data)
{
    P1Object *connobj = (P1Object *) data;
    P1ConnectionFull *connf = (P1ConnectionFull *) data;
    int ret;

    if (strcmp(key, "preset") == 0 ||
        strcmp(key, "profile") == 0 ||
        strcmp(key, "tune") == 0)
        return true;

    ret = x264_param_parse(&connf->video_params, key, val);
    if (ret != 0) {
        if (ret == X264_PARAM_BAD_NAME)
            p1_log(connobj, P1_LOG_ERROR, "Invalid x264 parameter name '%s'", key);
        else if (ret == X264_PARAM_BAD_VALUE)
            p1_log(connobj, P1_LOG_ERROR, "Invalid value for x264 parameter '%s'", key);
        return false;
    }

    return true;
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
