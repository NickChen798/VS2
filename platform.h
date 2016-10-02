/*!
 *  @file       platform.h
 *  @version    1.0
 *  @date       07/27/2013
 *  @author     Jacky Hsu <sch0914@gmail>
 *  @note       Platform Dependent Declaration
 */

#ifndef __PLATFORM_H__
#define __PLATFORM_H__

#if defined(WIN32) || defined(_WIN32)
#include "platform_win32.h"
#elif defined(__ANDROID__)
// __ANDROID_API__
#include "platform_ndk.h"
#elif defined(__gnu_linux__) || defined(__linux__)
#include "platform_linux.h"
#elif defined(__CYGWIN__)
#include "platform_linux.h"
#elif defined(macintosh)
#error "MACOS is not supported yet."
#else
#error "Unknown System"
#endif

#endif //__PLATFORM_H__
