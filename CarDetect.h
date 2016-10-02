//CarDetect.h
//#include "OBLogFil.h"

#ifndef _CARDETECT_H_
#define _CARDETECT_H_

#ifdef	__cplusplus
extern "C"  {
#endif	/* __cplusplus */

	typedef unsigned int		HLOG;
	typedef void				*HIMAGE;

	typedef struct tagImage{
		short   Width;
		short   Height;
		short   WidthBytes;
		short   BitsPerSample;
		short   SamplesPerPixel;
		short   xResolution;
		short   yResolution;
		unsigned short	nCompressMode;
		char   *lpBuffer;
	} UIMAGE, *PUIMAGE, *LPUIMAGE;
	//don't use sizeof(UIMAGE) as 20 is not 8x
#define UIMAGE_HD_SIZE	20	

typedef struct tagCRDRST{
 	int		nConfidence;
 	RECT	rtBoundary;
} CRDRST, far *LPCRDRST;

HIMAGE	OCR_LoadImage( 
	char *szFile );

int		OCR_WriteImage( 
	HIMAGE hImage, char *szFile ) ;

void 	OCR_FreeImage( 
	HIMAGE hImage );

HANDLE	WINAPI	CRDCreate( 
		int		nWidth,
		int		nHeight,
		int		nTotalRoi,
		int		src_num,
		DWORD	dwOption,
		HLOG	hLog);

int		WINAPI	CRDSetParam( 
		HANDLE	hDetect, 
		DWORD	dwParam, int nValue );

int		WINAPI	CRDSetROI( 
		HANDLE	hDetect, 
		int		nRoi,	//from 0
		LPRECT	lpBoundary );

int		WINAPI	CRDDetect( 
		HANDLE	hDetect, 
		LPBYTE	lpY,
		LPBYTE	lpX,
		LPCRDRST	lpResult, 
		int		src_num);

int		WINAPI	CRDFree( HANDLE hDetect );

#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif	//_CARDETECT_H_
