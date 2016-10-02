/******************************************************************************

  Copyright (C), 2001-2011, Hisilicon Tech. Co., Ltd.

 ******************************************************************************
  File Name     : mpi_ao.h
  Version       : Initial Draft
  Author        : Hisilicon multimedia software group
  Created       : 2006/12/08
  Description   : Common Def Of aio 
  History       :
  1.Date        : 2006/12/15
    Author      : z50825
    Modification: Created file
******************************************************************************/

#ifndef _MPI_AO_H__
#define _MPI_AO_H__

#include "hi_type.h"
#include "hi_common.h"
#include "hi_comm_aio.h"

#ifdef __cplusplus
#if __cplusplus
extern "C"
{
#endif
#endif /* __cplusplus */


HI_S32 HI_MPI_AO_SetPubAttr(AUDIO_DEV AudioDevId, const AIO_ATTR_S *pstAttr);
HI_S32 HI_MPI_AO_GetPubAttr(AUDIO_DEV AudioDevId, AIO_ATTR_S *pstAttr);

HI_S32 HI_MPI_AO_Enable(AUDIO_DEV AudioDevId);
HI_S32 HI_MPI_AO_Disable(AUDIO_DEV AudioDevId);

HI_S32 HI_MPI_AO_EnableChn(AUDIO_DEV AudioDevId, AO_CHN AoChn);
HI_S32 HI_MPI_AO_DisableChn(AUDIO_DEV AudioDevId, AO_CHN AoChn);

HI_S32 HI_MPI_AO_SendFrame(AUDIO_DEV AudioDevId, AO_CHN AoChn, 
        const AUDIO_FRAME_S *pstData, HI_U32 u32Block);

HI_S32 HI_MPI_AO_PauseChn(AUDIO_DEV AudioDevId, AO_CHN AoChn);
HI_S32 HI_MPI_AO_ResumeChn(AUDIO_DEV AudioDevId, AO_CHN AoChn);

HI_S32 HI_MPI_AO_GetFd(AUDIO_DEV AudioDevId, AO_CHN AoChn);

/* Close all the Fd of AO device and channel */
HI_S32 HI_MPI_AO_CloseFd(HI_VOID);


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif

