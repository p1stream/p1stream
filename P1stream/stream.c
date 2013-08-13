#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <mach/mach_time.h>
#include <rtmp.h>

#include "stream.h"
#include "conf.h"

static const size_t max_queue_len = 64;

static struct {
    RTMP rtmp;

    // ring buffer
    RTMPPacket *q[max_queue_len];
    size_t q_start;
    size_t q_len;

    mach_timebase_info_data_t timebase;
    uint64_t start;
} state;

static RTMPPacket *p1_stream_new_packet(uint8_t type, int64_t time, uint32_t body_size);
static void p1_stream_submit_packet(RTMPPacket *pkt);


void p1_stream_init()
{
    int res;
    RTMP * const r = &state.rtmp;

    RTMP_Init(r);

    res = RTMP_SetupURL(r, p1_conf.stream.url);
    assert(res == TRUE);

    RTMP_EnableWrite(r);

    res = RTMP_Connect(r, NULL);
    assert(res == TRUE);

    res = RTMP_ConnectStream(r, 0);
    assert(res == TRUE);

    mach_timebase_info(&state.timebase);
    state.start = mach_absolute_time();
}

void p1_stream_video_config(x264_nal_t *nals, int len)
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

    RTMPPacket *pkt = p1_stream_new_packet(RTMP_PACKET_TYPE_VIDEO, 0, tag_size);
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

    int err = RTMP_SendPacket(&state.rtmp, pkt, FALSE);
    free(pkt);
    assert(err == TRUE);
}

void p1_stream_video(x264_nal_t *nals, int len, x264_picture_t *pic)
{
    uint32_t size = 0;
    for (int i = 0; i < len; i++)
        size += nals[i].i_payload;
    const uint32_t tag_size = size + 5;

    RTMPPacket *pkt = p1_stream_new_packet(RTMP_PACKET_TYPE_VIDEO, pic->i_dts, tag_size);
    char * const body = pkt->m_body;

    body[0] = (pic->b_keyframe ? 0x10 : 0x20) | 0x07; // keyframe/IDR, AVC
    body[1] = 1; // AVC NALU
    // skip composition time

    memcpy(body + 5, nals[0].p_payload, size);

    p1_stream_submit_packet(pkt);
}

void p1_stream_audio_config()
{
    const uint32_t tag_size = 2 + 2;

    RTMPPacket *pkt = p1_stream_new_packet(RTMP_PACKET_TYPE_AUDIO, 0, tag_size);
    char * const body = pkt->m_body;

    body[0] = 0xa0 | 0x0c | 0x02 | 0x01; // AAC, 44.1kHz, 16-bit, Stereo
    body[1] = 0; // AAC config

    // Low Complexity profile, 44.1kHz, Stereo
    body[2] = 0x10 | 0x02;
    body[3] = 0x10;

    int err = RTMP_SendPacket(&state.rtmp, pkt, FALSE);
    free(pkt);
    assert(err == TRUE);
}

void p1_stream_audio(AudioQueueBufferRef buf, int64_t mtime)
{
    const uint32_t size = buf->mAudioDataByteSize;
    const uint32_t tag_size = 2 + size;

    RTMPPacket *pkt = p1_stream_new_packet(RTMP_PACKET_TYPE_AUDIO, mtime, tag_size);
    char * const body = pkt->m_body;

    body[0] = 0xa0 | 0x0c | 0x02 | 0x01; // AAC, 44.1kHz, 16-bit, Stereo
    body[1] = 1; // AAC raw

    memcpy(body + 2, buf->mAudioData, size);

    p1_stream_submit_packet(pkt);
}

static RTMPPacket *p1_stream_new_packet(uint8_t type, int64_t time, uint32_t body_size)
{
    const size_t prelude_size = sizeof(RTMPPacket) + RTMP_MAX_HEADER_SIZE;

    // Allocate packet memory.
    RTMPPacket *pkt = calloc(1, prelude_size + body_size);
    assert(pkt != NULL);

    // Fill basic fields.
    pkt->m_packetType = type;
    pkt->m_nChannel = 0x04;
    pkt->m_nInfoField2 = state.rtmp.m_stream_id;
    pkt->m_nBodySize = body_size;
    pkt->m_body = (char *)pkt + prelude_size;

    // Set timestamp, if one was given.
    if (time) {
        // Relative time.
        time -= state.start;
        // Convert to milliseconds.
        time = time * state.timebase.numer / state.timebase.denom / 1000000;
        // x264 may have a couple of frames with negative time.
        if (time < 0) time = 0;
        // Wrap when we exceed 32-bits.
        pkt->m_nTimeStamp = (uint32_t) (time & 0x7fffffff);
    }

    // Start stream with a large header.
    pkt->m_headerType = time ? RTMP_PACKET_SIZE_MEDIUM : RTMP_PACKET_SIZE_LARGE;

    return pkt;
}

static void p1_stream_submit_packet(RTMPPacket *pkt)
{
    int err;
    RTMP * const r = &state.rtmp;

    // This algorithm assumes there will only ever be two types of packets in
    // the queue. In our case, we only queue audio and video packets.

    // Queue is empty, always queue the packet.
    if (state.q_len == 0)
        goto queue;

    // Already queuing packets of this type.
    if (state.q[state.q_start]->m_packetType == pkt->m_packetType)
        goto queue;

    // Dequeue all packets of other types that come before this one.
    while (state.q_len) {
        RTMPPacket *dequeue_pkt = state.q[state.q_start];

        // FIXME: doesn't account for wrapping
        if (dequeue_pkt->m_nTimeStamp > pkt->m_nTimeStamp) break;

        err = RTMP_SendPacket(r, dequeue_pkt, FALSE);
        free(dequeue_pkt);
        assert(err == TRUE);

        state.q_start = (state.q_start + 1) % max_queue_len;
        state.q_len--;
    }

    // Queue is empty again, queue the packet.
    if (state.q_len == 0)
        goto queue;

    // Other types still waiting, so we can send this immediately.
    err = RTMP_SendPacket(r, pkt, FALSE);
    free(pkt);
    assert(err == TRUE);
    return;

queue:
    if (state.q_len == max_queue_len) {
        printf("A/V desync, dropping packet!\n");
        free(pkt);
    }
    else {
        size_t pos = (state.q_start + state.q_len++) % max_queue_len;
        state.q[pos] = pkt;
    }
}
