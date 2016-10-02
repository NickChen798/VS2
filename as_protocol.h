/*!
 *  @file       as_protocol.h
 *  @version    1.0
 *  @date       08/30/2012
 *  @author     Jacky Hsu <jacky.hsu@altasec.com>
 *  @note       AS Data Transmission Protocol
 *              Copyright (C) 2012 ALTASEC Corp.
 */

#ifndef __AS_DTP_H__
#define __AS_DTP_H__

#ifndef u8
#define u8      unsigned char
#endif

#ifndef u16
#define u16     unsigned short
#endif

#ifndef u32
#define u32     unsigned int
#endif

#ifndef u64
#define u64     unsigned long long
#endif

// ----------------------------------------------------------------------------
// Ethernet packet definition (Big Endian)
// ----------------------------------------------------------------------------
#define ASC_PSIZE       16
#define ASC_ETH_TYPE    0x5343
#define ASC_MARKER      0x0681
typedef struct {

    u8      dest[6];
    u8      src[6];
    u16     eth_type;
    u16     marker;     // for check and 4-byte alignment

} asc_packet_t;

// ----------------------------------------------------------------------------
// AS Command packet definition (Little Endian)
// ----------------------------------------------------------------------------

#define CMD_SERV_PORT   8008
#define CMD_DEV_PORT    8009
#define DATA_PORT       8010

#define ASCMD_MAGIC     0x41436d64  // 'ACmd'
#define ASCMD_VERSION   0x20120830
#define ASCMD_PAVO		0x80658679
#define ASCMD_TLVO		0x84768679
#define ASCMD_ONVO		0x79788079

#define ASCMD_MASK              0x0fff
#define ASCMD_GRP_MASK          0xf000

#define ASCMD_S2D               0x1000
#define ASCMD_S2D_DISCOVERY     0x1000      // flag = 0: normal, 1:hello_ex
#define ASCMD_S2D_SETUP         0x1001      // flag = N/A
#define ASCMD_S2D_UPGRADE       0x1002
#define ASCMD_S2D_RESET         0x1003
#define ASCMD_S2D_STREAM        0x1004      // flag = b15=status, b14=audio, b13=meta, b12=video, b7-b4=ch, b3-b0=stream (if b14 is set)
#define ASCMD_S2D_DO            0x1005      // flag = DO status : b0 = DO1, b1 = DO2 (b8-b15 mask)
#define ASCMD_S2D_FTEST         0x1006      // flag = 0 test only, 1 program
#define ASCMD_S2D_AUDIO_OUT     0x1007      // flag = 0 raw ADPCM data, flag = 1 FH + data
#define ASCMD_S2D_SETUP_EX      0x1008
#define ASCMD_S2D_BT_RFCOMM     0x1009
#define ASCMD_S2D_BT_STATUS     0x100a
#define ASCMD_S2D_RING_CTRL     0x100b      // flag = on/off
#define ASCMD_S2D_ISP           0x100c      // flag = # of reg(2)/value(2) pairs
#define ASCMD_S2D_RS485         0x100d
#define ASCMD_S2D_ROI_MODE      0x100e
#define ASCMD_S2D_USERDATA      0x100f      // flag = 0:get, 1:set
#define ASCMD_S2D_NUM           0x0010
#define ASCMD_S2D_VALID(X)      ((((X) & ASCMD_GRP_MASK) == ASCMD_S2D) && (((X) & ASCMD_MASK) < ASCMD_S2D_NUM))

#define ASCMD_S2D_STREAM_STATUS     0x80
#define ASCMD_S2D_STREAM_AUDIO      0x40
#define ASCMD_S2D_STREAM_META       0x20
#define ASCMD_S2D_STREAM_VIDEO      0x10
#define ASCMD_S2D_STREAM_MOTION     0x01
#define ASCMD_S2D_STREAM_MOTION_ONLY(X) (((X) & 0x100) ? 1 : 0)
#define ASCMD_S2D_STREAM_MODE(X)    (((X) & 0xfe00) >> 8)
#define ASCMD_S2D_STREAM_CH(X)      (((X) & 0x00f0) >> 4)
#define ASCMD_S2D_STREAM_ID(X)      ((X) & 0x000f)

#define ASCMD_D2S               0x2000
#define ASCMD_D2S_HELLO         0x2000      // flag = N/A
#define ASCMD_D2S_FTEST         0x2001
#define ASCMD_D2S_HELLO_EX      0x2002
#define ASCMD_D2S_BT_RFCOMM     0x2003      // flag: 0=received, -1=invalid, -2=busy
#define ASCMD_D2S_BT_STATUS     0x2004      // flag: 0=idle, 1=connecting, 2=sending, 3=done, -1=socket failed, -2=conn failed, -3=send failed
#define ASCMD_D2S_USERDATA      0x2005      // flag = 0:get ok, 1:set ok, 0xffff:failed
#define ASCMD_D2S_NUM           0x0006
#define ASCMD_D2S_VALID(X)      ((((X) & ASCMD_GRP_MASK) == ASCMD_D2S) && (((X) & ASCMD_MASK) < ASCMD_D2S_NUM))

// MFTEST
#define ASCMD_MFTEST            0x3000      // low byte is test id, flag = param/status
#define ASCMD_IS_MFTEST(X)      (((X) & ASCMD_GRP_MASK) == ASCMD_MFTEST)

// USER DATA
#define ASC_USERDATA_SIZE       4088

typedef struct __attribute__ ((packed)) {

    u32     magic;
    u32     version;
    u16     cmd;
    u16     flag;
    u32     len;

} as_cmd_t;

typedef struct __attribute__ ((packed)) {

    u8      vtype;
    u8      fps;
    u8      quality;
    u8      audio;
    u16     width;
    u16     height;
    u32     bitrate;

} as_enc_setup_t;


#define ASCMD_S2D_ID_FORCE      0xff
#define ASCMD_S2D_ID_ALL        0x00

typedef struct __attribute__ ((packed)) {

    as_cmd_t        cmd;

    u32     network;    // Device IP if .id = 0xff
    u8      id;         // 255:force, 1-254: setup single device, 0:setup all devices
    u8      streams;    // 1-2, 0 = no change
    u8      mac[6];     // target MAC, 0 or ff means ALL
    u32     gateway;    // only valid when id=255
    u32     netmask;    // only valid when id=255
    u32     dns;        // only valid when id=255
    u8      resv[16];   // reserved for authentication

    as_enc_setup_t  enc1;
    as_enc_setup_t  enc2;

} asc_s2d_setup_t;

typedef struct __attribute__ ((packed)) {

    as_cmd_t        cmd;

    u8      mac[6];
    char    serial_number[32];
    u8      link;       // b0 port0, b1 port1, b2 vin0
    u8      dio;        // b7 status led
    u8      id;
    u8      resv[3];

    u16     mode;           // @sa ASS_MODE_xxx
    u16     status;         // mode=1: progress %, mode=2: upgrade return code @sa ASS_STATUS_xxx

    u8      resv2[60];

} asc_ftest_t;

#define AS_MODEL_HS07   0
#define AS_TYPE_D1      0
#define AS_TYPE_720P    1
#define AS_TYPE_1080P   2

typedef struct __attribute__ ((packed)) {

    as_cmd_t    cmd;

    u32     fw_ver;
    u32     fw_build;
    u8      model;
    u8      type;
    u8      mac[6];
    u32     ip;
    u8      id;
    u8      streams;
    u8      resv[2];

    as_enc_setup_t  enc1;
    as_enc_setup_t  enc2;

} asc_d2s_hello_t;

#define ASF_MAGIC       0x6153664d  // 'aSfM'

#define ASF_TYPE_VIDEO      1
#define ASF_TYPE_AUDIO      2
#define ASF_TYPE_META       3
#define ASF_TYPE_STATUS     4

#define ASF_VTYPE_H264      0x11
#define ASF_VTYPE_MJPEG     0x12
#define ASF_VTYPE_YUV420    0x13
#define ASF_VTYPE_GRAY      0x14

#define ASF_ATYPE_G711A         0x21
#define ASF_ATYPE_G711U         0x22
#define ASF_ATYPE_G726          0x23
#define ASF_ATYPE_ADPCM_IMA     0x24
#define ASF_ATYPE_ADPCM_DVI4    0x25

#define ASF_FLAG_KEY            0x80
#define ASF_FLAG_MOTION         0x20
#define ASF_FLAG_VLOSS          0x10
#define ASF_FLAG_DO2            0x08
#define ASF_FLAG_DO1            0x04
#define ASF_FLAG_DI2            0x02
#define ASF_FLAG_DI1            0x01

typedef struct __attribute__ ((packed)) {

    u32     magic;
    u32     size;
    u32     sec;
    u32     usec;
    u8      type;           // video, audio, or meta
    u8      sub_type;       // VTYPE or ATYPE
    u8      padding;
    u8      flags;          // 0x80: key frame
    u16     width;          // bitwidth for audio
    u16     height;         // sample rate for audio

} as_frame_t;

#define ASYF_MAGIC       0x61537946  // 'aSyF'
typedef struct __attribute__ ((packed)) {

    u32     magic;
    u32     size;
    u32     sec;
    u32     usec;
    u8      id;             // ROI id
    u8      flags;
    u8      resv[2];
    u16     width;          // bitwidth for audio
    u16     height;         // sample rate for audio
    u16     x;
    u16     y;

} as_yframe_t;

#define ASS_MODE_NORMAL     0
#define ASS_MODE_UPGRADING  1
#define ASS_MODE_UPGRADED   2

#define ASS_STATUS_OK           0
#define ASS_STATUS_RECV_ERR     0xf0
#define ASS_STATUS_WRONG_FILE   0xf1
#define ASS_STATUS_FAILED       0xf2

typedef struct __attribute__ ((packed)) {

    u32     magic;
    u32     size;
    u32     sec;
    u32     usec;
    u8      type;           // ASF_TYPE_STATUS
    u8      sub_type;       // N/A
    u8      padding;
    u8      flags;          // @sa ASF_FLAG_xxx
    u16     mode;           // @sa ASS_MODE_xxx
    u16     status;         // mode=1: progress %, mode=2: upgrade return code @sa ASS_STATUS_xxx

} as_status_t;

// -----------------------------------------------------------------------------
#define ASCMD_CONFIG_NETWORK    0x11
#define ASCMD_CONFIG_ENCODER    0x12
#define ASCMD_CONFIG_STUN       0x13
#define ASCMD_CONFIG_ROI        0x14
#define ASCMD_CONFIG_ISP        0x15

#define ASCMD_CONFIG_SET        0x01
#define ASCMD_CONFIG_SAVE       0x02

typedef struct __attribute__ ((packed)) {

    u32     ip;
    u32     gateway;
    u32     netmask;
    u32     dns;

} asc_config_network_t;

typedef struct __attribute__ ((packed)) {

    u8      ch;
    u8      stream;
    u8      vtype;
    u8      fps;
    u16     x;
    u16     y;
    u16     width;
    u16     height;
    u32     bitrate;

} asc_config_encoder_t;

#define ASC_STUN_NR         4
#define ASC_STUN_TYPE_OBS   0x11
#define ASC_STUN_TYPE_STD   0x22

typedef struct __attribute__ ((packed)) {

    u32     server[ASC_STUN_NR];
    u8      type[ASC_STUN_NR];
    u16     interval;
    u16     flag;
    u8      resv[16];

} asc_config_stun_t;

#define ASC_ROI_NUM     4
typedef struct __attribute__ ((packed)) {

    u8  enable[ASC_ROI_NUM];
    struct {
        u16 top;
        u16 left;
        u16 bottom;
        u16 right;
    } regions[ASC_ROI_NUM];

} asc_config_roi_t;

#define ASC_ISP_NUM     8
typedef struct __attribute__ ((packed)) {

    u16 addr[ASC_ISP_NUM];
    u8  data[ASC_ISP_NUM];

} asc_config_isp_t;

typedef struct __attribute__ ((packed)) {

    u8      id;
    u8      flag;
    u16     size;

    u8      data[0];

} asc_config_ex_t;

typedef struct __attribute__ ((packed)) {

    as_cmd_t        cmd;

    u8      mac[6];     // target MAC, 0 or ff means ALL
    u8      resv[26];   // reserve for key

    u8      data[0];    // asc_config_ex_t

} asc_s2d_setup_ex_t;

typedef struct __attribute__ ((packed)) {

    as_cmd_t    cmd;

    u32     fw_ver;
    u32     fw_build;
    char    serial_number[32];
    u8      mac[6];
    u8      model;
    u8      type;
    u8      ver;
    u8      id;
    u8      link;       // b0 port0, b1 port1, b2 vin0
    u8      dio;        // b7 status led
    u16     mode;       // @sa ASS_MODE_xxx
    u16     status;     // mode=1: progress %, mode=2: upgrade return code @sa ASS_STATUS_xxx

    u8      data[0];    // asc_config_ex_t

} asc_d2s_hello_ex_t;

// -----------------------------------------------------------------------------
typedef struct __attribute__ ((packed)) {

    as_cmd_t        cmd;        // [0-15]

    u8      mac[6];             // [16-21] target MAC, 0 or ff means ALL
    u8      target_resv[66];    // [22-87]

    // Network
    u8      set_network;        // [88-88] b7: save, b1: set
    u8      network_ver;        // [89-89]
    u8      network_resv[2];    // [90-91]
    u32     ip;                 // [92-95]
    u32     gateway;            // [96-99]
    u32     netmask;            // [100-103]
    u32     dns;                // [104-107]
    u32     stun_server[2];     // [108-115]
    u16     stun_duration;      // [116-117]
    u8      network_resv2[62];  // [118-179]

    u8      set_encoder;        // [180-180] b7: save, b1: set
    u8      enc_ver;            // [181-181]
    u8      enc_resv[2];        // [182-183]
    as_enc_setup_t  enc1;       // [184-195]
    as_enc_setup_t  enc2;       // [196-207]
    u8      enc_resv2[64];      // [208-271]

} asc_s2d_setup2_t;

typedef struct __attribute__ ((packed)) {

    as_cmd_t    cmd;

    u32     fw_ver;
    u32     fw_build;
    u8      model;
    u8      type;
    u8      ver;
    u8      id;
    u8      mac[6];
    u8      link;       // b0 port0, b1 port1, b2 vin0
    u8      dio;        // b7 status led
    char    serial_number[32];

    u32     ip;
    u32     netmask;
    u32     gateway;
    u32     dns;
    u32     stun_server[2];
    u32     wan_ip;
    u16     wan_port;
    u16     stun_duration;

    u16     mode;           // @sa ASS_MODE_xxx
    u16     status;         // mode=1: progress %, mode=2: upgrade return code @sa ASS_STATUS_xxx

    as_enc_setup_t  enc1;
    as_enc_setup_t  enc2;

} asc_d2s_hello2_t;

typedef struct __attribute__ ((packed)) {

    as_cmd_t cmd;

    u8      mac[6];
    u8      channel;
    u8      pin[9];
    u16     size;
    u8      data[0];

} asc_bt_rfcomm_t;

#endif // __AS_DTP_H__

