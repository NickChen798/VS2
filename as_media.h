/*!
 *  @file       as_media.h
 *  @version    0.1
 *  @date       10/26/2011
 *  @author     Daniel Tsai <daniel.tsai@altasec.com>
 *  @note       Media Library <br>
 *              Copyright (C) 2011 ALTASEC Corp.
 */

#ifndef __AS_MEDIA_H__
#define __AS_MEDIA_H__

#ifdef __cplusplus
#if __cplusplus
extern "C"
{
#endif
#endif // __cplusplus

// ----------------------------------------------------------------------------
// Include Files
// ----------------------------------------------------------------------------
#include "as_types.h"
#include "as_dm.h"

// ----------------------------------------------------------------------------
// Constant Definition
// ----------------------------------------------------------------------------
// media_return_code_def MEDIA Return Code
#define AS_MEDIA_RC_READY            (0)
#define AS_MEDIA_RC_DNR              (-1)    // Data Not Ready
#define AS_MEDIA_RC_ND               (-2)    // No Data
#define AS_MEDIA_RC_PERR             (-3)    // Param Error
#define AS_MEDIA_RC_BUSY             (-4)    // Busy, try later

enum {
    AS_MEDIA_VIDEO_TYPE_NTSC = 0,
    AS_MEDIA_VIDEO_TYPE_PAL,
    AS_MEDIA_VIDEO_TYPE_END
};

enum {
	AS_MEDIA_VI_TYPE_CCTV = 0,
	AS_MEDIA_VI_TYPE_IPCAM,
	AS_MEDIA_VI_TYPE_HDSDI,
	AS_MEDIA_VI_TYPE_DVBT,
	AS_MEDIA_VI_TYPE_END
};

enum {
	AS_MEDIA_VO_TYPE_HDMI = 0,
	AS_MEDIA_VO_TYPE_VGA,
	AS_MEDIA_VO_TYPE_CVBS,
	AS_MEDIA_VO_TYPE_BT1120,
	AS_MEDIA_VO_TYPE_BT656,
	AS_MEDIA_VO_TYPE_END
};

enum {
	AS_MEDIA_VO_FMT_1080P_60 = 0,
	AS_MEDIA_VO_FMT_1080P_50,
	AS_MEDIA_VO_FMT_1080P_30,
	AS_MEDIA_VO_FMT_1080P_25,
	AS_MEDIA_VO_FMT_1080P_24,
	AS_MEDIA_VO_FMT_1080I_60,
	AS_MEDIA_VO_FMT_1080I_50,
	AS_MEDIA_VO_FMT_720P_60,
	AS_MEDIA_VO_FMT_720P_50,
	AS_MEDIA_VO_FMT_NTSC,
	AS_MEDIA_VO_FMT_PAL,
	AS_MEDIA_VO_FMT_640X480_60,
	AS_MEDIA_VO_FMT_800X600_60,
	AS_MEDIA_VO_FMT_1024X768_60,
	AS_MEDIA_VO_FMT_1280X1024_60,
	AS_MEDIA_VO_FMT_1366X768_60,
	AS_MEDIA_VO_FMT_1440X900_60,
	AS_MEDIA_VO_FMT_1600X1200_60,
	AS_MEDIA_VO_FMT_END
};

enum {
	AS_MEDIA_VENC_TYPE_H264_BP = 0,
	AS_MEDIA_VENC_TYPE_H264_MP,
	AS_MEDIA_VENC_TYPE_H264_HP,
	AS_MEDIA_VENC_TYPE_MPEG4_SP,
	AS_MEDIA_VENC_TYPE_MJPEG,
	AS_MEDIA_VENC_TYPE_END
};

enum {
	AS_MEDIA_VENC_RES_FHD = 0,
	AS_MEDIA_VENC_RES_HD,
	AS_MEDIA_VENC_RES_QFHD,
	AS_MEDIA_VENC_RES_QHD,
	AS_MEDIA_VENC_RES_960H,
    AS_MEDIA_VENC_RES_D1,
	AS_MEDIA_VENC_RES_VGA,
	AS_MEDIA_VENC_RES_HFHD,
	AS_MEDIA_VENC_RES_NHD,
	AS_MEDIA_VENC_RES_HHD,
    AS_MEDIA_VENC_RES_2CIF,
    AS_MEDIA_VENC_RES_CIF,
    AS_MEDIA_VENC_RES_QCIF,
    AS_MEDIA_VENC_RES_END
};

enum {
	AS_MEDIA_VENC_RC_CBR = 0,
	AS_MEDIA_VENC_RC_VBR,
	AS_MEDIA_VENC_RC_FIXQP,
	AS_MEDIA_VENC_RC_END
};

enum {
	AS_MEDIA_VDEC_TYPE_H264 = 0,
	AS_MEDIA_VDEC_TYPE_MPEG4,
	AS_MEDIA_VDEC_TYPE_MJPEG,
	AS_MEDIA_VDEC_TYPE_END
};

enum {
	AS_MEDIA_VAD_TYPE_BRIGHTNESS = 0,
	AS_MEDIA_VAD_TYPE_CONTRAST,
	AS_MEDIA_VAD_TYPE_SATURATION,
	AS_MEDIA_VAD_TYPE_HUE,
	AS_MEDIA_VAD_TYPE_END
};

enum {
	AS_MEDIA_VID_H264 		= 0x00,
	AS_MEDIA_VID_H264_BP 	= 0x01,
	AS_MEDIA_VID_H264_MP 	= 0x02,
	AS_MEDIA_VID_H264_HP 	= 0x03,
	AS_MEDIA_VID_MP4 		= 0x10,
	AS_MEDIA_VID_MP4_SP 	= 0x11,
	AS_MEDIA_VID_MP4_ASP 	= 0x12,
	AS_MEDIA_VID_MP4_SH 	= 0x13,
	AS_MEDIA_VID_MP2 		= 0x20,
	AS_MEDIA_VID_MP2_MP 	= 0x21,
	AS_MEDIA_VID_MP2_SP 	= 0x22,
	AS_MEDIA_VID_MJPEG  	= 0x30,
	AS_MEDIA_VID_UNKNOWN 	= 0xff
};

enum {
	AS_MEDIA_PT_PCMU        = 0,         // A  8000  [RFC3551]
	AS_MEDIA_PT_GSM         = 3,         // A  8000  [RFC3551]
	AS_MEDIA_PT_G723        = 4,         // A  8000  [Kumar][RFC3551]
	AS_MEDIA_PT_DVI4        = 5,         // A  8000  [RFC3551]
	AS_MEDIA_PT_DVI4_16     = 6,         // A  16000 [RFC3551]
	AS_MEDIA_PT_LPC         = 7,         // A  8000  [RFC3551]
	AS_MEDIA_PT_PCMA        = 8,         // A  8000  [RFC3551]
	AS_MEDIA_PT_G722        = 9,         // A  8000  [RFC3551]
	AS_MEDIA_PT_L16         = 10,        // A  44100 [RFC3551]
	AS_MEDIA_PT_L16_2       = 11,        // A  44100 [RFC3551]
	AS_MEDIA_PT_QCELP       = 12,        // A  8000  [RFC3551]
	AS_MEDIA_PT_CN          = 13,        // A  8000  [RFC3389]
	AS_MEDIA_PT_MPA         = 14,        // A  90000 [RFC3551][RFC2250]
	AS_MEDIA_PT_G728        = 15,        // A  8000  [RFC3551]
	AS_MEDIA_PT_DVI4_11     = 16,        // A  11025 [DiPol]
	AS_MEDIA_PT_DVI4_22     = 17,        // A  22050 [DiPol]
	AS_MEDIA_PT_G729        = 18,        // A  8000  [RFC3551]
	AS_MEDIA_PT_CELB        = 25,        // V  90000 [RFC2029]
	AS_MEDIA_PT_JPEG        = 26,        // V  90000 [RFC2435]
	AS_MEDIA_PT_NV          = 28,        // V  90000 [RFC3551]
	AS_MEDIA_PT_H261        = 31,        // V  90000 [RFC4587]
	AS_MEDIA_PT_MPV         = 32,        // V  90000 [RFC2250]
	AS_MEDIA_PT_MP2T        = 33,        // AV 90000 [RFC2250]
	AS_MEDIA_PT_H263        = 34,        // V  90000 [Zhu]
};

// ----------------------------------------------------------------------------
// Type Declaration
// ----------------------------------------------------------------------------
typedef struct _as_media_init_attr_t {
	u32 vsystem;
	u32 disp_num;
	struct {
		u32 type;
		u32 format;
	} disp_attr[10];
} as_media_init_attr_t;

typedef struct _as_media_venc_attr_t {
	u8 type;
	u8 ch;
	u16 stream_id;
	u8 fps;
	u8 gop;
	u8 rc_type;
	u8 res;
	u32 bitrate;
} as_media_venc_attr_t;

// ----------------------------------------------------------------------------
// Global Variable Declaration
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// APIs
// ----------------------------------------------------------------------------
/*!
 * @brief   audio out
 * @return  0 on success, otherwise failed
 */
extern int media_adec_out(dm_data_t *data);

/*!
 * @brief   Start encode
 * @return  0 on success, otherwise failed
 */
extern int as_media_enc_start(void);

/*!
 * @brief   Stop encode
 * @return  0 on success, otherwise failed
 */
extern int as_media_enc_stop(void);

/*!
 * @brief   Setup encode
 * @return  0 on success, otherwise failed
 */
extern int as_media_enc_setup(void);

/*!
 * @brief   read video loss
 * @param[in]  type      	brightness, contrast, saturation, hue
 * @param[in]  value       	0 ~ 100, initital value:50
 *
 * @return  0 on success, otherwise failed
 */
extern int as_media_set_video(u16 type, u16 value);

/*!
 * @brief   read video loss
 * @param[out]  vloss       video loss bitmap, 0:ok, 1:loss
 *
 * @return  0 on success, otherwise failed
 */
extern int as_media_read_vloss(u32 *vloss);

/*!
 * @brief   Set attribute of video encode
 * @param[out]  attr		venc attribute
 * @return  0 on success, otherwise failed
 */
extern int as_media_set_venc(const as_media_venc_attr_t *attr);

/*!
 * @brief  	Media initialization
 * @param[in]   attr		initialize attribute of a media
 * @return  0 on success, otherwise failed
 */
extern int as_media_init(const as_media_init_attr_t *attr);

extern int as_codec_is_720p(void);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif // __cplusplus

#endif // __AS_MEDIA_H__
