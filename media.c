/*!
 *  @file       media.c
 *  @version    0.01
 *  @author     James Chien <james@phototracq.com>
 */

#include "as_types.h"
#include "as_dm.h"
#include "as_media.h"
// ----------------------------------------------------------------------------
// APIs
// ----------------------------------------------------------------------------
/*!
 * @brief   Initialization
 * @return  0 on success, otherwise failed
 */

// ----------------------------------------------------------------------------
// APIs
// ----------------------------------------------------------------------------
/*!
 * @brief   audio out
 * @return  0 on success, otherwise failed
 */

int media_adec_out(dm_data_t *data) {
}

/*!
 * @brief   Start encode
 * @return  0 on success, otherwise failed
 */
int as_media_enc_start(void) {
}

/*!
 * @brief   Stop encode
 * @return  0 on success, otherwise failed
 */
int as_media_enc_stop(void) {
}

/*!
 * @brief   Setup encode
 * @return  0 on success, otherwise failed
 */
int as_media_enc_setup(void) {
}

/*!
 * @brief   read video loss
 * @param[in]  type      	brightness, contrast, saturation, hue
 * @param[in]  value       	0 ~ 100, initital value:50
 *
 * @return  0 on success, otherwise failed
 */
int as_media_set_video(u16 type, u16 value) {
}

/*!
 * @brief   read video loss
 * @param[out]  vloss       video loss bitmap, 0:ok, 1:loss
 *
 * @return  0 on success, otherwise failed
 */
int as_media_read_vloss(u32 *vloss) {
}

/*!
 * @brief   Set attribute of video encode
 * @param[out]  attr		venc attribute
 * @return  0 on success, otherwise failed
 */
int as_media_set_venc(const as_media_venc_attr_t *attr) {
}

/*!
 * @brief  	Media initialization
 * @param[in]   attr		initialize attribute of a media
 * @return  0 on success, otherwise failed
 */
int as_media_init(const as_media_init_attr_t *attr) {
}

int as_codec_is_720p(void) {
}
