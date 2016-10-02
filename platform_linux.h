/*!
 *  @file       platform_linux.h
 *  @version    1.0
 *  @date       07/27/2013
 *  @author     Jacky Hsu <sch0914@gmail>
 *  @note       Platform Dependent Declaration (Linux)
 */

#ifndef __PLATFORM_LINUX_H__
#define __PLATFORM_LINUX_H__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <poll.h>
#include <malloc.h>
#include <netdb.h>
#include <syscall.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/statfs.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <linux/if.h>
#include <linux/cdrom.h>
#include <linux/types.h>
#include <linux/hdreg.h>
#include <linux/fs.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

//#define NDEBUG
#include <assert.h>

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

// ----------------------------------------------------------------------------
// TYPEs
// ----------------------------------------------------------------------------
#define u8      unsigned char
#define u16     unsigned short
#define u32     unsigned int
#define u64     unsigned long long
#define i8      char
#define i16     short
#define i32     int
#define i64     long long

#define JH_PACKED	__attribute__ ((packed))

// ----------------------------------------------------------------------------
// Debug Output
// ----------------------------------------------------------------------------
#define dbg			printf

#define LOGV 		dbg
#define LOGD 		dbg
#define LOGI 		dbg
#define LOGW 		dbg
#define LOGE 		dbg

#define jh_strerror	strerror

// ----------------------------------------------------------------------------
// Memory
// ----------------------------------------------------------------------------
#define jh_copy				memcpy
#define jh_malloc			malloc
#define jh_free				free
#define jh_zero(X,Y)		memset(X, 0, Y)

static inline void *jh_alloc(u32 size)
{
    void *ptr;
    if(size < sysconf(_SC_PAGESIZE))
        ptr = memalign(512, size);
    else
        ptr = valloc(size);
    return ptr;
}

// ----------------------------------------------------------------------------
// String
// ----------------------------------------------------------------------------
#define jh_sprintf(a,b,...)	_snprintf_s(a,b, ##__VA_ARGS__)		

// ----------------------------------------------------------------------------
// Time
// ----------------------------------------------------------------------------
#define jh_msleep(MS) do {                          \
    struct timespec ms_delay;                       \
    ms_delay.tv_sec = ((MS)>=1000) ? (MS)/1000 : 0; \
    ms_delay.tv_nsec = ((MS)%1000) * 1000000;       \
    nanosleep(&ms_delay, NULL);                     \
} while(0)

static inline u64 jh_msec(void)
{
	struct timeval tv;
	if(gettimeofday(&tv, NULL) != 0)
		return 0;
	return (((u64)tv.tv_sec) * 1000) + (tv.tv_usec / 1000);
}

static inline u64 jh_usec(void)
{
	struct timeval tv;
	if(gettimeofday(&tv, NULL) != 0)
		return 0;
	return (((u64)tv.tv_sec) * 1000000) + tv.tv_usec;
}

// ----------------------------------------------------------------------------
// Socket
// ----------------------------------------------------------------------------
#define closesocket		close

// ----------------------------------------------------------------------------
// Thread
// ----------------------------------------------------------------------------
#define jh_gettid() 		syscall(__NR_gettid)

static inline int jh_thread_create(void* thrd, void *arg)
{
    pthread_t thread_id;
    if(pthread_create(&thread_id, NULL, thrd, arg) != 0) {
        dbg("normal thread create error: %s\n", strerror(errno));
        return -1;
    }
    pthread_detach(thread_id);
    return 0;
}

// ----------------------------------------------------------------------------
// Mutex
// ----------------------------------------------------------------------------
#define jh_mutex_t			pthread_mutex_t
#define jh_mutex_init(m)	pthread_mutex_init(m,0)
#define jh_mutex_lock(m)	pthread_mutex_lock(m)
#define jh_mutex_unlock(m)	pthread_mutex_unlock(m)
#define jh_mutex_exit(m)	pthread_mutex_destroy(m)

// ----------------------------------------------------------------------------

#ifdef __cplusplus
}
#endif // __cplusplus

#endif //__PLATFORM_LINUX_H__
