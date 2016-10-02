/*!
 *  @file       as_time.h
 *  @version    1.0
 *  @date       12/05/2011
 *  @author     Jacky Hsu <jacky.hsu@altasec.com>
 *  @note       Time Functions <br>
 *              Copyright (C) 2011 AltaSec Technology Corporation <br>
 *              http://www.altasec.com
 */

#ifndef __AS_TIME_H__
#define __AS_TIME_H__

// ----------------------------------------------------------------------------
// Include Files
// ----------------------------------------------------------------------------
#include "as_headers.h"

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

// ----------------------------------------------------------------------------
// Constant & Macro Definition
// ----------------------------------------------------------------------------
/*! @defgroup time_str_format_def Time String Format */
// @{
#define TSTR_YYYYMMDD       0
#define TSTR_MMDDYYYY       1
#define TSTR_DDMMYYYY       2
// @}

// YYYY, MM, DD -> 0xYYYYMMDD
#define AS_DATE(Y,M,D)      (((Y) << 16) | ((M) << 8) | (D))

// HH, MM, SS -> 0x00HHMMSS
#define AS_TIME(H,M,S)      (((H) << 16) | ((M) << 8) | (S))

// 0xYYYYMMDD -> yyyy-mm-dd
#define AS_DATE_YEAR(T)     ((T) >> 16)             // number of years
#define AS_DATE_MONTH(T)    (((T) >> 8) & 0xff)     // number of months, 1 ~ 12
#define AS_DATE_DAY(T)      ((T) & 0xff)            // day of month, 1 ~ 31

// 0x00HHMMSS -> hh:mm:ss
#define AS_TIME_HOUR(T)     (((T) >> 16) & 0xff)    // number of hours, 0 ~ 23
#define AS_TIME_MINUTE(T)   (((T) >> 8) & 0xff)     // number of minutes, 0 ~ 59
#define AS_TIME_SECOND(T)   ((T) & 0xff)            // number of seconds, 0 ~ 59

#define AS_UTC(U,Y,M,D,H,MM,S) do {     \
    struct tm tt;                       \
    tt.tm_sec = S;                      \
    tt.tm_min = MM;                     \
    tt.tm_hour = H;                     \
    tt.tm_mday = D;                     \
    tt.tm_mon = (M) - 1;                \
    tt.tm_year = (Y) - 1900;            \
    U = (u32) mktime(&tt);              \
} while(0)

// ----------------------------------------------------------------------------
// Type Declaration
// ----------------------------------------------------------------------------
/*!
 *  @struct ds_info_t
 *  @brief  Daylight Saving Information
 */
typedef struct {

    int     tz1;            // Normal Time Zone in seconds
    int     tz2;            // TZ in Daylight Saving
    u8      start_m;        // 1 <= m <= 12
    u8      start_w;        // 1 <= w <= 5
    u8      start_d;        // 0 <= d <= 6 (0=Sun, 6=Sat)
    u8      start_h;        // 0 <= h <= 23
    u8      start_min;      // 0 <= m <= 59
    u8      end_m;          // 1 <= m <= 12
    u8      end_w;          // 1 <= w <= 5
    u8      end_d;          // 0 <= d <= 6 (0=Sun, 6=Sat)
    u8      end_h;          // 0 <= h <= 23
    u8      end_min;        // 0 <= m <= 59

}ds_info_t;

// ----------------------------------------------------------------------------
// APIs
// ----------------------------------------------------------------------------
/*!
 * @brief   Set time by UTC
 * @param[in]   utc     utc time
 */
extern void as_time_set(u32 utc);

/*!
 * @brief   Set timezone
 * @param[in]   tz      timezone in seconds (ex: GMT-8 = -28800)
 * @param[in]   ds      ds information, 0 means no daylight saving
 * @return  0 on success, otherwise failed
 */
extern int as_tz_set(int tz, ds_info_t *ds);

/*!
 * @brief   Get UTC time
 * @param[out]  sec     UTC time in seconds
 * @param[out]  usec    micro seconds
 * @param[out]  tz      timezone in seconds (ex: GMT-8 = -28800)
 */
extern void as_time_get(u32 *sec, u32 *usec, int *tz);

/*!
 * @brief   Set local time
 * @param[in]   local   local time (tm_wday, tm_yday are ignored, tm_isdst is in effect if dst is set)
 */
extern void as_localtime_set(struct tm *local);

/*!
 * @brief   Get local time
 * @param[out]  local   local time
 * @param[out]  tz      timezone in seconds (ex: GMT-8 = -28800)
 */
extern void as_localtime_get(struct tm *local, int *tz);

/*!
 * @brief   UTC to Local time
 * @param[in]   utc     UTC time
 * @param[out]  local   Local time
 */
extern void as_localtime(u32 utc, struct tm *local);

/*!
 * @brief   Local time to UTC
 * @param[in]  local   local time
 * @return  reutrn UTC time
 */
extern u32 as_utc(struct tm *local);

/*!
 * @brief   Set time format
 * @param[in]   format      format index @sa time_str_format_def
 * @param[in]   is_24h      use 24h format, 0 means 12h
 * @return  0 on success, otherwise failed
 */
extern int as_time_format(int format, int is_24h);

/*!
 * @brief   Get datetime string (Local)
 * @param[in]   utc     given UTC or 0 for current time
 * @param[out]  str     buffer pointer
 * @param[in]   len     buffer size
 * @return  string len
 */
extern int as_datetime_str(u32 utc, char *str, u32 len);

/*!
 * @brief   Get date string (Local)
 * @param[in]   utc     given UTC or 0 for current time
 * @param[out]  str     buffer pointer
 * @param[in]   len     buffer size
 * @return  string len
 */
extern int as_date_str(u32 utc, char *str, u32 len);

/*!
 * @brief   Get time string (Local)
 * @param[in]   utc     given UTC or 0 for current time
 * @param[out]  str     buffer pointer
 * @param[in]   len     buffer size
 * @return  string len
 */
extern int as_time_str(u32 utc, char *str, u32 len);

// ----------------------------------------------------------------------------
// AS_TIME API Functions
// ----------------------------------------------------------------------------
/*!
 * @brief   UTC to AS_DATE
 * @param[in]   utc     UTC time
 * @return  reutrn AS_DATE
 */
static inline u32 utc2_date(u32 utc)
{
    struct tm tt;
    time_t t1 = (time_t)utc;
    localtime_r(&t1, &tt);
    return (u32) AS_DATE(tt.tm_year+1900, tt.tm_mon+1, tt.tm_mday);
}

/*!
 * @brief   UTC to AS_TIME
 * @param[in]   utc     UTC time
 * @return  reutrn AS_TIME
 */
static inline u32 utc2_time(u32 utc)
{
    struct tm tt;
    time_t t1 = (time_t)utc;
    localtime_r(&t1, &tt);
    return (u32) AS_TIME(tt.tm_hour, tt.tm_min, tt.tm_sec);
}

/*!
 * @brief   as_time to UTC
 * @param[in]   udate   AS_DATE
 * @param[in]   utime   AS_TIME
 * @return  reutrn UTC time
 */
static inline u32 as2_utc(u32 udate, u32 utime)
{
    struct tm tt;
    tt.tm_sec = AS_TIME_SECOND(utime);
    tt.tm_min = AS_TIME_MINUTE(utime);
    tt.tm_hour = AS_TIME_HOUR(utime);
    tt.tm_mday = AS_DATE_DAY(udate);
    tt.tm_mon = AS_DATE_MONTH(udate) - 1;
    tt.tm_year = AS_DATE_YEAR(udate) - 1900;
    return (u32) mktime(&tt);
}

/*!
 * @brief   AS_TIME to datetime string
 * @param[in]   udate   AS_DATE
 * @param[in]   utime   AS_TIME
 * @param[out]  str     buffer pointer
 * @param[in]   len     buffer size
 * @return  string len
 */
extern int as2_datetime_str(u32 udate, u32 utime, char *str, u32 len);

/*!
 * @brief   AS_DATE to date string
 * @param[in]   udate   AS_DATE
 * @param[out]  str     buffer pointer
 * @param[in]   len     buffer size
 * @return  string len
 */
extern int as2_date_str(u32 udate, char *str, u32 len);

/*!
 * @brief   AS_TIME to time string
 * @param[in]   utime   AS_TIME
 * @param[out]  str     buffer pointer
 * @param[in]   len     buffer size
 * @return  string len
 */
extern int as2_time_str(u32 utime, char *str, u32 len);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __AS_TIME_H__

