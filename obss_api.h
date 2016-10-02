/*!
 *  @file       obss_api.h
 *  @version    1.0
 *  @date       07/29/2013
 *  @author     Jacky Hsu <sch0914@gmail.com>
 *  @note       Orbit Stream Server APIs
 */

#ifndef __OBSS_API_H__
#define __OBSS_API_H__

#include "platform.h"
#include "as_dm.h"
#include "as_protocol.h"
#include "obs_protocol.h"

extern int obs_init(u8 *mac, u32 fver);
extern void obs_stun_setup(asc_config_stun_t *stun);
extern void obs_stun_info(asc_config_stun_t *stun);
extern void obs_set_dio(u32 value);
extern void obs_set_attr(u8 ch, u8 sid, u8 type, u8 sub_type, u16 w, u16 h);
extern int obs_video_in(dm_data_t *data);
extern int obs_audio_in(dm_data_t *data);

#endif // __OBSS_API_H__

