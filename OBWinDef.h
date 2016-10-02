//OBWinDef.h : replace for windows.h
//
//	Copyright(C) 2013 by Orbit Technology Inc., All Rights Reserved.
//	Written by Marvin Chen
//
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifndef _OBWINDEF_
#define _OBWINDEF_

#if defined(_DSP) || defined(_UNIX)
//////////////////////////////////////////////////////////////////////////////////////////////
//start of WinNT.h
#define	DLL_PROCESS_ATTACH		0
#define DLL_THREAD_ATTACH		1
#define DLL_THREAD_DETACH		2
#define DLL_PROCESS_DETACH		3
#define SUBLANG_DEFAULT			0x01    // user default
#define LANG_NEUTRAL			0x00
#define DECLARE_HANDLE(name)	typedef HANDLE name
#define MAKELANGID(p, s)       ((((WORD  )(s)) << 10) | (WORD  )(p))
#define RtlCopyMemory(Destination,Source,Length) memcpy((Destination),(Source),(Length))

typedef char				CHAR;
typedef unsigned int		UINT;
typedef unsigned int        *PUINT;
//typedef int             	BOOL;
typedef int					INT, *LPINT, LONG, *LPLONG;
typedef unsigned int   		DWORD, ULONG, *LPDWORD;
typedef unsigned short  	WORD, *LPWORD;
typedef short 				*PSHORT;  
typedef unsigned char		BYTE;
typedef BYTE	        	*LPBYTE;
typedef char				*LPSTR;
typedef void				*HANDLE;
typedef void	        	*LPVOID;
//end of WinNT.h
//////////////////////////////////////////////////////////////////////////////////////////////
//WinDef.h
#define far
#define FAR
#define near
#define NEAR
#define WINAPI  
#define APIENTRY			WINAPI
#define FALSE               0
#define TRUE				1
#define MAX_PATH			260
#define LOWORD(l)           ((WORD)(((l)) & 0xffff))
#define HIWORD(l)           ((WORD)((((l)) >> 16) & 0xffff))
#define LOBYTE(w)           ((BYTE)(w))
#define HIBYTE(w)           ((BYTE)(((WORD)(w) >> 8) & 0xFF))
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#define min(a,b)            (((a) < (b)) ? (a) : (b))

typedef DWORD   			COLORREF;
typedef DWORD   			*LPCOLORREF;
typedef HANDLE				HWND;
typedef HANDLE				HDC;
typedef HANDLE				HPEN;
typedef HANDLE              HGLOBAL;
typedef HANDLE              HBITMAP;

typedef struct tagPOINT {
    int  	x;
    int  	y;
} POINT, *PPOINT, near *NPPOINT, far *LPPOINT;

typedef struct tagRECT {
    int    left;
    int    top;
    int    right;
    int    bottom;
} RECT, *PRECT, near *NPRECT, far *LPRECT;
typedef const RECT FAR* LPCRECT;

DECLARE_HANDLE(HINSTANCE);
//End of WinDef.h
//////////////////////////////////////////////////////////////////////////////////////////////
//Start of WinBase.h
#define DRIVE_UNKNOWN				0
#define DRIVE_REMOTE				4
#define CopyMemory					RtlCopyMemory
#define FORMAT_MESSAGE_FROM_SYSTEM  0x00001000

typedef struct _SYSTEMTIME {
    WORD wYear;
    WORD wMonth;
    WORD wDayOfWeek;
    WORD wDay;
    WORD wHour;
    WORD wMinute;
    WORD wSecond;
    WORD wMilliseconds;
} SYSTEMTIME, *PSYSTEMTIME, *LPSYSTEMTIME;
//End of WinBase.h
//////////////////////////////////////////////////////////////////////////////////////////////
//WinUser.h
#define MB_OK							0x00000000L
#define MB_ICONHAND						0x00000010L
#define MB_ICONQUESTION					0x00000020L
#define MB_ICONSTOP						MB_ICONHAND
#define MB_ICONEXCLAMATION				0x00000030L
#define MB_ICONASTERISK					0x00000040L
#define MB_ICONINFORMATION				MB_ICONASTERISK
//End of WinUser.h
//////////////////////////////////////////////////////////////////////////////////////////////
//WinError.h
#define ERROR_IO_PENDING            997L    // dderror
//End of WinError.h
//////////////////////////////////////////////////////////////////////////////////////////////
//WinGDI.h
#define SRCCOPY						(DWORD)0x00CC0020 /* dest = source                   */
#define RGB(r,g,b)          ((COLORREF)(((BYTE)(r)|((WORD)((BYTE)(g))<<8))|(((DWORD)(BYTE)(b))<<16)))
//End of WinGDI.h
//////////////////////////////////////////////////////////////////////////////////////////////
//NULL functions to be competible /w windows
#define _snprintf
#define GetDC
#define GetFocus()						NULL
#define SetLastError(dwErrCode)			NULL
#define GetLastError()					NULL
#define MessageBox(a1,a2,a3,a4)			0
#define FormatMessage(a,b,c,d,e,f,g)	0
#define LoadString(a,b,c,d)				NULL

#define __try		
#define	__except(X)	except(X)	
#define __catch(X)	catch(X)
#define __throw_exception_again throw
//////////////////////////////////////////////////////////////////////////////////////////////
#else	//defined(_DSP) || defined(_UNIX)
#include <windows.h>

#endif	//defined(_DSP) || defined(_UNIX)

#endif	//_OBWINDEF_
