/*!
 *  @file       obs_protocol.h
 *  @version    1.0
 *  @date       06/30/2013
 *  @author     Jacky Hsu <sch0914@gmail.com>
 *  @note       Orbit Stream Portocol
 *              Copyright (C) 2013 Orbit Corp.
 */

#ifndef __OBS_PROTOCOL_H__
#define __OBS_PROTOCOL_H__

#include "platform.h"

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

#if defined(WIN32) || defined(_WIN32)
#pragma pack(push)
#pragma pack(1)
#endif

#define OBSP_MAGIC              0x4F625350  // 'ObSP'
#define OBSP_PEER_HS            0x20000003  // Hand Shake, param: 0=SYNC, 1=SYNC+ACK 2=ACK
#define OBSP_PEER_FIN           0x20000004  // Peer Finalized, param = session
#define OBSP_CMD_STREAM         0x30000001  // Stream Request
#define OBSP_CMD_RESEND         0x30000002  // Resend Request, param = # of packets
#define OBSP_CMD_DIO            0x30000003  // DIO Request/Ack, param = obsp_dio_t
#define OBSP_CMD_AOUT           0x30000004  // Audio Out Request, param = sequence
#define OBSP_CMD_RING_CTRL      0x30000005  // Ring Control, param = on/off
#define OBSP_STAT_SENDER        0x40000001  // Sender Statistic
#define OBSP_STAT_RECEIVER      0x40000002  // Reciever Statistic

// OBSP_PEER_HS
#define OBSP_PEER_SYNC          0   // SYNC
#define OBSP_PEER_SYNC_ACK      1   // SYNC + ACK

// OBSP_PEER_FIN
#define OBSP_FIN_NORMAL         0x80000000
#define OBSP_FIN_TIMEOUT        0x80000001
#define OBSP_FIN_CMD_ERR        0x80000002
#define OBSP_FIN_NO_DATA        0x80000003
#define OBSP_FIN_NO_ACK         0x80000004
#define OBSP_FIN_NET_ERR        0x80000005
#define OBSP_FIN_LEFT           0x80000006

typedef struct JH_PACKED {

    u32     session;        // for TURN mode
    u32     magic;
    u32     cmd;
    u32     param;

} obsp_cmd_t;

typedef struct JH_PACKED {

    obsp_cmd_t  cmd;

    u8      ch;
    u8      id;     // 0x80 = audio

} obsp_stream_req_t;

#define OBS_UNKNOWN             0xff
#define OBS_TYPE_VIDEO          1
#define OBS_TYPE_AUDIO          2
#define OBS_VTYPE_H264          0x11
#define OBS_VTYPE_MJPEG         0x12
#define OBS_VTYPE_VALID(X)      (((X) == OBS_VTYPE_H264) || ((X) == OBS_VTYPE_MJPEG))
#define OBS_ATYPE_G711A         0x21
#define OBS_ATYPE_G711U         0x22
#define OBS_ATYPE_G726          0x23
#define OBS_ATYPE_ADPCM_IMA     0x24
#define OBS_ATYPE_ADPCM_DVI4    0x25
#define OBS_ATYPE_VALID(X)      (((X) >= 0x21) || ((X) <= 0x25))

typedef struct JH_PACKED {

    obsp_cmd_t  cmd;

    u32     session;
    u8      vtype;
    u8      atype;
    u16     width;
    u16     height;
    u16     bitwidth;
    u16     samplerate;

} obsp_stream_attr_t;

typedef struct JH_PACKED {

    obsp_cmd_t  cmd;

    u32     session;
    u32     duration;       // msec
    // video
    u16     v_seq_start;
    u16     v_seq_end;
    u32     vpkt_received;
    u32     vpkt_dropped;
    u32     vframe_received;
    u32     vframe_dropped;
    // audio
    u16     a_seq_start;
    u16     a_seq_end;
    u32     apkt_received;
    u32     apkt_dropped;
    // all
    u32     byte_received;
    u32     byte_dropped;
    u32     preferred_bps;

} obsp_stat_receiver_t;

typedef struct JH_PACKED {

    obsp_cmd_t  cmd;

    u32     session;
    u16     v_seq;
    u16     a_seq;
    u32     duration;       // msec
    u32     pkt_sent;
    u32     pkt_dropped;
    u32     frame_sent;
    u32     frame_dropped;
    u32     byte_sent;
    u32     byte_dropped;

} obsp_stat_sender_t;

// Stream Packet
typedef struct JH_PACKED {

	u32	    turn_session;
	u32	    session;    // must not = OBSP_MAGIC
	u16	    sequence;
    u8      flag;
    u8      status;     // DIO

} obsp_packet_t;

#define OBS_FLAG_RESEND         0x10
#define OBS_FLAG_DIO			0x08
#define OBS_FLAG_AUDIO			0x04
#define OBS_FLAG_KEY			0x02
#define OBS_FLAG_START			0x01

typedef struct JH_PACKED {

	u32     timestamp;		// usec
	u32     size;

} obsp_frame_t;

typedef struct JH_PACKED {

	u8      type;
	u8      num;
    u16     unit_size;

} obsp_audio_t;

typedef struct JH_PACKED {

	u16     check;
	u16     value;

} obsp_dio_t;

// -----------------------------------------------------------------------------
#if defined(WIN32) || defined(_WIN32)
#pragma pack(pop)
#endif

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __OBS_PROTOCOL_H__

