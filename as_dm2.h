/*!
 *  @file       as_dm.h
 *  @version    1.0
 *  @date       11/23/2011
 *  @author     Jacky Hsu <jacky.hsu@altasec.com>
 *  @note       Data Management <br>
 *              Copyright (C) 2011 AltaSec Technology Corporation <br>
 *              http://www.altasec.com
 */

#ifndef __AS_DM_H__
#define __AS_DM_H__

// ----------------------------------------------------------------------------
// Include Files
// ----------------------------------------------------------------------------
#include "as_headers.h"
#include "as_time.h"

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

// ----------------------------------------------------------------------------
// Constant Definition
// ----------------------------------------------------------------------------
/*! @defgroup dm_data_type_def DM Data Type */
// @{
#define DM_DT_VIDEO             (0x01)
#define DM_DT_AUDIO             (0x02)
#define DM_DT_META              (0x03)
// @}

/*! @defgroup dm_return_code_def DM Return Code */
// @{
#define DM_RC_READY             (0)
#define DM_RC_DNR               (-1)    // Data Not Ready
#define DM_RC_ND                (-2)    // No Data
#define DM_RC_PERR              (-3)    // Param Error
#define DM_RC_BUSY              (-4)    // Busy, try later
#define DM_RC_WERR              (-5)    // Write Error
// @}

/*! @defgroup dm_video_type_def DM Video Type */
// @{
#define VID_H264                (0x00)
#define VID_H264_BP             (0x01)
#define VID_H264_MP             (0x02)
#define VID_H264_HP             (0x03)
#define VID_MP4                 (0x10)
#define VID_MP4_SP              (0x11)
#define VID_MP4_ASP             (0x12)
#define VID_MP4_SH              (0x13)
#define VID_MP2                 (0x20)
#define VID_MP2_MP              (0x21)
#define VID_MP2_SP              (0x22)
#define VID_MJPEG               (0x30)
#define VID_YUV420				(0x40)
#define VID_UNKNOWN             (0xff)
// @}

// ----------------------------------------------------------------------------
// Type Declaration
// ----------------------------------------------------------------------------
/*!
 *  @struct dm_data_t
 *  @brief  read/write data structure
 */
typedef struct {

    u8                  channel;    //!< channel id
    u8                  id;         //!< stream id : 0: primary, 1: secondary, etc.
    u8                  type;       //!< @sa dm_data_type_def
    u8                  sub_type;   //!< @sa dm_video_type_def, dm_audio_type_def
    int                 is_key;     //!< this is key frame
    u32                 diff_time;  //!< diff time in usec
    struct timeval      enc_time;   //!< timestamp
    u16                 width;      //!< Video: picture width
    u16                 height;     //!< Video: picture height
    u32                 samplerate; //!< Audio: sample rate
    u16                 bitwidth;   //!< Audio: bit width
    u8                  flags;      //!< b3:event, b2:vloss, b1:motion, b0:alarm
    u8                  title_len;  //!< title length
    u16                 *title;     //!< camera title
    u16                 dch;        //!< decode channel (playback use)
    u16                 pl_num;     //!< payload number
    struct iovec        payloads[16];   //!< payload data

}dm_data_t;

/*!
 *  @struct dm_status_t
 *  @brief  playback status
 */
typedef struct {

    u8                  channel;    //!< channel id
    u8                  id;         //!< stream id : 0: primary, 1: secondary, etc.
    u8                  vtype;      //!< video type @sa dm_video_type_def
    u8                  atype;      //!< audio type @sa dm_payload_type_def
    u16                 width;      //!< Video: picture width
    u16                 height;     //!< Video: picture height
    u16                 samplerate; //!< Audio: sample rate
    u16                 bitwidth;   //!< Audio: bit width
    struct timeval      pb_time;    //!< timestamp
    u16                 title[33];  //!< camera title
    u8                  flags;      //!< b3:event, b2:vloss, b1:motion, b0:alarm
    u32                 Kbps;       //!< bitrate

}dm_status_t;

/*!
 * @typedef int (*dm_callback_t) (dm_data_t *);
 * @brief DM stream call back
 * @param[in]   stream data structure
 * @return  success or not
 * @retval  0   success
 * @retval  -1  unsupported format
 * @retval  -2  decode failed
 * @retval  otherwise  unknown failed
 */
typedef int (*dm_callback_t) (dm_data_t *);

// ----------------------------------------------------------------------------
// Event Input APIs
// ----------------------------------------------------------------------------
/*!
 * @brief   alarm state
 * @param[in]   id      alarm id
 * @param[in]   status  status: b0=on/off, b7=locked
 * @param[in]   trigger trigger map
 * @return  0 on success, otherwise failed
 */
extern int dm_alarm(u8 id, u8 status, u32 trigger);

/*!
 * @brief   motion state
 * @param[in]   id      motion id
 * @param[in]   status  status: b0=on/off, b7=locked
 * @param[in]   trigger trigger map
 * @return  0 on success, otherwise failed
 */
extern int dm_motion(u8 id, u8 status, u32 trigger);

/*!
 * @brief   vloss state
 * @param[in]   id      vloss id
 * @param[in]   status  status: b0=on/off, b7=locked
 * @param[in]   trigger trigger map
 * @return  0 on success, otherwise failed
 */
extern int dm_vloss(u8 id, u8 status, u32 trigger);

/*!
 * @brief   event state
 * @param[in]   id      event id
 * @param[in]   status  status: b0=on/off, b7=locked
 * @param[in]   trigger trigger map
 * @return  0 on success, otherwise failed
 */
extern int dm_event(u8 id, u8 status, u32 trigger);

/*!
 * @brief   camera flags
 * @param[in]   id      camera id
 * @param[in]   flags   b3:event, b2:vloss, b1:motion, b0:alarm
 * @return  0 on success, otherwise failed
 */
extern int dm_camera_flags(u8 id, u8 flags);

// ----------------------------------------------------------------------------
// Data Input APIs
// ----------------------------------------------------------------------------
/*!
 * @brief   Data entry
 * @param[in]   data        data attributes
 * @return  0 on success, otherwise failed
 */
extern int dm_data_in(dm_data_t *data);

/*!
 * @brief   Motion Data entry
 * @param[in]   data        data attributes
 * @return  0 on success, otherwise failed
 */
extern int dm_md_in(dm_data_t *data);

/*!
 * @brief   register a live stream callback
 * @param[in]   cb      callback function
 * @return  0 on success, otherwise failed
 */
extern int dm_reg_live_cb(dm_callback_t cb);

/*!
 * @brief   Register a live stream callback for AS Stream
 * @param[in]   cb      callback function
 * @return  0 on success, otherwise failed
 */
extern int dm_reg_aslive_cb(dm_callback_t cb);

/*!
 * @brief   Register a live yuv stream callback
 * @param[in]   cb      callback function
 * @return  0 on success, otherwise failed
 */
extern int dm_reg_yuv_cb(dm_callback_t cb);

// ----------------------------------------------------------------------------
// Init APIs
// ----------------------------------------------------------------------------
/*!
 * @brief   Data Manager Input initialization
 * @param[in]   ch      max channel
 * @param[in]   stream  max stream per channel
 * @return  0 on success, otherwise failed
 */
extern int dm_init(u32 ch, u32 stream);
//James 20160218
extern int dm_SetEnc(unsigned char stream,unsigned short w,unsigned short h,unsigned char fps,unsigned short kbps);
extern int dm_SetCh(unsigned char mode);
#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __AS_DM_H__

