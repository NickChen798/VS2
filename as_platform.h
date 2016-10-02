/*!
 *  @file       as_platform.h
 *  @version    1.0
 *  @date       09/14/2011
 *  @author     Jacky Hsu <jacky.hsu@altasec.com>
 *  @note       platform dependent functions <br>
 *              Copyright (C) 2011 AltaSec Technology Corporation <br>
 *              http://www.altasec.com
 */

#ifndef __AS_PLATFORM__
#define __AS_PLATFORM__

#include "as_types.h"
#include <syscall.h>

// ----------------------------------------------------------------------------
// Constant & Macro Definition
// ----------------------------------------------------------------------------
#define msleep(MS) do {                             \
    struct timespec ms_delay;                       \
    ms_delay.tv_sec = ((MS)>=1000) ? (MS)/1000 : 0; \
    ms_delay.tv_nsec = ((MS)%1000) * 1000000;       \
    nanosleep(&ms_delay, NULL);                     \
} while(0)

#define gettid() syscall(__NR_gettid)

// ----------------------------------------------------------------------------
// APIs
// ----------------------------------------------------------------------------
/*!
 * @brief   Create system thread
 *
 * @param[in]   thrd        pointer to thread entry
 * @param[in]   arg         pointer to argument
 * @param[in]   priority    thread priority, 0 = maximum
 *
 * @return  0 on success, otherwise < 0
 */
static inline int create_sys_thread(void* thrd, void *arg, u32 priority)
{
    pthread_t           thread_id;
    pthread_attr_t      proc_thread_attr;
    struct sched_param  proc_thread_priority;
    int                 proc_thread_policy;
    int                 sched_priority_max, sched_priority_min;

    sched_priority_min = sched_get_priority_min(SCHED_RR);
    sched_priority_max = sched_get_priority_max(SCHED_RR);

    if(priority >= sched_priority_max || (sched_priority_max - priority) < sched_priority_min)
        priority = sched_priority_min;
    else
        priority = sched_priority_max - priority;

    pthread_attr_init(&proc_thread_attr);
    pthread_attr_setscope(&proc_thread_attr, PTHREAD_SCOPE_SYSTEM);

    if(pthread_create(&thread_id, &proc_thread_attr, thrd, arg) != 0)
    {
        printf("system thread create error: %s\n", strerror(errno));
        return -1;
    }

    pthread_getschedparam(thread_id, &proc_thread_policy, &proc_thread_priority);
    proc_thread_policy = SCHED_RR;
    proc_thread_priority.sched_priority = priority;
    pthread_setschedparam(thread_id, proc_thread_policy, &proc_thread_priority);
    return 0;
}

/*!
 * @brief   Create normal thread
 * @param[in]   thrd        pointer to thread entry
 * @param[in]   arg         pointer to argument
 * @return  0 on success, otherwise < 0
 */
static inline int create_norm_thread(void* thrd, void *arg)
{
    pthread_t thread_id;

    if(pthread_create(&thread_id, NULL, thrd, arg) != 0)
    {
        printf("normal thread create error: %s\n", strerror(errno));
        return -1;
    }

    pthread_detach(thread_id);

    return 0;
}

/*!
 * @brief   Memory Allocation
 * @param[in]   size    Size of buffer
 * @return      Direct I/O accessible memory pointer
 */
static inline void *as_alloc(u32 size)
{
    void *ptr;
    if(size < sysconf(_SC_PAGESIZE))
        ptr = memalign(512, size);
    else
        ptr = valloc(size);
    return ptr;
}

#define as_malloc           malloc
#define as_free             free
#define as_zero(P,S)        memset((P), 0, (S))
#define as_mset             memset
#define as_copy             memcpy


#endif // __AS_PLATFORM__

