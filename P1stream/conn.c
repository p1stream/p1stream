#include "p1stream_priv.h"

#include <stdlib.h>
#include <string.h>

static const char *default_url = "rtmp://localhost/app/test";

static RTMPPacket *p1_conn_new_packet(P1ConnectionFull *connf, uint8_t type, uint32_t body_size);
static void p1_conn_submit_packet(P1ConnectionFull *connf, RTMPPacket *pkt, int64_t time);
static void *p1_conn_main(void *data);
static bool p1_conn_flush(RTMP *r, P1ConnectionFull *connf);

bool p1_conn_init(P1ConnectionFull *connf, P1Config *cfg, P1ConfigSection *sect)
{
    P1Object *connobj = (P1Object *) connf;

    if (!p1_object_init(connobj, P1_OTYPE_CONNECTION))
        goto fail_object;

    int ret = pthread_cond_init(&connf->cond, NULL);
    if (ret != 0) {
        p1_log(connobj, P1_LOG_ERROR, "Failed to initialize condition variable: %s", strerror(ret));
        goto fail_cond;
    }

    if (!cfg->get_string(cfg, sect, "url", connf->url, sizeof(connf->url)))
        strcpy(connf->url, default_url);

    return true;

fail_cond:
    p1_object_destroy(connobj);

fail_object:
    return false;
}

void p1_conn_destroy(P1ConnectionFull *connf)
{
    P1Object *connobj = (P1Object *) connf;

    int ret = pthread_cond_destroy(&connf->cond);
    if (ret != 0)
        p1_log(connobj, P1_LOG_ERROR, "Failed to destroy condition variable: %s", strerror(ret));

    p1_object_destroy(connobj);
}

void p1_conn_start(P1ConnectionFull *connf)
{
    P1Object *connobj = (P1Object *) connf;

    p1_object_set_state(connobj, P1_STATE_STARTING);

    int ret = pthread_create(&connf->thread, NULL, p1_conn_main, connf);
    if (ret != 0) {
        p1_log(connobj, P1_LOG_ERROR, "Failed to start connection thread: %s", strerror(ret));
        p1_object_set_state(connobj, P1_STATE_HALTED);
    }
}

void p1_conn_stop(P1ConnectionFull *connf)
{
    P1Object *connobj = (P1Object *) connf;

    p1_object_set_state(connobj, P1_STATE_STOPPING);

    int ret = pthread_cond_signal(&connf->cond);
    if (ret != 0)
        p1_log(connobj, P1_LOG_ERROR, "Failed to signal connection thread: %s", strerror(ret));
}

// Send video configuration.
void p1_conn_video_config(P1ConnectionFull *connf, x264_nal_t *nals, int len)
{
    P1Object *connobj = (P1Object *) connf;
    int i;

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
        return;
    }

    int sps_size = nal_sps->i_payload-4;
    int pps_size = nal_pps->i_payload-4;
    uint32_t tag_size = 16 + sps_size + pps_size;

    RTMPPacket *pkt = p1_conn_new_packet(connf, RTMP_PACKET_TYPE_VIDEO, tag_size);
    if (pkt == NULL) return;
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

    p1_conn_submit_packet(connf, pkt, 0);
}

// Send video data.
void p1_conn_video(P1ConnectionFull *connf, x264_nal_t *nals, int len, x264_picture_t *pic)
{
    uint32_t size = 0;
    for (int i = 0; i < len; i++)
        size += nals[i].i_payload;
    const uint32_t tag_size = size + 5;

    RTMPPacket *pkt = p1_conn_new_packet(connf, RTMP_PACKET_TYPE_VIDEO, tag_size);
    if (pkt == NULL) return;
    char * const body = pkt->m_body;

    body[0] = (pic->b_keyframe ? 0x10 : 0x20) | 0x07; // keyframe/IDR, AVC
    body[1] = 1; // AVC NALU
    // skip composition time

    memcpy(body + 5, nals[0].p_payload, size);

    p1_conn_submit_packet(connf, pkt, pic->i_dts);
}

// Send audio configuration.
void p1_conn_audio_config(P1ConnectionFull *connf)
{
    const uint32_t tag_size = 2 + 2;

    RTMPPacket *pkt = p1_conn_new_packet(connf, RTMP_PACKET_TYPE_AUDIO, tag_size);
    if (pkt == NULL) return;
    char * const body = pkt->m_body;

    body[0] = 0xa0 | 0x0c | 0x02 | 0x01; // AAC, 44.1kHz, 16-bit, Stereo
    body[1] = 0; // AAC config

    // Low Complexity profile, 44.1kHz, Stereo
    body[2] = 0x10 | 0x02;
    body[3] = 0x10;

    p1_conn_submit_packet(connf, pkt, 0);
}

// Send audio data.
void p1_conn_audio(P1ConnectionFull *connf, int64_t mtime, void *buf, size_t len)
{
    const uint32_t tag_size = (uint32_t) (2 + len);

    RTMPPacket *pkt = p1_conn_new_packet(connf, RTMP_PACKET_TYPE_AUDIO, tag_size);
    if (pkt == NULL) return;
    char * const body = pkt->m_body;

    body[0] = 0xa0 | 0x0c | 0x02 | 0x01; // AAC, 44.1kHz, 16-bit, Stereo
    body[1] = 1; // AAC raw

    // FIXME: Do the extra work to avoid this copy.
    memcpy(body + 2, buf, len);

    p1_conn_submit_packet(connf, pkt, mtime);
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

// Submit a packet to the queue.
static void p1_conn_submit_packet(P1ConnectionFull *connf, RTMPPacket *pkt, int64_t time)
{
    P1Object *connobj = (P1Object *) connf;
    P1Context *ctx = connobj->ctx;
    P1ContextFull *ctxf = (P1ContextFull *) ctx;
    int ret;

    p1_object_lock(connobj);

    if (connobj->state != P1_STATE_RUNNING) {
        free(pkt);
        goto end;
    }

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
        goto end;
    }

    // Set timestamp, if one was given.
    if (time) {
        // Relative time.
        time -= connf->start;
        // Convert to milliseconds.
        time = time * ctxf->timebase.numer / ctxf->timebase.denom / 1000000;
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
    ret = pthread_cond_signal(&connf->cond);
    if (ret != 0)
        p1_log(connobj, P1_LOG_ERROR, "Failed to signal connection thread: %s", strerror(ret));

end:
    p1_object_unlock(connobj);
}

// The main loop of the streaming thread.
static void *p1_conn_main(void *data)
{
    P1ConnectionFull *connf = (P1ConnectionFull *) data;
    P1Object *connobj = (P1Object *) data;
    RTMP r;
    int ret;

    RTMP_Init(&r);

    ret = RTMP_SetupURL(&r, connf->url);
    if (!ret) {
        p1_log(connobj, P1_LOG_ERROR, "Failed to parse URL.");
        goto fail;
    }

    RTMP_EnableWrite(&r);

    ret = RTMP_Connect(&r, NULL);
    if (!ret) {
        p1_log(connobj, P1_LOG_ERROR, "Connection failed.");
        goto fail;
    }

    ret = RTMP_ConnectStream(&r, 0);
    if (!ret) {
        p1_log(connobj, P1_LOG_ERROR, "Failed to connect to stream.");
        goto fail;
    }

    p1_object_lock(connobj);

    connf->start = mach_absolute_time();
    p1_object_set_state(connobj, P1_STATE_RUNNING);

    do {
        if (!p1_conn_flush(&r, connf))
            goto fail_locked;

        ret = pthread_cond_wait(&connf->cond, &connobj->lock);
        if (ret != 0) {
            p1_log(connobj, P1_LOG_ERROR, "Failed to wait on condition: %s\n", strerror(ret));
            goto fail_locked;
        }
    } while (connobj->state == P1_STATE_RUNNING);

    RTMP_Close(&r);

    p1_object_set_state(connobj, P1_STATE_IDLE);

    p1_object_unlock(connobj);

    return NULL;

fail:
    p1_object_lock(connobj);

fail_locked:
    p1_object_set_state(connobj, P1_STATE_HALTING);
    RTMP_Close(&r);
    p1_object_set_state(connobj, P1_STATE_HALTED);

    p1_object_unlock(connobj);

    return NULL;
}

// Flush as many queued packets as we can to the connection.
// This is called with the stream lock held.
static bool p1_conn_flush(RTMP *r, P1ConnectionFull *connf)
{
    P1Object *connobj = (P1Object *) connf;
    P1PacketQueue *aq = &connf->audio_queue;
    P1PacketQueue *vq = &connf->video_queue;
    int ret;
    bool result = true;

    // We release the lock while writing, but that means another thread may
    // have signalled in the meantime. Thus we loop until exhausted.

    while (true) {

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

        RTMPPacket **end = i;
        for (i = list; i != end; i++) {
            RTMPPacket *pkt = *i;

            pkt->m_nInfoField2 = r->m_stream_id;

            ret = RTMP_SendPacket(r, pkt, FALSE);
            free(pkt);
            if (!ret) {
                p1_log(connobj, P1_LOG_ERROR, "Failed to send packet.");
                result = false;
                break;
            }
        }

        p1_object_lock(connobj);
    }

    return result;
}
