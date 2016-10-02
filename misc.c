/*!
 *  @file       misc.c
 *  @version    1.0
 *  @date       28/08/2014
 *  @author     Jacky Hsu
 *  @note
 *
 */

// ----------------------------------------------------------------------------
// Include Files
// ----------------------------------------------------------------------------
#include "as_headers.h"
#include "as_platform.h"
#include "misc_api.h"
#include <termios.h>

// ----------------------------------------------------------------------------
// Debug
// ----------------------------------------------------------------------------
#define d_err(format,...)  printf("[M]%s(%d): " format, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define d_msg(format,...)  printf("[M] " format, ##__VA_ARGS__)

// ----------------------------------------------------------------------------
// Constant & Macro Definition
// ----------------------------------------------------------------------------
#define BSIZE   4096

// ----------------------------------------------------------------------------
// Type Declaration
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Local Variable Declaration
// ----------------------------------------------------------------------------
static pthread_mutex_t rs485_mutex = PTHREAD_MUTEX_INITIALIZER;
static int rs485_inuse = 0;
static u8 rs485_buf[BSIZE];

#define BR_NR   8
static int baudrate[BR_NR] = {B115200, B57600, B38400, B19200, B9600, B4800, B2400, B1200};
static int data_bits[4] = {CS5, CS6, CS7, CS8};

// ----------------------------------------------------------------------------
// Local Functions
// ----------------------------------------------------------------------------
#define RS485_DEV   "/dev/ttyAMA1"
static int rs485_init(const char *dev, uart_attr_t *ua)
{
	struct termios newtio;
	int fd = -1;

	fd = open(dev, O_RDWR | O_NOCTTY);
	if(fd < 0) {
		d_err("open %s fail %s\n", dev, strerror(errno));
		return -1;
	}

	bzero(&newtio, sizeof(newtio));
    if(ua->baudrate >= BR_NR) {
        d_err("invalid baudrate index: %d", ua->baudrate);
        return -1;
    }
    if(ua->data_bits < 5 || ua->data_bits > 8) {
        d_err("invalid data bits: %d", ua->data_bits);
        return -1;
    }
    if(ua->stop_bits < 1 || ua->stop_bits > 2) {
        d_err("invalid stop bits: %d", ua->stop_bits);
        return -1;
    }
    if(ua->parity > 2) {
        d_err("invalid parity: %d", ua->parity);
        return -1;
    }
    if(ua->flow_control > 2) {
        d_err("invalid flow control : %d", ua->flow_control);
        return -1;
    }
	newtio.c_iflag = IGNPAR;
	newtio.c_oflag = 0;
	newtio.c_lflag = 0;
	newtio.c_cc[VTIME] = 1;
	newtio.c_cc[VMIN] = 36;
    newtio.c_cflag = baudrate[ua->baudrate] | \
                     data_bits[ua->data_bits-5] | \
                     ((ua->stop_bits == 2) ? CSTOPB : 0) | CLOCAL | CREAD;
    if(ua->parity) {
        if(ua->parity == 1)
            newtio.c_cflag |= PARENB | PARODD;
        else
            newtio.c_cflag |= PARENB;
    }
    if(ua->flow_control) {
        if(ua->flow_control == 1)
            newtio.c_iflag |= IXON | IXOFF;
        else if(ua->flow_control == 2)
            newtio.c_cflag |= CRTSCTS;

    }

	tcsetattr(fd,TCSANOW,&newtio);
	tcflush(fd, TCIOFLUSH);

	return fd;
}

static int get_attr(int sd, uart_attr_t *ua)
{
    int off = 0;
    while(off < sizeof(uart_attr_t)) {
        int rsize = read(sd, ((char *)ua)+off, sizeof(uart_attr_t)-off);
        if(rsize <= 0) {
            d_err("read attr error: %s\n", strerror(errno));
            return -1;
        }
        off += rsize;
    }
    d_msg("BR:%u, DB:%u, SB:%u, P:%u, FC:%u\n",
            ua->baudrate, ua->data_bits, ua->stop_bits, ua->parity, ua->flow_control);
    return 0;
}

// ----------------------------------------------------------------------------
// APIs
// ----------------------------------------------------------------------------
int rs485_proc(int sd)
{
    pthread_mutex_lock(&rs485_mutex);
    if(rs485_inuse) {
        pthread_mutex_unlock(&rs485_mutex);
        return -1;
    }
    rs485_inuse = 1;
    pthread_mutex_unlock(&rs485_mutex);

    int fd = -1;
    uart_attr_t ua;

    if(get_attr(sd, &ua) < 0)
        goto _rs485_exit_;

_rs485_init_:
    if(fd >= 0) {
        close(fd);
        sleep(1);
    }
    fd = rs485_init(RS485_DEV, &ua);
    if(fd < 0)
        goto _rs485_exit_;

    while(1) {
        fd_set rds;
        struct timeval tv;
        int retval;

        FD_ZERO(&rds);
        FD_SET(sd, &rds);
        FD_SET(fd, &rds);

        tv.tv_sec = 0;
        tv.tv_usec = 20000;

        retval = select((fd > sd) ? fd+1 : sd+1, &rds, 0, 0, &tv);

        if(retval < 0) {
            d_err("select error: %s\n", strerror(errno));
            goto _rs485_exit_;
        }
        else if(retval) {
            if(FD_ISSET(sd, &rds)) { // socket or bt
                int rsize = read(sd, rs485_buf, BSIZE);
                if(rsize <= 0) {
                    d_err("read error on source: %s\n", strerror(errno));
                    break;
                }
                int total = 0;
                d_msg("read %d from source\n", rsize);
                while(total < rsize) {
                    int ssize = write(fd, rs485_buf+total, rsize-total);
                    if(ssize <= 0) {
                        d_err("write error on rs485: %s\n", strerror(errno));
                        break;
                    }
                    total += ssize;
                }
                d_msg("write %d to rs485\n", rsize);
            }
            if(FD_ISSET(fd, &rds)) { // rs485
                int rsize = read(fd, rs485_buf, BSIZE);
                if(rsize <= 0) {
                    d_err("read error on rs485: %s\n", strerror(errno));
                    break;
                }
                int total = 0;
                d_msg("read %d from rs485\n", rsize);
                while(total < rsize) {
                    int ssize = write(sd, rs485_buf+total, rsize-total);
                    if(ssize <= 0) {
                        d_err("write error on source: %s\n", strerror(errno));
                        break;
                    }
                    total += ssize;
                }
                d_msg("write %d to source\n", rsize);
            }
        }
    }

_rs485_exit_:
    if(fd >= 0)
        close(fd);
    pthread_mutex_lock(&rs485_mutex);
    rs485_inuse = 0;
    pthread_mutex_unlock(&rs485_mutex);

    return 0;
}

