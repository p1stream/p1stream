#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <mach/mach_time.h>
#include <rtmp.h>

#include "stream.h"

static struct {
    RTMP rtmp;
    mach_timebase_info_data_t timebase;
    uint64_t start;
} state;

static uint32_t p1_stream_format_time(int64_t time);


void p1_stream_init(const char *c_url)
{
    int res;
    RTMP * const r = &state.rtmp;

    mach_timebase_info(&state.timebase);

    RTMP_Init(r);

    char *url = strdup(c_url);
    res = RTMP_SetupURL(r, url);
    free(url);
    assert(res == TRUE);

    RTMP_EnableWrite(r);

    res = RTMP_Connect(r, NULL);
    assert(res == TRUE);

    res = RTMP_ConnectStream(r, 0);
    assert(res == TRUE);

    state.start = mach_absolute_time();
}

void p1_stream_video_config(x264_nal_t *nals, int len)
{
    RTMP * const r = &state.rtmp;
    RTMPPacket * const pkt = &r->m_write;
    int err;
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

    pkt->m_headerType = RTMP_PACKET_SIZE_LARGE;
    pkt->m_packetType = RTMP_PACKET_TYPE_VIDEO;
    pkt->m_nChannel = 0x04;
    pkt->m_nTimeStamp = 0;
    pkt->m_nInfoField2 = r->m_stream_id;
    pkt->m_nBodySize = tag_size;

    err = RTMPPacket_Alloc(pkt, tag_size);
    assert(err == TRUE);
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

    err = RTMP_SendPacket(r, pkt, FALSE);
    RTMPPacket_Free(pkt);
    assert(err == TRUE);
}

void p1_stream_video(x264_nal_t *nals, int len, x264_picture_t *pic)
{
    RTMP * const r = &state.rtmp;
    RTMPPacket * const pkt = &r->m_write;
    int err;

    uint32_t size = 0;
    for (int i = 0; i < len; i++)
        size += nals[i].i_payload;

    const uint32_t tag_size = size + 5;

    pkt->m_headerType = RTMP_PACKET_SIZE_MEDIUM;
    pkt->m_packetType = RTMP_PACKET_TYPE_VIDEO;
    pkt->m_nChannel = 0x04;
    pkt->m_nTimeStamp = p1_stream_format_time(pic->i_dts);
    pkt->m_nInfoField2 = r->m_stream_id;
    pkt->m_nBodySize = tag_size;

    err = RTMPPacket_Alloc(pkt, tag_size);
    assert(err == TRUE);
    char * const body = pkt->m_body;

    body[0] = (pic->b_keyframe ? 0x10 : 0x20) | 0x07; // keyframe/IDR, AVC
    body[1] = 1; // AVC NALU
    // skip composition time

    memcpy(body + 5, nals[0].p_payload, size);

    err = RTMP_SendPacket(r, pkt, FALSE);
    RTMPPacket_Free(pkt);
    assert(err == TRUE);
}

void p1_stream_audio_config()
{
    RTMP * const r = &state.rtmp;
    RTMPPacket * const pkt = &r->m_write;
    int err;

    const uint32_t tag_size = 2 + 2;

    pkt->m_headerType = RTMP_PACKET_SIZE_LARGE;
    pkt->m_packetType = RTMP_PACKET_TYPE_AUDIO;
    pkt->m_nChannel = 0x04;
    pkt->m_nTimeStamp = 0;
    pkt->m_nInfoField2 = r->m_stream_id;
    pkt->m_nBodySize = tag_size;

    err = RTMPPacket_Alloc(pkt, tag_size);
    assert(err == TRUE);
    char * const body = pkt->m_body;

    body[0] = 0xa0 | 0x0c | 0x02 | 0x01; // AAC, 44.1kHz, 16-bit, Stereo
    body[1] = 0; // AAC config

    // Low Complexity profile, 44.1kHz, Stereo
    body[2] = 0x10 | 0x02;
    body[3] = 0x10;

    err = RTMP_SendPacket(r, pkt, FALSE);
    RTMPPacket_Free(pkt);
    assert(err == TRUE);
}

void p1_stream_audio(AudioQueueBufferRef buf, int64_t time)
{
    RTMP * const r = &state.rtmp;
    RTMPPacket * const pkt = &r->m_write;
    int err;

    const uint32_t size = buf->mAudioDataByteSize;
    const uint32_t tag_size = 2 + size;

    pkt->m_headerType = RTMP_PACKET_SIZE_MEDIUM;
    pkt->m_packetType = RTMP_PACKET_TYPE_AUDIO;
    pkt->m_nChannel = 0x04;
    pkt->m_nTimeStamp = p1_stream_format_time(time);
    pkt->m_nInfoField2 = r->m_stream_id;
    pkt->m_nBodySize = tag_size;

    err = RTMPPacket_Alloc(pkt, tag_size);
    assert(err == TRUE);
    char * const body = pkt->m_body;

    body[0] = 0xa0 | 0x0c | 0x02 | 0x01; // AAC, 44.1kHz, 16-bit, Stereo
    body[1] = 1; // AAC raw

    memcpy(body + 2, buf->mAudioData, size);

    err = RTMP_SendPacket(r, pkt, FALSE);
    RTMPPacket_Free(pkt);
    assert(err == TRUE);
}

static uint32_t p1_stream_format_time(int64_t time) {
    // Relative time.
    time -= state.start;
    // Convert to milliseconds.
    time = time * state.timebase.numer / state.timebase.denom / 1000000;
    // Wrap when we exceed 32-bits.
    return (uint32_t) (time & 0x7fffffff);
}
