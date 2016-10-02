/*!
 *  @file       obs_stun.h
 *  @version    1.0
 *  @date       08/27/2013
 *  @author     Jacky Hsu <sch0914@gmail.com>
 *  @note       Orbit Stun Portocol
 *              Copyright (C) 2013 Orbit Corp.
 */

#ifndef __OBS_STUN_H__
#define __OBS_STUN_H__

#include "platform.h"

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

#if defined(WIN32) || defined(_WIN32)
#pragma pack(push)
#pragma pack(1)
#endif

#define OBS_STUN_PORT           9791

#define OBST_MAGIC              0x4F625333  // 'ObS3'
#define OBST_STUN_REQ           0x10000001  // param = obst_stun_t
#define OBST_GET_TURN           0x10000003  // param = obst_turn_t
#define OBST_DEV_REG            0x10000004  // param = obst_dev_t
#define OBST_CONN_REQ           0x10000005  // param = obst_conn_t
#define OBST_TURN_REG           0x10000006  // param = obst_turn_reg_t
#define OBST_TURN_UPDATE        0x10000007  // param = obst_turn_update_t
#define OBST_STUN_SERV          0x10000008  // param[0] = ip, param[1] = port

typedef struct JH_PACKED {

    u32     magic;
    u32     cmd;
    u32     param[8];

} obst_cmd_t;

typedef struct JH_PACKED {

    u32     ip;
    u16     port;
    u8      mac[6];

} obst_stun_t;

typedef struct JH_PACKED {

    u32     region_id;
    u32     ip;
    u16     port;

} obst_turn_t;

typedef struct JH_PACKED {

    u32     session;
    u32     ip;
    u16     port;

} obst_turn_reg_t;

// TURN node commands
typedef struct JH_PACKED {

    u32     session;
    u32     magic;
    u32     cmd;
    u32     param[8];

} obst_turn_cmd_t;

typedef struct JH_PACKED {

    u32     ip;
    u16     port;
    u8      peer_id;
    u8      flags;

} obst_tc_update_t;

#define OBST_DEV_VS1        0x56533120  // 'VS1 '
#define OBST_DEV_TURN       0x4f42544e  // 'OBTN'
typedef struct JH_PACKED {

    u32     dev_id;
    u32     pub_ip;
    u16     pub_port;
    u8      mac[6];
    u32     attr[4];        // [0] vs:fw_ver, turn:region id
                            // [1] turn: max
                            // [2] turn: used

} obst_dev_t;

#define OBST_CONN_NR    2
#define OBST_CONN_ALL   0xffffffff
#define OBST_CONN_ON    1
#define OBST_STATUS_CONN_FAILED     0x80000000
#define OBST_STATUS_CONN_FULL       0x80000001
#define OBST_STATUS_CONN_TIMEOUT    0x80000002
#define OBST_STATUS_CONN_NOWIP      0x80000003
typedef struct JH_PACKED {

    u32     peer_id;
    u32     peer_ip[OBST_CONN_NR];
    u16     peer_port[OBST_CONN_NR];
    u32     param;              // b0=on/off
    u32     status;

} obst_conn_t;

// -----------------------------------------------------------------------------
#if defined(WIN32) || defined(_WIN32)
#pragma pack(pop)
#endif

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // __OBS_STUN_H__

