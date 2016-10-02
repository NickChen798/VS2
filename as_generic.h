/*!
 *  @file       as_generic.h
 *  @version    0.1
 *  @date       12/31/2011
 *  @author     Daniel Tsai <daniel.tsai@altasec.com>
 *  @note       Generic Library <br>
 *              Copyright (C) 2011 ALTASEC Corp.
 */

#ifndef __AS_GENERIC_H__
#define __AS_GENERIC_H__

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

// ----------------------------------------------------------------------------
// Constant Definition
// ----------------------------------------------------------------------------
#define AS_GEN_FW_STATUS_IDLE			1
#define AS_GEN_FW_STATUS_READY			0
#define AS_GEN_FW_STATUS_ERROR			(-1)

// ----------------------------------------------------------------------------
// Type Declaration
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Global Variable Declaration
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// APIs
// ----------------------------------------------------------------------------
extern int as_gen_read_nt99140(u32 addr , u8 *data); 
extern int as_gen_write_nt99140(u32 addr, u8 *data); 
extern int as_gen_rs485_test(void); 
extern int as_gen_rs485_status(int *st);
extern int as_gen_usb_status(int *st);
extern int as_gen_read_nt99140(u32 addr, u8 *data);
extern int as_gen_write_nt99140(u32 addr, u8 *data);

extern int as_gen_read_dip_switch(u32 *data);

extern int as_gen_read_alarm_in(u32 *alarm);

extern int as_gen_set_status_led(u32 flag);

extern int as_gen_set_do(u32 flag);

extern int as_gen_set_red_led(u32 flag);

extern int as_gen_set_green_led(u32 flag);

extern int as_gen_set_red_led1(u32 flag);

extern int as_gen_set_green_led1(u32 flag);

extern int as_gen_wdt_reboot(void);

extern int as_gen_wdt_set_margin(u32 margin);

extern int as_gen_wdt_keep_alive(void);

extern int as_gen_wdt_disable(void);

extern int as_gen_wdt_enable(void);

/*!
 * @brief       send pipe command
 *
 * @param[in]  	cmd      		pipe command
 *
 * @return      0 on success, otherwise < 0
 */
extern int as_gen_net_pipe_cmd(const char *cmd);

/*!
 * @brief       set serial number
 *
 * @param[in]	sn		serial number string buffer, max:32 byte
 *
 * @return      0 on success, otherwise < 0
 */
extern int as_gen_set_serial_num(char *sn);

/*!
 * @brief       get serial number
 *
 * @param[out]	sn		serial number string buffer
 * @param[in]	size	string size
 *
 * @return      0 on success, otherwise < 0
 */
extern int as_gen_get_serial_num(char *sn, u32 size);

/*!
 * @brief       set mac address
 *
 * @param[in]	mac		mac address1, 6 byte
 *
 * @return      0 on success, otherwise < 0
 */
extern int as_gen_set_mac(char *mac);

/*!
 * @brief       get mac address
 *
 * @param[out]	mac		mac address
 *
 * @return      0 on success, otherwise < 0
 */
extern int as_gen_get_mac(u8 *mac);

/*!
 * @brief       get firmware version
 *
 * @param[out]	version	version string buffer
 * @param[in]	size	string size
 *
 * @return      0 on success, otherwise < 0
 */
extern int as_gen_fw_get_version(char *version, u32 size);

/*!
 * @brief       get firmware version number
 *
 * @return      firmware version number in 32bits
 */
extern u32 as_gen_fw_get_version_num(void);

/*!
 * @brief       get firmware upgrade status
 *
 * @param[out]	percentage	upgrade status
 *
 * @return      0 on success
 * @return      1 on idle
 * @return      -1 on fail
 */
extern int as_gen_fw_status(u32 *percentage);

/*!
 * @brief       erase config
 *
 * @return      0 on success, otherwise < 0
 */
extern int as_gen_cfg_erase(void);

/*!
 * @brief       write config to flash
 *
 * @param[in]	data	config data
 * @param[in]	size	config size
 * @param[in]	offset	config offset on flash
 *
 * @return      0 on success, otherwise < 0
 */
extern int as_gen_cfg_write(u8 *data, u32 size, u32 offset);

/*!
 * @brief       read config from flash
 *
 * @param[out]	data	config data
 * @param[in]	size	config size
 * @param[in]	offset	config offset on flash
 *
 * @return      0 on success, otherwise < 0
 */
extern int as_gen_cfg_read(u8 *data, u32 size, u32 offset);

/*!
 * @brief       upgrade firmware
 *
 * @param[out]	data	firmware data
 * @param[in]	size	firmware size
 *
 * @return      0 on success, otherwise < 0
 */
extern int as_gen_fw_upgrade(const u8 *data, u32 size);

/*!
 * @brief       generic library initialize
 *
 * @return      0 on success, otherwise < 0
 */
int as_gen_init(void);
int as_gen_set_do(u32 flag);
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif // __cplusplus

#endif // __AS_GENERIC_H__
