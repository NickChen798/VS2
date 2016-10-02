/*!
 *  @file       as_cmd.h
 *  @version    1.0
 *  @date       09/01/2011
 *  @author     Jacky Hsu <jacky.hsu@altasec.com>
 *  @note       CMD sender/receiver <br>
 *              Copyright (C) 2012 AltaSec Technology Corporation <br>
 *              http://www.altasec.com
 */

#ifndef __AS_CMD_H__
#define __AS_CMD_H__

// ----------------------------------------------------------------------------
// Include Files
// ----------------------------------------------------------------------------
#include "as_headers.h"
#include "as_dm.h"

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

// ----------------------------------------------------------------------------
// APIs
// ----------------------------------------------------------------------------
/*!
 * @brief       init CMD thread
 * @return      0 on success, otherwise < 0
 */
extern int as_cmd_init(void);

/*!
 * @brief       init data thread
 * @param[in]   ch_nr      max channel
 * @param[in]   stream_nr  max stream per channel
 * @param[in]   client_nr  max client
 * @return      0 on success, otherwise < 0
 */
extern int as_data_init(int ch_nr, int stream_nr, int client_nr);

/*!
 * @brief       data input
 * @param[in]   data    pointer to data
 * @return      0 on success, otherwise < 0
 */
extern int as_data_in(dm_data_t *data);

/*!
 * @brief       yuv data input
 * @param[in]   data    pointer to data
 * @return      0 on success, otherwise < 0
 */
extern int as_yuv_in(dm_data_t *data);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __AS_CMD_H__

