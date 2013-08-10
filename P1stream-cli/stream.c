#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <rtmp.h>

#include "stream.h"

static struct {
    RTMP rtmp;
} state;


void p1_stream_init(const char *c_url)
{
    int res;
    RTMP * const r = &state.rtmp;

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
}

void p1_stream_video_config(x264_nal_t *nals, int len)
{
    // The following is based on GStreamer code and RTMP_Write.

    RTMP * const r = &state.rtmp;
    RTMPPacket *pkt = &r->m_write;
    int err;
    int i;

    x264_nal_t *nal_sps, *nal_pps;
    for (i = 0; i < len; i++) {
        x264_nal_t * const nal = &nals[i];
        switch (nal->i_type) {
            case NAL_SPS: nal_sps = nal; break;
            case NAL_PPS: nal_pps = nal; break;
        }
    }

    int sps_size = nal_sps->i_payload-4;
    int pps_size = nal_pps->i_payload-4;
    uint32_t size = 11 + sps_size + pps_size;
    uint32_t tag_size = 5 + size;

    pkt->m_headerType = RTMP_PACKET_SIZE_LARGE;
    pkt->m_packetType = RTMP_PACKET_TYPE_VIDEO;
    pkt->m_nChannel = 0x04;
    pkt->m_nTimeStamp = 0;
    pkt->m_nInfoField2 = r->m_stream_id;
    pkt->m_nBodySize = size;

    err = RTMPPacket_Alloc(pkt, tag_size);
    assert(err == TRUE);
    char * const body = pkt->m_body;

    body[0] = 16 | 7;
    body[1] = 0;
    // skip stream id

    uint8_t *sps = nal_sps->p_payload + 4;
    /* skip NAL unit type */
    sps++;

    body[5] = 1;                /* AVC Decoder Configuration Record ver. 1 */
    body[6] = sps[0];           /* profile_idc                             */
    body[7] = sps[1];           /* profile_compability                     */
    body[8] = sps[2];           /* level_idc                               */
    body[9] = 0xfc | (4 - 1);   /* nal_length_size_minus1                  */

    i = 10;

    body[i++] = 0xe0 | 1; /* number of SPSs */
    *(uint16_t *) (body+i) = htons(sps_size);
    memcpy(body+i+2, nal_sps->p_payload+4, sps_size);

    i += sps_size + 2;

    body[i++] = 1; /* number of PPSs */
    *(uint16_t *) (body+i) = htons(pps_size);
    memcpy(body+i+2, nal_pps->p_payload+4, pps_size);

    err = RTMP_SendPacket(r, pkt, FALSE);
    RTMPPacket_Free(pkt);
    assert(err == TRUE);
}

void p1_stream_video(x264_nal_t *nals, int len, x264_picture_t *pic)
{
    RTMP * const r = &state.rtmp;
    int err;

    uint32_t size = 0;
    for (int i = 0; i < len; i++)
        size += nals[i].i_payload;

    // Based on RTMP_Write.
    RTMPPacket *pkt = &r->m_write;
    const uint32_t tag_size = size + 5;
    // Wrap the timestamp when it exceeds the 32-bit field
    uint32_t time = pic->i_pts & 0x7fffffff;

    pkt->m_headerType = time == 0 ? RTMP_PACKET_SIZE_LARGE : RTMP_PACKET_SIZE_MEDIUM;
    pkt->m_packetType = RTMP_PACKET_TYPE_VIDEO;
    pkt->m_nChannel = 0x04;
    pkt->m_nTimeStamp = time;
    pkt->m_nInfoField2 = r->m_stream_id;
    pkt->m_nBodySize = size;

    err = RTMPPacket_Alloc(pkt, tag_size);
    assert(err == TRUE);
    char * const body = pkt->m_body;

    body[0] = (pic->b_keyframe ? 16 : 0) | 7;
    body[1] = 1;
    // skip stream id

    memcpy(body + 5, nals[0].p_payload, size);

    err = RTMP_SendPacket(r, pkt, FALSE);
    RTMPPacket_Free(pkt);
    assert(err == TRUE);
}
