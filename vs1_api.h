/*!
 *  @file       vs1_api.h
 *  @version    1.0
 *  @date       11/23/2012
 *  @author     Jacky Hsu <jacky.hsu@altasec.com>
 *  @note       VS1 APIs <br>
 *              Copyright (C) 2012 AltaSec Technology Corporation <br>
 *              http://www.altasec.com
 */

#ifndef __VS1_API_H__
#define __VS1_API_H__

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

/*!
 * @brief   YUV Data Callback
 * @param[in]   width       width
 * @param[in]   height      height
 * @param[in]   ptr         buffer pointer
 * @param[in]   size        data size
 * @return  0 on success, otherwise failed
 */
typedef int (*yuv_callback_t) (int, int, char *, int);

/*!
 * @brief   Register YUV Data Callback
 * @param[in]   callback    YUV data callback
 * @return  0 on success, otherwise failed
 */
extern int vs1_yuv_register(yuv_callback_t cb);

/*!
 * @brief   Set DO
 * @param[in]   status      DO status
 * @return  0 on success, otherwise failed
 */
extern int vs1_set_do(unsigned int status,int flag);

/*!
 * @brief   Get DI
 * @return  DI Status
 */
extern unsigned int vs1_get_di(void);

/*!
 * @brief   Audio Out
 * @param[in]   buf         audio buffer
 * @param[in]   size        audio size
 * @return  0 on success, otherwise failed
 */
extern int vs1_audio_out(unsigned char *buf, int size);

/*!
 * @brief   Set Ring
 * @param[in]   on          Ring on/off
 * @return  0 on success, otherwise failed
 */
extern int vs1_set_ring(unsigned int on);

/*!
 * @brief   Get Motion Client
 * @return  0: no client connected with motion mode, >0 otherwise
 */
extern int vs1_get_motion_client(void);

/*!
 * @brief   Set Motion Status
 * @param[in]   status      1: on, 0:off
 * @return  0 on success, otherwise failed
 */
extern int vs1_set_motion(int status);

/*!
 * @brief   Motion Data Callback
 * @param[in]   width       width
 * @param[in]   height      height
 * @param[in]   ptr         buffer pointer (size=widthxheight)
 * @return  0 on success, otherwise failed
 */
typedef int (*md_callback_t) (int, int, unsigned char *);

/*!
 * @brief   Register Motion Data Callback
 * @param[in]   callback    Motion data callback
 * @return  0 on success, otherwise failed
 */
extern int vs1_md_register(md_callback_t cb);

#define CD_GROUP_NR     5

typedef struct {

    int     id;
    int     status;     // 0:no car, 1: car in
    int     width;
    int     height;
    int     left;
    int     top;
    int     right;
    int     bottom;
    time_t  sec;
    time_t  usec;
    void    *data[CD_GROUP_NR];

} roi_info_t;

extern int vs1_roi_out(roi_info_t *roi);

extern const char *vs1_get_userdata(int a);
extern int vs1_set_userdata(char *buf, int size);
extern void vimg_H264_skip_frame(unsigned int s_frame);
extern int vimg_SetYUV_Res(unsigned char HiLoRes);
extern int encoder_set(void);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __VS1_API_H__

