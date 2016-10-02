/*!
 *  @file       as_config.h
 *  @version    1.0
 *  @date       01/11/2013
 *  @author     Jacky Hsu <jacky.hsu@altasec.com>
 *  @note       VS1 config <br>
 *              Copyright (C) 2013 AltaSec Technology Corporation <br>
 *              http://www.altasec.com
 */

#ifndef __AS_CONFIG_H__
#define __AS_CONFIG_H__

// ----------------------------------------------------------------------------
// Include Files
// ----------------------------------------------------------------------------
#include "as_headers.h"

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

// ----------------------------------------------------------------------------
// APIs
// ----------------------------------------------------------------------------
/*!
 * @brief       init config
 * @return      0 on success, otherwise < 0
 */
extern int as_cfg_init(void);

#define ASCFG_IPADDR    0x01
#define ASCFG_IPCONFIG  0x02
#define ASCFG_STUN      0x03
#define ASCFG_ENCODER1  0x04
#define ASCFG_ENCODER2  0x05
#define ASCFG_ROI       0x06
#define ASCFG_ISP       0x07
#define ASCFG_USER      0x08

/*!
 * @brief       Get config data
 * @param[in]   cid     config id
 * @param[in]   dsize   config size
 * @param[out]  data    config data
 * @return      0 on success, otherwise < 0
 */
extern int as_cfg_get(u8 cid, u16 dsize, u8 *data);

/*!
 * @brief       Set config data
 * @param[in]   cid     config id
 * @param[in]   dsize   config size
 * @param[in]   data    config data
 * @return      0 on success, otherwise < 0
 */
extern int as_cfg_set(u8 cid, u16 dsize, u8 *data);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __AS_CONFIG_H__

