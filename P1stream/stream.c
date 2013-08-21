#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "p1stream_priv.h"


static const char *default_url = "rtmp://localhost/app/test";

static RTMPPacket *p1_stream_new_packet(P1Context *ctx, uint8_t type, int64_t time, uint32_t body_size);
static void p1_stream_submit_packet(P1Context *ctx, RTMPPacket *pkt);
static void p1_stream_submit_packet_on_thread(P1Context *ctx, RTMPPacket *pkt);


// Setup state and connect.
void p1_stream_init(P1Context *ctx, P1Config *cfg)
{
    int res;
    RTMP * const r = &ctx->rtmp;

    ctx->dispatch = dispatch_queue_create("stream", DISPATCH_QUEUE_SERIAL);

    RTMP_Init(r);

    if (!cfg->get_string(cfg, NULL, "stream.url", ctx->url, sizeof(ctx->url)))
        strcpy(ctx->url, default_url);
    res = RTMP_SetupURL(r, ctx->url);
    assert(res == TRUE);

    RTMP_EnableWrite(r);

    res = RTMP_Connect(r, NULL);
    assert(res == TRUE);

    res = RTMP_ConnectStream(r, 0);
    assert(res == TRUE);

    ctx->start = mach_absolute_time();
}

// Send video configuration.
void p1_stream_video_config(P1Context *ctx, x264_nal_t *nals, int len)
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

    RTMPPacket *pkt = p1_stream_new_packet(ctx, RTMP_PACKET_TYPE_VIDEO, 0, tag_size);
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

    p1_stream_submit_packet(ctx, pkt);
}

// Send video data.
void p1_stream_video(P1Context *ctx, x264_nal_t *nals, int len, x264_picture_t *pic)
{
    uint32_t size = 0;
    for (int i = 0; i < len; i++)
        size += nals[i].i_payload;
    const uint32_t tag_size = size + 5;

    RTMPPacket *pkt = p1_stream_new_packet(ctx, RTMP_PACKET_TYPE_VIDEO, pic->i_dts, tag_size);
    char * const body = pkt->m_body;

    body[0] = (pic->b_keyframe ? 0x10 : 0x20) | 0x07; // keyframe/IDR, AVC
    body[1] = 1; // AVC NALU
    // skip composition time

    memcpy(body + 5, nals[0].p_payload, size);

    p1_stream_submit_packet(ctx, pkt);
}

// Send audio configuration.
void p1_stream_audio_config(P1Context *ctx)
{
    const uint32_t tag_size = 2 + 2;

    RTMPPacket *pkt = p1_stream_new_packet(ctx, RTMP_PACKET_TYPE_AUDIO, 0, tag_size);
    char * const body = pkt->m_body;

    body[0] = 0xa0 | 0x0c | 0x02 | 0x01; // AAC, 44.1kHz, 16-bit, Stereo
    body[1] = 0; // AAC config

    // Low Complexity profile, 44.1kHz, Stereo
    body[2] = 0x10 | 0x02;
    body[3] = 0x10;

    p1_stream_submit_packet(ctx, pkt);
}

// Send audio data.
void p1_stream_audio(P1Context *ctx, int64_t mtime, void *buf, int len)
{
    const uint32_t tag_size = 2 + len;

    RTMPPacket *pkt = p1_stream_new_packet(ctx, RTMP_PACKET_TYPE_AUDIO, mtime, tag_size);
    char * const body = pkt->m_body;

    body[0] = 0xa0 | 0x0c | 0x02 | 0x01; // AAC, 44.1kHz, 16-bit, Stereo
    body[1] = 1; // AAC raw

    // FIXME: Do the extra work to avoid this copy.
    memcpy(body + 2, buf, len);

    p1_stream_submit_packet(ctx, pkt);
}

// Allocate a new packet and set header fields.
static RTMPPacket *p1_stream_new_packet(P1Context *ctx, uint8_t type, int64_t time, uint32_t body_size)
{
    const size_t prelude_size = sizeof(RTMPPacket) + RTMP_MAX_HEADER_SIZE;

    // We read some state without synchronisation here, but that's okay,
    // because we only touch parts that don't change during the connection.

    // Allocate packet memory.
    RTMPPacket *pkt = calloc(1, prelude_size + body_size);
    assert(pkt != NULL);

    // Fill basic fields.
    pkt->m_packetType = type;
    pkt->m_nChannel = 0x04;
    pkt->m_nInfoField2 = ctx->rtmp.m_stream_id;
    pkt->m_nBodySize = body_size;
    pkt->m_body = (char *)pkt + prelude_size;

    // Set timestamp, if one was given.
    if (time) {
        // Relative time.
        time -= ctx->start;
        // Convert to milliseconds.
        time = time * ctx->timebase.numer / ctx->timebase.denom / 1000000;
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

// Submit a packet. It'll either be sent immediately, or queued.
static void p1_stream_submit_packet(P1Context *ctx, RTMPPacket *pkt)
{
    dispatch_async(ctx->dispatch, ^{
        p1_stream_submit_packet_on_thread(ctx, pkt);
    });
}

// Continuation of p1_stream_submit_packet when on the correct thread.
static void p1_stream_submit_packet_on_thread(P1Context *ctx, RTMPPacket *pkt)
{
    int err;
    RTMP * const r = &ctx->rtmp;

    // The logic here assumes there will only ever be two types of packets
    // in the queue. We only send audio and video packets.

    // We only need to queue packets with relative timestamps.
    if (pkt->m_hasAbsTimestamp)
        goto send;

    // Queue is empty, always queue the packet.
    if (ctx->q_len == 0)
        goto queue;

    // Already queuing packets of this type.
    if (ctx->q[ctx->q_start]->m_packetType == pkt->m_packetType)
        goto queue;

    // Dequeue all packets of other types that come before this one.
    while (ctx->q_len) {
        RTMPPacket *dequeue_pkt = ctx->q[ctx->q_start];

        // FIXME: doesn't account for wrapping
        if (dequeue_pkt->m_nTimeStamp > pkt->m_nTimeStamp) break;

        err = RTMP_SendPacket(r, dequeue_pkt, FALSE);
        free(dequeue_pkt);
        assert(err == TRUE);

        ctx->q_start = (ctx->q_start + 1) % P1_PACKET_QUEUE_LENGTH;
        ctx->q_len--;
    }

    // Queue is empty again, queue the packet.
    if (ctx->q_len == 0)
        goto queue;

    // Remaining packets come after this one, so send immediately.
    goto send;

queue:
    if (ctx->q_len == P1_PACKET_QUEUE_LENGTH) {
        printf("A/V desync, dropping packet!\n");
        free(pkt);
    }
    else {
        size_t pos = (ctx->q_start + ctx->q_len++) % P1_PACKET_QUEUE_LENGTH;
        ctx->q[pos] = pkt;
    }
    return;

send:
    err = RTMP_SendPacket(r, pkt, FALSE);
    free(pkt);
    assert(err == TRUE);
}
