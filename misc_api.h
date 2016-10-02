/*!
 *  @file       misc_api.h
 *  @version    1.0
 *  @date       08/26/2014
 *  @author     Jacky Hsu <sch0914@gmail.com>
 *  @note       Misc APIs <br>
 */

#ifndef __MISC_API_H__
#define __MISC_API_H__

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

typedef struct {

    unsigned char baudrate;       // 0=115200, 1=57600, 2=38400, 3=19200, 4=9600, 5=4800, 6=2400, 7=1200
    unsigned char data_bits;      // 5,6,7,8
    unsigned char stop_bits;      // 1,2
    unsigned char parity;         // 0:none, 1:odd, 2:even
    unsigned char flow_control;   // 0:no, 1:XON/XOFF, 2:RTS/CTS

} uart_attr_t;

extern int rs485_proc(int sd);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __MISC_API_H__

