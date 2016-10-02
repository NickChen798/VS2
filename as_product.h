/*!
 *  @file       as_product.h
 *  @version    0.1
 *  @date       12/26/2011
 *  @author     Daniel Tsai <daniel.tsai@altasec.com>
 *  @note       Product define <br>
 *              Copyright (C) 2011 ALTASEC Corp.
 */

#ifndef __AS_PRODUCT_H__
#define __AS_PRODUCT_H__

#ifdef __cplusplus
#if __cplusplus
extern "C"
{
#endif
#endif // __cplusplus

#ifdef MKF_BUILD_DATE
#define FW_BUILD        MKF_BUILD_DATE
#else
#define FW_BUILD        0x00000000
#endif

// MKF_PRODUCT:
//		0: NVR
//		1: DVR
//		2: HYBRID DVR
//		3: IPCAM
//		4: TVR
//		5: VS1
// ----------------------------------------------------------------------------
// Type Declaration
// ----------------------------------------------------------------------------
#define MKF_SENSOR_NONE							0

// << NVR PRODUCT >>
#if (MKF_PRODUCT == 0)
// << DVR PRODUCT >>
#elif (MKF_PRODUCT == 1)
// << HYBRID DVR >>
#elif (MKF_PRODUCT == 2)
// << IPCAM & DTV CAM >>
#elif (MKF_PRODUCT == 3)
// << TVR PRODUCT >>
#elif (MKF_PRODUCT == 4)
// << VS1 PRODUCT >>
#elif (MKF_PRODUCT == 5)
#if (MKF_MODEL == 0)
	#define MKF_SD_CHANNEL				2
	#define MKF_960H_CHANNEL			0
	#define MKF_SDI_CHANNEL				0
	#define MKF_DTV_CHANNEL				0
	#define MKF_IPC_CHANNEL  			0

    #define MKF_DC_SENSOR				MKF_SENSOR_NONE
    #define MKF_HD_MAX                  0
    #define MKF_DEC_MAX_D1              0

	#define MKF_ALMOUT_NUM        		2
	#define MKF_ALMIN_NUM           	2

	#define MODEL_NAME				    "VS1"
	#define FW_FILE_NAME			    "vs1.bin"
	#define CFG_EXPORT_FILE_NAME	    "vs1.cfg"
#else // MKF_MODEL
	#define MKF_SD_CHANNEL				0
	#define MKF_960H_CHANNEL			0
	#define MKF_SDI_CHANNEL				1
	#define MKF_DTV_CHANNEL				0
	#define MKF_IPC_CHANNEL  			0

    #define MKF_DC_SENSOR				MKF_SENSOR_NONE
    #define MKF_HD_MAX                  0
    #define MKF_DEC_MAX_D1              0

	#define MKF_ALMOUT_NUM        		2
	#define MKF_ALMIN_NUM           	2

	#define MODEL_NAME				    "VS1HD"
	#define FW_FILE_NAME			    "vs1hd.bin"
	#define CFG_EXPORT_FILE_NAME	    "vs1hd.cfg"
#endif // MKF_MODEL
#else
// NONE
#endif


#define MKF_TOTAL_CHANNEL   (MKF_SD_CHANNEL+MKF_960H_CHANNEL+MKF_SDI_CHANNEL+MKF_DTV_CHANNEL+MKF_IPC_CHANNEL+MKF_HD_CHANNEL)

#define MKF_STREAM_NUM		2

#define MKF_MAX_CLIENT      16

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif // __cplusplus

#endif // __AS_PRODUCT_H__
