/*!
 *  @file       as_debug.h
 *  @version    1.0
 *  @date       12/20/2011
 *  @author     Sam Tsai <sam.tsai@altasec.com>
 *  @note       Debug Setting <br>
 *              Copyright (C) 2011 AltaSec Technology Corporation <br>
 *              http://www.altasec.com
 */

#ifndef __AS_DEBUG__
#define __AS_DEBUG__

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

#ifndef dbg
#define dbg( fmt, ... )		    printf( "[%s]"fmt"\n", __FUNCTION__, ##__VA_ARGS__ )
#endif

#ifndef dbg_line
#define dbg_line( fmt, ... )	printf( "[%s line: %d]"fmt"\n", __FUNCTION__, __LINE__, ##__VA_ARGS__ )
#endif

#ifndef dbg_r
#define dbg_r(fmt, args...) printf("\033[1;31m"fmt"\033[0;39m\n",## args)
#endif
#ifndef dbg_g
#define dbg_g(fmt, args...) printf("\033[1;32m"fmt"\033[0;39m\n",## args)
#endif
#ifndef dbg_b
#define dbg_b(fmt, args...) printf("\033[1;34m"fmt"\033[0;39m\n",## args)
#endif
#ifndef dbg_y
#define dbg_y(fmt, args...) printf("\033[1;33m"fmt"\033[0;39m\n",## args)
#endif

#define AS_DBG_ASSERT(msg...) \
	do { \
		printf("\nAssert failed at: \n ==> File name: %s\n ==> Function: %s\n ==> Line No.: %d\n ==> Comment: ", \
				__FILE__, __func__, __LINE__); \
		printf(msg); \
		exit(-1); \
	} while(0)


#define AS_DBG_WARN(msg...) \
	do { \
		printf("\nWarn failed at: \n ==> File name: %s\n ==> Function: %s\n ==> Line No.: %d\n ==> Comment: ", \
				__FILE__, __func__, __LINE__); \
		printf(msg); \
	} while(0)

#ifdef __cplusplus
}
#endif // __cplusplus

#endif //__AS_DEBUG__

