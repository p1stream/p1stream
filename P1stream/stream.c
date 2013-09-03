#include "p1stream_priv.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

static const char *default_url = "rtmp://localhost/app/test";

static RTMPPacket *p1_conn_new_packet(P1ConnectionFull *connf, uint8_t type, int64_t time, uint32_t body_size);
static void p1_conn_submit_packet(P1ConnectionFull *connf, RTMPPacket *pkt);
static void *p1_conn_main(void *data);
static void p1_conn_flush(P1ConnectionFull *connf);

void p1_conn_init(P1ConnectionFull *connf, P1Config *cfg, P1ConfigSection *sect)
{
    RTMP *r = &connf->rtmp;
    int res;

    res = pthread_mutex_init(&connf->lock, NULL);
    assert(res == 0);

    res = pthread_cond_init(&connf->cond, NULL);
    assert(res == 0);

    RTMP_Init(r);

    if (!cfg->get_string(cfg, sect, "url", connf->url, sizeof(connf->url)))
        strcpy(connf->url, default_url);
    res = RTMP_SetupURL(r, connf->url);
    assert(res == TRUE);

    RTMP_EnableWrite(r);
}

void p1_conn_start(P1ConnectionFull *connf)
{
    P1Connection *conn = (P1Connection *) connf;
    P1Context *ctx = conn->ctx;

    p1_set_state(ctx, P1_OTYPE_CONNECTION, conn, P1_STATE_STARTING);

    int res = pthread_create(&connf->thread, NULL, p1_conn_main, connf);
    assert(res == 0);
}

void p1_conn_stop(P1ConnectionFull *connf)
{
    // FIXME
}

// Send video configuration.
void p1_conn_video_config(P1ConnectionFull *connf, x264_nal_t *nals, int len)
{
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
    assert(nal_sps != NULL && nal_pps != NULL);

    int sps_size = nal_sps->i_payload-4;
    int pps_size = nal_pps->i_payload-4;
    uint32_t tag_size = 16 + sps_size + pps_size;

    RTMPPacket *pkt = p1_conn_new_packet(connf, RTMP_PACKET_TYPE_VIDEO, 0, tag_size);
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

    p1_conn_submit_packet(connf, pkt);
}

// Send video data.
void p1_conn_video(P1ConnectionFull *connf, x264_nal_t *nals, int len, x264_picture_t *pic)
{
    uint32_t size = 0;
    for (int i = 0; i < len; i++)
        size += nals[i].i_payload;
    const uint32_t tag_size = size + 5;

    RTMPPacket *pkt = p1_conn_new_packet(connf, RTMP_PACKET_TYPE_VIDEO, pic->i_dts, tag_size);
    char * const body = pkt->m_body;

    body[0] = (pic->b_keyframe ? 0x10 : 0x20) | 0x07; // keyframe/IDR, AVC
    body[1] = 1; // AVC NALU
    // skip composition time

    memcpy(body + 5, nals[0].p_payload, size);

    p1_conn_submit_packet(connf, pkt);
}

// Send audio configuration.
void p1_conn_audio_config(P1ConnectionFull *connf)
{
    const uint32_t tag_size = 2 + 2;

    RTMPPacket *pkt = p1_conn_new_packet(connf, RTMP_PACKET_TYPE_AUDIO, 0, tag_size);
    char * const body = pkt->m_body;

    body[0] = 0xa0 | 0x0c | 0x02 | 0x01; // AAC, 44.1kHz, 16-bit, Stereo
    body[1] = 0; // AAC config

    // Low Complexity profile, 44.1kHz, Stereo
    body[2] = 0x10 | 0x02;
    body[3] = 0x10;

    p1_conn_submit_packet(connf, pkt);
}

// Send audio data.
void p1_conn_audio(P1ConnectionFull *connf, int64_t mtime, void *buf, size_t len)
{
    const uint32_t tag_size = (uint32_t) (2 + len);

    RTMPPacket *pkt = p1_conn_new_packet(connf, RTMP_PACKET_TYPE_AUDIO, mtime, tag_size);
    char * const body = pkt->m_body;

    body[0] = 0xa0 | 0x0c | 0x02 | 0x01; // AAC, 44.1kHz, 16-bit, Stereo
    body[1] = 1; // AAC raw

    // FIXME: Do the extra work to avoid this copy.
    memcpy(body + 2, buf, len);

    p1_conn_submit_packet(connf, pkt);
}

// Allocate a new packet and set header fields.
static RTMPPacket *p1_conn_new_packet(P1ConnectionFull *connf, uint8_t type, int64_t time, uint32_t body_size)
{
    P1Connection *conn = (P1Connection *) connf;
    P1Context *ctx = conn->ctx;
    P1ContextFull *ctxf = (P1ContextFull *) ctx;
    RTMP *r = &connf->rtmp;
    const size_t prelude_size = sizeof(RTMPPacket) + RTMP_MAX_HEADER_SIZE;

    // We read some state without synchronisation here, but that's okay,
    // because we only touch parts that don't change during the connection.

    // Allocate packet memory.
    RTMPPacket *pkt = calloc(1, prelude_size + body_size);
    assert(pkt != NULL);

    // Fill basic fields.
    pkt->m_packetType = type;
    pkt->m_nChannel = 0x04;
    pkt->m_nInfoField2 = r->m_stream_id;
    pkt->m_nBodySize = body_size;
    pkt->m_body = (char *)pkt + prelude_size;

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

    return pkt;
}

// Submit a packet to the queue.
static void p1_conn_submit_packet(P1ConnectionFull *connf, RTMPPacket *pkt)
{
    P1Connection *conn = (P1Connection *) connf;
    P1Context *ctx = conn->ctx;
    int res;

    P1PacketQueue *q;
    switch (pkt->m_packetType) {
        case RTMP_PACKET_TYPE_AUDIO: q = &connf->audio_queue; break;
        case RTMP_PACKET_TYPE_VIDEO: q = &connf->video_queue; break;
        default: abort();
    }

    res = pthread_mutex_lock(&connf->lock);
    assert(res == 0);

    if (q->length == UINT8_MAX) {
        free(pkt);
        p1_log(ctx, P1_LOG_WARNING, "Packet queue full, dropping packet!\n");
        goto end;
    }

    // Deliberate overflow of q->write.
    q->head[q->write++] = pkt;
    q->length++;

    res = pthread_cond_signal(&connf->cond);
    assert(res == 0);

end:
    res = pthread_mutex_unlock(&connf->lock);
    assert(res == 0);
}

// The main loop of the streaming thread.
static void *p1_conn_main(void *data)
{
    P1ConnectionFull *connf = (P1ConnectionFull *) data;
    P1Connection *conn = (P1Connection *) connf;
    P1Context *ctx = conn->ctx;
    RTMP *r = &connf->rtmp;
    int res;

    res = RTMP_Connect(r, NULL);
    assert(res == TRUE);

    res = RTMP_ConnectStream(r, 0);
    assert(res == TRUE);

    res = pthread_mutex_lock(&connf->lock);
    assert(res == 0);

    connf->start = mach_absolute_time();
    p1_set_state(ctx, P1_OTYPE_CONNECTION, conn, P1_STATE_RUNNING);

    do {
        p1_conn_flush(connf);

        res = pthread_cond_wait(&connf->cond, &connf->lock);
        assert(res == 0);
    } while (conn->state == P1_STATE_RUNNING);

    res = pthread_mutex_unlock(&connf->lock);
    assert(res == 0);

    return NULL;
}

// Flush as many queued packets as we can to the connection.
// This is called with the stream lock held.
static void p1_conn_flush(P1ConnectionFull *connf)
{
    RTMP *r = &connf->rtmp;
    P1PacketQueue *aq = &connf->audio_queue;
    P1PacketQueue *vq = &connf->video_queue;
    int res;

    // We release the lock while writing, but that means another thread may
    // have signalled in the meantime. Thus we loop until exhausted.

    while (true) {

        // We need to chronologically order packets, but they arrive separately.
        // Wait until we have at least one audio and one video packet to compare.

        // (In other words, if we don't have one of either, it's possible the other
        // stream will generate a packet with an earlier timestamp.)

        RTMPPacket *last_audio = (aq->length != 0) ? aq->head[aq->write - 1] : NULL;
        RTMPPacket *last_video = (vq->length != 0) ? vq->head[vq->write - 1] : NULL;
        if (last_audio == NULL || last_video == NULL)
            return;

        // Gather a list of packets to send.

        RTMPPacket *list[UINT8_MAX * 2];
        RTMPPacket **i = list;

        RTMPPacket *ap = aq->head[aq->read];
        RTMPPacket *vp = vq->head[vq->read];
        RTMPPacket *pkt;
        do {
            if (ap->m_nTimeStamp < vp->m_nTimeStamp) {
                pkt = ap;

                ap = aq->head[++aq->read];
                aq->length--;
            }
            else {
                pkt = vp;

                vp = vq->head[++vq->read];
                vq->length--;
            }

            *(i++) = pkt;
        } while (pkt != last_audio && pkt != last_video);

        // Now write out the list. Release the lock so blocking doesn't affect
        // other threads queuing new packets.

        res = pthread_mutex_unlock(&connf->lock);
        assert(res == 0);

        RTMPPacket **end = i;
        for (i = list; i != end; i++) {
            pkt = *i;

            res = RTMP_SendPacket(r, pkt, FALSE);
            free(pkt);
            assert(res == TRUE);
        }

        res = pthread_mutex_lock(&connf->lock);
        assert(res == 0);
    }
}
