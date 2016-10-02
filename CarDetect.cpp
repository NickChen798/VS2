#include <time.h>
#ifndef _DSP
#include "stdafx.h"
#endif	//_DSP
#include <sys/time.h>

//#include "OBErrMsg.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "OBWinDef.h"
#include "CarDetect.h"
#include "./JrDetectCar_NUcv/JrDetectCar_NUcv.h"

//===============================IMAGE==========================================

char	szErrMsg[256];
int		flag = 0;

void  
	SetError( char *szMessage )
{
	strcpy( szErrMsg, szMessage );
	return;
}
void 
	ShowError()
{
	printf( szErrMsg );
	return;
}

//忽略參數nRotate及bInvert, 且只讀OBI檔
HIMAGE
	OCR_LoadImage( char *szFile ) 
{
	LPUIMAGE	lpImage;
	FILE		*fp;
	int			rc;
	char		Msg[256];

	if( (lpImage=(LPUIMAGE)malloc(sizeof(UIMAGE))) == NULL )
	{
		SetError( "Out of memory!" );
		return( NULL );	/*		if Header only */
	}

	if( (fp=fopen(szFile,"rb")) == NULL )
	{
		SetError( "Fail to open file!" );
		free( lpImage );
		return( NULL );
	}
	if( (rc=fread(lpImage,UIMAGE_HD_SIZE,1,fp)) != 1 )
	{
		SetError( "Fail to read file!" );
		fclose( fp );
		free( lpImage );
		return( NULL );
	}

	if( (lpImage->lpBuffer=(char *)(malloc(lpImage->WidthBytes*lpImage->Height))) == NULL )
	{
		SetError("Out of memory!" );
		fclose( fp );
		free( lpImage );
		return( NULL );	/*		if Header only */
	}
	if( (rc=fread(lpImage->lpBuffer,lpImage->WidthBytes,lpImage->Height,fp)) != lpImage->Height )
	{
		sprintf( Msg, "Fail to read %s(nRc=%d)!", szFile, rc );
		SetError( Msg );
		fclose( fp );
		free( lpImage );
		return( NULL );
	}
	fclose( fp );

	return( (HIMAGE)lpImage );
}

int
	OCR_WriteImage( HIMAGE hImage, char *szFile ) 
{
	LPUIMAGE	lpImage;
	FILE		*fp;
	int		rc;

	if( (lpImage=(LPUIMAGE)hImage) == NULL )
	{
		SetError( "OCR_WriteImage(): Null hImage!" );
		return( 0 );	/*		if Header only */
	}
	if( (fp=fopen(szFile,"wb")) == NULL )
	{
		SetError( "Fail to open file!" );
		free( lpImage );
		return( 0 );
	}

	if( (rc=fwrite(lpImage,UIMAGE_HD_SIZE,1,fp)) != 1 )
	{
		SetError( "Fail to write file!" );
		fclose( fp );
		free( lpImage );
		return( 0 );
	}
	if( (rc=fwrite(lpImage->lpBuffer,lpImage->WidthBytes,lpImage->Height,fp)) != lpImage->Height )
	{
		SetError( "Fail to write file!" );
		fclose( fp );
		free( lpImage );
		return( 0 );
	}
	fclose( fp );
	return( 1 );
}

void 	
	OCR_FreeImage( HIMAGE hImage )
{
	LPUIMAGE   lpImage;

	if( (lpImage=(LPUIMAGE)hImage) != NULL )
	{
		if ( lpImage->lpBuffer != NULL )
			free( lpImage->lpBuffer );
		lpImage->lpBuffer = NULL;
		free(lpImage);
	}
	return;
}

//===============================DETECT==========================================

typedef struct tagDETECT{
	HANDLE	hJr; 	
	int		nWidth;
	int		nHeight;
	int		nTotalRoi;
 	RECT	rtRoi[4], rtBound[4];
	HLOG	hLog;
} DETECT, far *LPDETECT;


HANDLE	WINAPI	CRDCreate( int nWidth, int nHeight, int nUnused, int src_num, DWORD dwOption, HLOG hLog)
{
	LPDETECT	lpDetect;
//	RECT		rtRoi;
	int			nTotalRoi;
	printf("[CRD] CRDCreate in\n");
	if (src_num < 0){flag = 1;}

	if( (lpDetect=(LPDETECT)malloc(sizeof(DETECT))) == NULL )
	{
		//SetError( OB_ERR_OUTOFMEMORY, "lpDetect" );
		return( NULL );
	}
	memset( lpDetect, 0, sizeof(DETECT) );
	if( hLog == NULL )
		lpDetect->hJr = JrDetectCar_NUcv_Create(nWidth,nHeight,8,hLog,0,&nTotalRoi,src_num);
	else
		lpDetect->hJr = JrDetectCar_NUcv_Create(nWidth,nHeight,8,hLog,1,&nTotalRoi,src_num);
	if( lpDetect->hJr == NULL )
	{
		free( lpDetect );
		//SetError( OB_ERR_UNDEFINED, "JrDetectCar_NUcv_Create()" );
		return( NULL );
	}
	//printf("\n\nnTotalRoi = %d \n\n",nTotalRoi);
	//if( !JrDetectCar_NUcv_SetParam(lpDetect->hJr,PARM_ROIcnt,nTotalRoi,hLog) )
	//{
	//	JrDetectCar_NUcv_Free( lpDetect->hJr, hLog );
	//	free( lpDetect );
	//	SetError( OB_ERR_UNDEFINED, "JrDetectCar_NUcv_SetParam()" );
	//	return( NULL );
	//}
	//rtRoi.left = rtRoi.top = 0;
	//rtRoi.right = nWidth - 1;
	//rtRoi.bottom = nHeight - 1;
	//for( i=0; i<nTotalRoi; i++ )
	//	memcpy( &(lpDetect->rtRoi[i]), &rtRoi, sizeof(RECT) );
//	if( !JrDetectCar_NUcv_SetROI(lpDetect->hJr,PARM_ROI_ResetArea,lpDetect->rtRoi,nTotalRoi,hLog) )
	if( !JrDetectCar_NUcv_SetROI(lpDetect->hJr,PARM_ROI_ResetArea,hLog) )
	{
		JrDetectCar_NUcv_Free( lpDetect->hJr, hLog );
		free( lpDetect );
		//SetError( OB_ERR_UNDEFINED, "JrDetectCar_NUcv_SetROI()" );
		return( NULL );
	}
	//getroi
	if( !JrDetectCar_NUcv_GetROI(lpDetect->hJr,PARM_ROI_RECT,lpDetect->rtRoi) )
	{
		JrDetectCar_NUcv_Free( lpDetect->hJr, hLog );
		free( lpDetect );
		//SetError( OB_ERR_UNDEFINED, "JrDetectCar_NUcv_GetROI()" );
		return( NULL );
	}

	lpDetect->nWidth = nWidth;
	lpDetect->nHeight = nHeight;
	lpDetect->nTotalRoi = nTotalRoi;
	lpDetect->hLog = hLog;
	return( (HANDLE)lpDetect );
}

int	WINAPI	CRDSetParam( HANDLE	hDetect, DWORD	dwParam, int nValue )
{
	return( 0 );
}

int	WINAPI	CRDSetROI( HANDLE hDetect, int nRoi, LPRECT	lpBoundary )
{
	//LPDETECT	lpDetect;

	//if( (lpDetect=(LPDETECT)hDetect) == NULL )
	//{
	//	SetError( OB_ERR_SYSPARAM, "CRDSetROI(): Null hDetect!" );
	//	return( -1 );
	//}
	//memcpy( &(lpDetect->rtRoi[nRoi]),lpBoundary, sizeof(RECT) );
	//if( !JrDetectCar_NUcv_SetROI(lpDetect->hJr,PARM_ROI_ResetArea,lpDetect->hLog) )
	//{
	//	SetError( OB_ERR_UNDEFINED, "JrDetectCar_NUcv_SetROI()" );
	//	return( -1 );
	//}
	return( 0 );
}

static void _Resize( LPRECT lpDest, int nDestWidth, int nDestHeight, LPRECT lpSrc, LPRECT lpRoi, int nWidth, int nHeight )
{
	//int	nX, nY;
	//printf("\n\nWidth = %d Height = %d\n\n",nWidth,nHeight);
	//nX = (lpSrc->left + lpSrc->right) / 2;
	//nY = (lpSrc->top + lpSrc->bottom) / 2;
//	lpDest->left = nX + lpRoi->left - nDestWidth/2;
	if (nHeight == 540)
	{
		nHeight = nHeight*2;
		nWidth = nWidth*2;
		lpSrc->left = lpSrc->left*2;
		lpSrc->bottom = lpSrc->bottom*2;
		lpSrc->right = lpSrc->right*2;
		lpSrc->top = lpSrc->top*2;
	}
	lpDest->left = (lpSrc->left + lpSrc->right - nDestWidth) / 2;	//nX - nDestWidth/2
	lpDest->right = lpDest->left + nDestWidth - 1;	
	if( lpDest->left < 0 )
	{
		lpDest->left = 0;
		lpDest->right = nDestWidth - 1;
	}
	else if( lpDest->right >= nWidth )
	{
		lpDest->right = nWidth - 1;		
		lpDest->left = nWidth - nDestWidth;
	}

//	lpDest->top = nY+lpRoi->top-120;
	lpDest->top = (lpSrc->top + lpSrc->bottom - nDestHeight) / 2;	//nY - nDestHeight/2
	lpDest->bottom = lpDest->top + nDestHeight - 1;	
	if( lpDest->top < 0 )
	{
		lpDest->top = 0;
		lpDest->bottom = nDestHeight - 1;
	}
	else if( lpDest->bottom >= nHeight )
	{
		lpDest->bottom = nHeight - 1;		
		lpDest->top = nHeight - nDestHeight;
	}
	return;
}

//static void
//_Resize320( LPRECT lpDest, LPRECT lpSrc, LPRECT lpRoi, int nWidth, int nHeight )
//{
//	int	nX, nY;
//
//	nX = (lpSrc->left + lpSrc->right) / 2;
//	nY = (lpSrc->top + lpSrc->bottom) / 2;
////	lpDest->left = nX+lpRoi->left-160;
//	lpDest->left = nX - 160;
//	lpDest->right = lpDest->left + 319;	
//	if( lpDest->left < 0 )
//	{
//		lpDest->left = 0;
//		lpDest->right = 319;
//	}
//	else if( lpDest->right >= nWidth )
//	{
//		lpDest->right = nWidth-1;		
//		lpDest->left = nWidth-320;
//	}
//
////	lpDest->top = nY+lpRoi->top-120;
//	lpDest->top = nY - 120;
//	lpDest->bottom = lpDest->top + 239;	
//	if( lpDest->top < 0 )
//	{
//		lpDest->top = 0;
//		lpDest->bottom = 239;
//	}
//	else if( lpDest->bottom >= nHeight )
//	{
//		lpDest->bottom = nHeight-1;		
//		lpDest->top = nHeight-240;
//	}
//	return;
//}

int	WINAPI	CRDDetect( HANDLE hDetect, LPBYTE lpY, LPBYTE lpX, LPCRDRST lpResult ,int src_num)
{
	LPDETECT	lpDetect;
	int			i, nAns[4];
	LPCRDRST	lpRst;
	clock_t		start, finish;
	struct timeval bef;
	struct timeval aft;
	int		use_sec;
	int		x;
	char	time_buffer[64];
	for(i=0;i<4;i++){nAns[i]=0;}
	if( (lpDetect=(LPDETECT)hDetect) == NULL )
	{
		//SetError( OB_ERR_SYSPARAM, "CRDDetect(): Null hDetect!" );
		return( -1 );
	}
//printf( "Enter: lpDetect=%d, hJr=%d\n", lpDetect, lpDetect->hJr );
gettimeofday(&bef, 0);
start = clock();
//nAns[0]=0;
//printf( "before: nAns[0]=%d, ROI=%d, lpDetect=%d, hJr=%d\n", nAns[0], lpDetect->nTotalRoi, lpDetect, lpDetect->hJr );
	if( !JrDetectCar_NUcv(lpDetect->hJr,lpY,lpX,nAns,lpDetect->hLog,lpDetect->rtBound) )
	{
		//SetError( OB_ERR_UNDEFINED, "in JrDetectCar_NUcv()!" );
		return( -1 );
	}
finish = clock();
printf("total ROI = %d\n",lpDetect->nTotalRoi);
printf( "\tJrDetectCar_NUcv(): CPU %2.5f seconds!\n", (double)(finish - start) / CLOCKS_PER_SEC );

gettimeofday(&aft, 0);
use_sec = aft.tv_sec - bef.tv_sec ;
printf("\tJrDetectCar_NUcv() Use Time = %d sec.\n",use_sec);
//printf( "after: ROI=%d\n", lpDetect->nTotalRoi );
	if (flag == 1){src_num = 0;}
	for( i=(0+src_num), lpRst=lpResult; i<(lpDetect->nTotalRoi+src_num); i++, lpRst++ )
	{
		
		lpRst->nConfidence = nAns[i];
		_Resize( &(lpRst->rtBoundary), 320, 240, &(lpDetect->rtBound[i]), &(lpDetect->rtRoi[i]), lpDetect->nWidth, lpDetect->nHeight );
printf( "CRDDetect(): ANS[%d]=%d, Resize(%d,%d;%d,%d)=%d,%d;%d,%d\n", i+1, nAns[i], 
	lpDetect->rtBound[i].left, lpDetect->rtBound[i].top, lpDetect->rtBound[i].right, lpDetect->rtBound[i].bottom, 
	lpRst->rtBoundary.left,    lpRst->rtBoundary.top,    lpRst->rtBoundary.right,    lpRst->rtBoundary.bottom );
	}
	printf("[CRD] CRDDetect in nAns[0] = %d , nAns[1] = %d , nAns[2] = %d , nAns[3] = %d\n",nAns[0],nAns[1],nAns[2],nAns[3]);
//printf( "leave: nAns[0]=%d, lpDetect=%d, hJr=%d\n", nAns[0], lpDetect, lpDetect->hJr );
	return( lpDetect->nTotalRoi );
}

int	WINAPI	CRDFree( HANDLE hDetect )
{
	LPDETECT	lpDetect;

	if( (lpDetect=(LPDETECT)hDetect) == NULL )
	{
		//SetError( OB_ERR_SYSPARAM, "CRDFree(): Null hDetect!" );
		return( -1 );
	}
	JrDetectCar_NUcv_Free( lpDetect->hJr, lpDetect->hLog );
	free( lpDetect );
	return( 0 );
}

