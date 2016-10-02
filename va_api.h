/*!
 *  @file       va_api.h
 *  @version    1.0
 *  @date       03/06/2014
 *  @author     Jacky Hsu <sch0914@gmail.com>
 *  @note       VA APIs <br>
 */

#ifndef __VA_API_H__
#define __VA_API_H__

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

typedef struct {

    int top;
    int left;
    int bottom;
    int right;

} roi_region_t;

extern int va_roi_setup(int number, int width, int height, roi_region_t *regions);
extern int va_change(void);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __VA_API_H__

