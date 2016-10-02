/*!
 *  @file       as_headers.h
 *  @version    1.0
 *  @date       09/14/2011
 *  @author     Jacky Hsu <jacky.hsu@altasec.com>
 *  @note       headers <br>
 *              Copyright (C) 2011 AltaSec Technology Corporation <br>
 *              http://www.altasec.com
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef _LARGEFILE_SOURCE
#define _LARGEFILE_SOURCE
#endif
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif

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
#include <netpacket/packet.h>
#include <net/ethernet.h>

//#define NDEBUG
#include <assert.h>

#include "as_types.h"
#include "as_debug.h"
#include "as_product.h"

