/*!
 *  @file       as_cmd.c
 *  @version    1.0
 *  @date       06/11/2012
 *  @author     Jacky Hsu <jacky.hsu@altasec.com>
 *  @note       CMD sender/receiver
 *              Copyright (C) 2012 ALTASEC Corp.
 */

#include "as_headers.h"
#include "as_platform.h"
#include "as_generic.h"
#include "as_media.h"
#include "as_config.h"

#include "as_protocol.h"
#include "obss_api.h"
#include "va_api.h"

// ----------------------------------------------------------------------------
// Debug
// ----------------------------------------------------------------------------
#define cmd_err(format,...)  printf("[CMD]%s(%d): " format, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define cmd_msg(format,...)  printf("[CMD] " format, ##__VA_ARGS__)

// ----------------------------------------------------------------------------
// Model & Type Definition
// ----------------------------------------------------------------------------
static u8   mkf_model = AS_MODEL_HS07;
#ifdef MKF_MODEL
#if MKF_MODEL <= AS_TYPE_1080P
static u8   mkf_type = MKF_MODEL;
#else
static u8   mkf_type = AS_TYPE_D1;
#endif
#else
static u8   mkf_type = AS_TYPE_D1;
#endif

// ----------------------------------------------------------------------------
// Type Define
// ----------------------------------------------------------------------------
#define CMD_BSIZE   2048

#define ETH_NAME    "eth0"

// legacy struct
typedef struct {

    u32 gateway;
    u32 netmask;
    u32 dns;

} ipconfig_t;

typedef struct {

    int     sd;

    int     r_size;

    struct sockaddr_ll  raw_addr;
    struct sockaddr_in  my_addr;
    struct sockaddr_in  bcast;
    struct sockaddr_in  peer;

    u32     my_ip;
    u8      my_mac[6];
    ipconfig_t  ipcfg;

    u8      streams;

    as_media_venc_attr_t    enc[2];

    u32     send_err;
    u32     recv_err;

    char    ip_str[32];
    char    syscmd[256];
    u8      rbuf[CMD_BSIZE];
    u8      sbuf[CMD_BSIZE];

} as_ctrl_t;

#define SETUP_SZ    sizeof(asc_s2d_setup_t)
#define HELLO_SZ    sizeof(asc_d2s_hello_t)

// ----------------------------------------------------------------------------
// Local variable
// ----------------------------------------------------------------------------
static as_ctrl_t as_ctrl;
static u8 switch_id = 0;
static int c_flag = 0;
//                                                FHD    HD  QFHD  QHD  960H  D1  VGA HFHD NHD  HHD 2CIF  CIF QCIF
static u16 enc_width[AS_MEDIA_VENC_RES_END]    = {1920, 1280, 960, 640, 960, 960, 640, 480, 416, 320, 704, 352, 176};
static u16 enc_height_N[AS_MEDIA_VENC_RES_END] = {1080,  720, 528, 352, 480, 540, 480, 256, 240, 176, 240, 240, 128};
static u16 enc_height_P[AS_MEDIA_VENC_RES_END] = {1080,  720, 528, 352, 576, 540, 480, 256, 240, 176, 288, 288, 144};
static u16 *enc_height = enc_height_N;

static char user_data[ASC_USERDATA_SIZE] = {0};

// ----------------------------------------------------------------------------
// Local Functions
// ----------------------------------------------------------------------------
static u8 wh_to_res(u16 width, u16 height)
{
    if(width >= 1920)
        return AS_MEDIA_VENC_RES_FHD;
    else if(width >= 1280)
        return AS_MEDIA_VENC_RES_HD;
    else if(width >= 704) {
        if(height > 288)
            return AS_MEDIA_VENC_RES_D1;
        else
            return AS_MEDIA_VENC_RES_2CIF;
    }
	else if(width >= 640) {
		return AS_MEDIA_VENC_RES_VGA;
	}
    else if(width >= 352)
        return AS_MEDIA_VENC_RES_CIF;
    else //if(width >= 176)
        return AS_MEDIA_VENC_RES_QCIF;
}

static int my_set_enc(as_media_venc_attr_t *e)
{
	//printf("my_set_enc in\n");
    int ret = as_media_set_venc(e);
	//printf("my_seas_media_set_venc out\n");
    obs_set_attr(e->ch, e->stream_id, OBS_TYPE_VIDEO,
            (e->type == AS_MEDIA_VENC_TYPE_H264_BP) ? OBS_VTYPE_H264 : OBS_VTYPE_MJPEG, enc_width[e->res], enc_height[e->res]);
	//printf("obs_set_attr out\n");
    obs_set_attr(e->ch, 0, OBS_TYPE_AUDIO, OBS_ATYPE_ADPCM_DVI4, 16, 16000); // TODO: got from codec ?
     // James 20160218
    //printf("\n\n\nset enc: ch:%d res:%d,e->fps:%d,e->bitrate:%d,e->ch:%d,e->gop:%d, e->rc_type:%d,e->type:%d \r\n\n\n",e->stream_id,e->res,e->fps, e->bitrate , e->ch,e->gop, e->rc_type,e->type);

    dm_SetEnc(e->stream_id,enc_width[e->res],enc_height[e->res],e->fps, e->bitrate);
    return ret;
}

static int get_hw_addr(const char *interface, struct sockaddr_ll *raddr)
{
    as_zero(raddr, sizeof(struct sockaddr_ll));
    raddr->sll_family = AF_PACKET;
    raddr->sll_halen = htons(6);

    int sd = -1;
    sd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if(sd < 0) {
        cmd_err("Open RAW socket error\n");
        return -1;
    }

    struct ifreq ifr;
    strcpy(ifr.ifr_name, interface);

    if (ioctl(sd, SIOCGIFINDEX, &ifr) < 0) {
        cmd_err("Could not find device index number for %s\n", ifr.ifr_name);
        close(sd);
        return -1;
    }
    raddr->sll_ifindex = ifr.ifr_ifindex;

    if (ioctl(sd, SIOCGIFHWADDR, &ifr) < 0) {
        cmd_err("Could not get MAC address for %s\n", ifr.ifr_name);
        close(sd);
        return -1;
    }
    as_copy(raddr->sll_addr, ifr.ifr_hwaddr.sa_data, 6);

    close(sd);

    u8 *a = raddr->sll_addr;
    cmd_msg("%s index is %d, MAC is %02x:%02x:%02x:%02x:%02x:%02x\n",
            interface, ifr.ifr_ifindex, a[0], a[1], a[2], a[3], a[4], a[5]);

    close(sd);
    return 0;
}

static int get_ip(const char *interface, u32 *ifaddr)
{
    struct ifreq ifr;
    strcpy(ifr.ifr_name, interface);
    *ifaddr = 0;

    int sd = socket(AF_INET, SOCK_STREAM, 0);
    if(sd < 0) {
        cmd_err("Open INET socket error\n");
        return -1;
    }

    if(ioctl(sd, SIOCGIFADDR, &ifr) < 0) {
        cmd_err("Could not get IF address for %s\n", ifr.ifr_name);
        close(sd);
        return -1;
    }
    *ifaddr = ntohl(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr);

    cmd_msg("%s IP is 0x%x\n", interface, *ifaddr);

    close(sd);
    return 0;
}

static u8 get_id(void)
{
    u32 id = 0;
    as_gen_read_dip_switch(&id);
    return (u8)(id & 0xff);
}

static int setup_network(as_ctrl_t *asc, u32 ip, u32 mask, u32 gw, u32 dns)
{
    char cmd[128];
    asc->my_ip = ip;
    if(mask == 0) {
        u8 ip3 = ip >> 24;
        if(ip3 == 192) {
            sprintf(cmd, "/sbin/ifconfig %s %u.%u.%u.%u netmask 255.255.255.0", ETH_NAME, (ip >> 24), (ip >> 16) & 0xff, (ip >> 8) & 0xff, ip & 0xff);
            asc->ipcfg.netmask = 0xffffff00;
        }
        else if(ip3 == 172) {
            sprintf(cmd, "/sbin/ifconfig %s %u.%u.%u.%u netmask 255.255.0.0", ETH_NAME, (ip >> 24), (ip >> 16) & 0xff, (ip >> 8) & 0xff, ip & 0xff);
            asc->ipcfg.netmask = 0xffff0000;
        }
        else if(ip3 == 10) {
            sprintf(cmd, "/sbin/ifconfig %s %u.%u.%u.%u netmask 255.0.0.0", ETH_NAME, (ip >> 24), (ip >> 16) & 0xff, (ip >> 8) & 0xff, ip & 0xff);
            asc->ipcfg.netmask = 0xff000000;
        }
    }
    else {
        sprintf(cmd, "/sbin/ifconfig %s %u.%u.%u.%u netmask %u.%u.%u.%u", ETH_NAME,
                (ip >> 24), (ip >> 16) & 0xff, (ip >> 8) & 0xff, ip & 0xff,
                (mask >> 24), (mask >> 16) & 0xff, (mask >> 8) & 0xff, mask & 0xff);
        asc->ipcfg.netmask = mask;
    }
    as_gen_net_pipe_cmd(cmd);
    cmd_msg("Init IP: %s\n", cmd);
    if(gw == 0) {
        sprintf(cmd, "/sbin/route add default gw %u.%u.%u.%u %s", (ip >> 24), (ip >> 16) & 0xff, (ip >> 8) & 0xff, ((ip & 0xff) == 1) ? 254 : 1, ETH_NAME);
        asc->ipcfg.gateway = (ip & 0xffffff00) | (((ip & 0xff) == 1) ? 254 : 1);
    }
    else {
        sprintf(cmd, "/sbin/route add default gw %u.%u.%u.%u %s", (gw >> 24), (gw >> 16) & 0xff, (gw >> 8) & 0xff, gw & 0xff, ETH_NAME);
        asc->ipcfg.gateway = gw;
    }
    as_gen_net_pipe_cmd(cmd);
    cmd_msg("Init Route: %s\n", cmd);
    if(dns == 0) {
        sprintf(cmd, "rm /etc/resolv.conf; echo 'nameserver 8.8.8.8' > /etc/resolv.conf");
        asc->ipcfg.dns = 0x08080808;
    }
    else {
        sprintf(cmd, "rm /etc/resolv.conf; echo 'nameserver %u.%u.%u.%u' > /etc/resolv.conf", (dns >> 24), (dns >> 16) & 0xff, (dns >> 8) & 0xff, dns & 0xff);
        asc->ipcfg.dns = dns;
    }
    as_gen_net_pipe_cmd(cmd);
    cmd_msg("Init DNS: %s\n", cmd);
    return 0;
}

static int renew_ip(as_ctrl_t *asc)
{
    u32 sip = 0;
    as_gen_net_pipe_cmd("/usr/bin/killall udhcpc");

    get_ip(ETH_NAME, &asc->my_ip);
    as_cfg_get(ASCFG_IPADDR, 4, (u8 *)&sip);
    as_cfg_get(ASCFG_IPCONFIG, sizeof(ipconfig_t), (u8 *)&asc->ipcfg);
    switch_id = get_id();
    u32 gw = asc->ipcfg.gateway;
    u32 mask = asc->ipcfg.netmask;
    u32 dns = asc->ipcfg.dns;

    cmd_msg("Current IP: 0x%x, Saved IP: 0x%x/%x/%x/%x, ID=%u\n", asc->my_ip, sip, gw, mask, dns, switch_id);

    char cmd[128];
    if(switch_id == 0 && sip) {
        if(mask == 0) {
            u8 ip3 = sip >> 24;
            if(ip3 == 192)
                sprintf(cmd, "/sbin/ifconfig %s %u.%u.%u.%u netmask 255.255.255.0", ETH_NAME, (sip >> 24), (sip >> 16) & 0xff, (sip >> 8) & 0xff, sip & 0xff);
            else if(ip3 == 172)
                sprintf(cmd, "/sbin/ifconfig %s %u.%u.%u.%u netmask 255.255.0.0", ETH_NAME, (sip >> 24), (sip >> 16) & 0xff, (sip >> 8) & 0xff, sip & 0xff);
            else if(ip3 == 10)
                sprintf(cmd, "/sbin/ifconfig %s %u.%u.%u.%u netmask 255.0.0.0", ETH_NAME, (sip >> 24), (sip >> 16) & 0xff, (sip >> 8) & 0xff, sip & 0xff);
        }
        else {
            sprintf(cmd, "/sbin/ifconfig %s %u.%u.%u.%u netmask %u.%u.%u.%u", ETH_NAME,
                    (sip >> 24), (sip >> 16) & 0xff, (sip >> 8) & 0xff, sip & 0xff,
                    (mask >> 24), (mask >> 16) & 0xff, (mask >> 8) & 0xff, mask & 0xff);
        }
        as_gen_net_pipe_cmd(cmd);
        cmd_msg("Init IP: %s\n", cmd);
        if(gw == 0)
            sprintf(cmd, "/sbin/route add default gw %u.%u.%u.%u %s", (sip >> 24), (sip >> 16) & 0xff, (sip >> 8) & 0xff, ((sip & 0xff) == 1) ? 254 : 1, ETH_NAME);
        else
            sprintf(cmd, "/sbin/route add default gw %u.%u.%u.%u %s", (gw >> 24), (gw >> 16) & 0xff, (gw >> 8) & 0xff, gw & 0xff, ETH_NAME);
        as_gen_net_pipe_cmd(cmd);
        cmd_msg("Init Route: %s\n", cmd);
        if(dns == 0)
            sprintf(cmd, "rm /etc/resolv.conf; echo 'nameserver 8.8.8.8' > /etc/resolv.conf");
        else
            sprintf(cmd, "rm /etc/resolv.conf; echo 'nameserver %u.%u.%u.%u' > /etc/resolv.conf", (dns >> 24), (dns >> 16) & 0xff, (dns >> 8) & 0xff, dns & 0xff);
        as_gen_net_pipe_cmd(cmd);
        cmd_msg("Init DNS: %s\n", cmd);
    }
    else if(switch_id < 255) {
        sip = 0x0a352b00 | switch_id;
        if(sip == asc->my_ip)
            return 0;
        sprintf(cmd, "/sbin/ifconfig %s 10.53.43.%u netmask 255.0.0.0", ETH_NAME, switch_id);
        as_gen_net_pipe_cmd(cmd);
        cmd_msg("Init IP: %s\n", cmd);
        sprintf(cmd, "/sbin/route add default gw 10.53.43.254 " ETH_NAME);
        as_gen_net_pipe_cmd(cmd);
        cmd_msg("Init Route: %s\n", cmd);
    }
    else {
        sprintf(cmd, "/sbin/udhcpc -i %s", ETH_NAME);
        as_gen_net_pipe_cmd(cmd);
        cmd_msg("DHCP Client: %s\n", cmd);
    }

    return 0;
}

// -----------------------------------------------------------------------------
// Legacy Commands
// -----------------------------------------------------------------------------
static int set_enc(as_ctrl_t *asc, int id, as_enc_setup_t *enc)
{
    if(id >= 2 || id < 0)
        return -1;

    as_media_venc_attr_t *e = &asc->enc[id];

    if(enc->vtype == ASF_VTYPE_H264)
        e->type = AS_MEDIA_VENC_TYPE_H264_BP;
    else
        e->type = AS_MEDIA_VENC_TYPE_MJPEG;
    e->ch = 0;
    e->stream_id = id;
    if(!enc) {
        e->fps = 0;
        e->gop = 0;
        my_set_enc(e);
        cmd_msg("enc#%d disable\n", id);
        return 0;
    }

    e->fps = (enc->fps < 30) ? enc->fps : 30;
    e->gop = (e->fps < 2) ? 2 : e->fps;
    e->rc_type = AS_MEDIA_VENC_RC_VBR;
    if(enc->width >= 1920) {
        e->res = AS_MEDIA_VENC_RES_FHD;
        if(e->fps > 10)
            e->fps = 10;
        e->gop = e->fps;
    }
    else if(enc->width >= 1280)
        e->res = AS_MEDIA_VENC_RES_HD;
    else if(enc->width >= 704) {
        if(enc->height > 288)
            e->res = AS_MEDIA_VENC_RES_D1;
        else
            e->res = AS_MEDIA_VENC_RES_2CIF;
    }
	else if (enc->width >=640)
		e->res = AS_MEDIA_VENC_RES_VGA;	
    else if(enc->width >= 352)
        e->res = AS_MEDIA_VENC_RES_CIF;
    else //if(enc->width >= 176)
        e->res = AS_MEDIA_VENC_RES_QCIF;

    e->bitrate = ((enc->bitrate/1000) > 10000) ? 10000 : (enc->bitrate/1000);
    if(e->bitrate < 64)
        e->bitrate = 64;
    my_set_enc(e);

    cmd_msg("enc#%d: WxH:%ux%u, res=%u, fps=%u, br=%u\n", id, enc->width, enc->height, e->res, e->fps, e->bitrate);

    return 0;
}

static int get_enc(as_ctrl_t *asc, int id, as_enc_setup_t *enc)
{
    assert(id < 2);

    as_media_venc_attr_t *e = &asc->enc[id];

    if(e->type == AS_MEDIA_VENC_TYPE_H264_BP)
        enc->vtype = ASF_VTYPE_H264;
    else
        enc->vtype = ASF_VTYPE_MJPEG;
    enc->fps = e->fps;
    enc->quality = 5;
    enc->audio = 1;
    enc->width = enc_width[e->res]; //enc_width[id]; //
    enc->height = enc_height[e->res]; //enc_height[id]; //e
    enc->bitrate = e->bitrate * 1000;

    return 0;
}

static int get_hello(as_ctrl_t *asc, asc_d2s_hello_t *h)
{
    as_zero(h, HELLO_SZ);
    h->cmd.magic = ASCMD_MAGIC;
    h->cmd.version = ASCMD_VERSION;
    h->cmd.cmd = ASCMD_D2S_HELLO;
    h->cmd.len = HELLO_SZ;
    h->fw_ver = as_gen_fw_get_version_num();
    h->fw_build = FW_BUILD;
    h->model = mkf_model;
    h->type = mkf_type;
    as_copy(h->mac, asc->my_mac, 6);
    get_ip(ETH_NAME, &asc->my_ip);
    h->ip = asc->my_ip;
    h->id = get_id();
    h->streams = asc->streams;
    get_enc(asc, 0, &h->enc1);
    if(asc->streams > 1)
        get_enc(asc, 1, &h->enc2);
    else
        as_zero(&h->enc2, sizeof(as_enc_setup_t));
    return 0;
}

static int send_hello(as_ctrl_t *asc, struct sockaddr_in *dest, struct sockaddr_in *bcast)
{
    asc_d2s_hello_t *h = (asc_d2s_hello_t *)(asc->sbuf);
    get_hello(asc, h);

    int ret;
    if(bcast && bcast->sin_addr.s_addr != 0 && bcast->sin_port != 0) {
        int ret = sendto(asc->sd, h, HELLO_SZ, 0, (struct sockaddr *)bcast, sizeof(struct sockaddr_in));
        if(ret != HELLO_SZ) {
            cmd_err("send HELLO error, %d, %s\n", errno, strerror(errno));
            if(ret < 0)
                return -1;
        }
    }
    if(dest && dest->sin_addr.s_addr != 0 && dest->sin_port != 0) {
        printf("Send Hello to:0x%x:%u\n", ntohl(dest->sin_addr.s_addr), dest->sin_port);
        int ret = sendto(asc->sd, h, HELLO_SZ, 0, (struct sockaddr *)dest, sizeof(struct sockaddr_in));
        if(ret != HELLO_SZ) {
            cmd_err("send HELLO error, %d, %s\n", errno, strerror(errno));
            if(ret < 0)
                return -1;
        }
    }

    return 0;
}

static int cmd_setup(as_ctrl_t *asc, asc_s2d_setup_t *s)
{
    u8 c_id = get_id();
    if((s->mac[0] | s->mac[1] | s->mac[2] | s->mac[3] | s->mac[4] | s->mac[5]) != 0 &&
       (s->mac[0] != 0xff || s->mac[1] != 0xff || s->mac[2] != 0xff || s->mac[3] != 0xff || s->mac[4] != 0xff || s->mac[5] != 0xff)) {
        if(s->mac[0] != asc->my_mac[0] || s->mac[1] != asc->my_mac[1] || s->mac[2] != asc->my_mac[2] || s->mac[3] != asc->my_mac[3] || s->mac[4] != asc->my_mac[4] || s->mac[5] != asc->my_mac[5]) {
            //cmd_msg("Got SETUP to %02x-%02x-%02x-%02x-%02x-%02x (not me)\n", s->mac[0], s->mac[1], s->mac[2], s->mac[3], s->mac[4], s->mac[5]);
            return -1;
        }
    }

    if(s->id == ASCMD_S2D_ID_FORCE) {
        as_cfg_set(ASCFG_IPADDR, 4, (u8 *)&s->network);
        cmd_msg("Set fixed ip to 0x%x (id=%u)\n", s->network, c_id);
        if(s->gateway && s->netmask) {
            // legacy struct
            ipconfig_t ipcfg;
            ipcfg.gateway = s->gateway;
            ipcfg.netmask = s->netmask;
            ipcfg.dns = s->dns;
            as_cfg_set(ASCFG_IPCONFIG, sizeof(ipconfig_t), (u8 *)&ipcfg);
            cmd_msg("Set ipconfig gw:%x, mask:%x dns:%x\n", s->gateway, s->netmask, s->dns);
        }
        if(c_id)
            return 0;

        renew_ip(asc);
    }

    if(s->streams) {
        set_enc(asc, 0, &s->enc1);
        set_enc(asc, 1, (s->streams > 1) ? &s->enc2 : 0);
        asc->streams = (s->streams > 1) ? 2 : 1;
    }

    return 0;
}

// -----------------------------------------------------------------------------
// Extended Commands
// -----------------------------------------------------------------------------
static int get_enc_ex(as_ctrl_t *asc, asc_config_encoder_t *enc)
{
    if(enc->ch || enc->stream >= 2)
        return -1;

    as_media_venc_attr_t *e = &asc->enc[enc->stream];

    if(e->type == AS_MEDIA_VENC_TYPE_H264_BP)
        enc->vtype = ASF_VTYPE_H264;
    else
        enc->vtype = ASF_VTYPE_MJPEG;
    enc->fps = e->fps;
    enc->x = 0;
    enc->y = 0;
    enc->width = enc_width[e->res];
    enc->height = enc_height[e->res];
    enc->bitrate = e->bitrate * 1000;

    return 0;
}

static int set_enc_ex(as_ctrl_t *asc, asc_config_encoder_t *enc)
{
    if(enc->ch || enc->stream >= 2)
        return -1;

    as_media_venc_attr_t *e = &asc->enc[enc->stream];

    e->ch = enc->ch;
    e->stream_id = enc->stream;
    e->type = (enc->vtype == ASF_VTYPE_H264) ? AS_MEDIA_VENC_TYPE_H264_BP : AS_MEDIA_VENC_TYPE_MJPEG;
    e->fps = (enc->fps < 30) ? enc->fps : 30;
    e->gop = (e->fps < 2) ? 2 : e->fps;
    e->rc_type = AS_MEDIA_VENC_RC_VBR;
    if(enc->width >= 1920) {
        e->res = AS_MEDIA_VENC_RES_FHD;
        if(e->fps > 5) e->fps = 5;
        e->gop = e->fps;
    }
    else if(enc->width >= 1280)
        e->res = AS_MEDIA_VENC_RES_HD;
    else if(enc->width >= 704) {
        if(enc->height > 288)
            e->res = AS_MEDIA_VENC_RES_D1;
        else
            e->res = AS_MEDIA_VENC_RES_2CIF;
    }
	else if(enc->width >= 640)
		e->res = AS_MEDIA_VENC_RES_VGA;
    else if(enc->width >= 352)
        e->res = AS_MEDIA_VENC_RES_CIF;
    else //if(enc->width >= 176)
        e->res = AS_MEDIA_VENC_RES_QCIF;

    e->bitrate = ((enc->bitrate/1000) > 10000) ? 10000 : (enc->bitrate/1000);
    if(e->bitrate < 64)
        e->bitrate = 64;
    my_set_enc(e);

    cmd_msg("enc#%d: WxH:%ux%u, res=%u, fps=%u, br=%u\n", enc->stream, enc->width, enc->height, e->res, e->fps, e->bitrate);

    return 0;
}

static int setup_roi(int width, int height, asc_config_roi_t *roi)
{
#if 0
    int number = 0;
    roi_region_t rr[ASC_ROI_NUM];
    int i;
    for(i=0;i<ASC_ROI_NUM;i++) {
        if(!roi->enable[i])
            continue;
        if(roi->regions[i].top >= height || roi->regions[i].bottom >= height ||
            roi->regions[i].left >= width || roi->regions[i].right >= width)
            continue;

        rr[number].top = roi->regions[i].top;
        rr[number].left = roi->regions[i].left;
        rr[number].bottom = roi->regions[i].bottom;
        rr[number].right = roi->regions[i].right;
        number++;
    }
    va_roi_setup(number, width, height, rr);
#endif
    return 0;
}

static int setup_isp(asc_config_isp_t *isp)
{
    int n;
    for(n=0;n<ASC_ISP_NUM;n++) {
        if(isp->addr[n] == 0xffff)
            continue;
        if(as_gen_write_nt99140(isp->addr[n], &isp->data[n]) < 0) {
            cmd_err("ISP write error @ 0x%x = %x\n", isp->addr[n], isp->data[n]);
        }
        else {
            cmd_msg("ISP write %02x to %04x\n", isp->data[n], isp->addr[n]);
        }
    }
    return 0;
}

static int cmd_setup_ex(as_ctrl_t *asc, asc_s2d_setup_ex_t *s)
{
    if((s->mac[0] | s->mac[1] | s->mac[2] | s->mac[3] | s->mac[4] | s->mac[5]) != 0 &&
       (s->mac[0] != 0xff || s->mac[1] != 0xff || s->mac[2] != 0xff || s->mac[3] != 0xff || s->mac[4] != 0xff || s->mac[5] != 0xff)) {
        if(s->mac[0] != asc->my_mac[0] || s->mac[1] != asc->my_mac[1] || s->mac[2] != asc->my_mac[2] || s->mac[3] != asc->my_mac[3] || s->mac[4] != asc->my_mac[4] || s->mac[5] != asc->my_mac[5]) {
            //cmd_msg("Got SETUP_EX to %02x-%02x-%02x-%02x-%02x-%02x (not me)\n", s->mac[0], s->mac[1], s->mac[2], s->mac[3], s->mac[4], s->mac[5]);
            return -1;
        }
    }

    u32 size = s->cmd.len - sizeof(asc_s2d_setup_ex_t);
    u32 off = 0;
    while(off < size) {
        asc_config_ex_t *cfg = (asc_config_ex_t *)(s->data+off);
        u32 noff = off + sizeof(asc_config_ex_t) + cfg->size;
        if(noff > size)
            break;
        if(cfg->id == ASCMD_CONFIG_NETWORK) {
            asc_config_network_t *net = (asc_config_network_t *)cfg->data;
            if(cfg->flag & ASCMD_CONFIG_SAVE) {
                as_cfg_set(ASCFG_IPADDR, 4, (u8 *)&net->ip);
                // legacy struct
                ipconfig_t ipcfg;
                ipcfg.gateway = net->gateway;
                ipcfg.netmask = net->netmask;
                ipcfg.dns = net->dns;
                as_cfg_set(ASCFG_IPCONFIG, sizeof(ipconfig_t), (u8 *)&ipcfg);
                cmd_msg("Save network: ip=%x, gateway=%x, netmask=%x, dns=%x\n", net->ip, net->gateway, net->netmask, net->dns);
            }
            if(cfg->flag & ASCMD_CONFIG_SET) {
                setup_network(asc, net->ip, net->netmask, net->gateway, net->dns);
            }
        }
        else if(cfg->id == ASCMD_CONFIG_ENCODER) {
            asc_config_encoder_t *enc = (asc_config_encoder_t *)cfg->data;
            if(cfg->flag & ASCMD_CONFIG_SAVE) {
				printf("\n\n\n[SAVE]set enc: enc->stream = %d,enc->ch = %d,enc->fps = %d,enc->height = %d,enc->bitrate = %d,enc->vtype = %d,enc->width = %d,enc->x = %d,enc->y = %d \r\n\n\n",enc->stream,enc->ch,enc->fps,enc->height,enc->bitrate,enc->vtype,enc->width,enc->x,enc->y);
                if(enc->ch == 0 && enc->stream < 2) {
                    as_cfg_set(ASCFG_ENCODER1+enc->stream, sizeof(asc_config_encoder_t), (u8 *)enc);
                }
            }
            if(cfg->flag & ASCMD_CONFIG_SET) {
                set_enc_ex(asc, enc);
            }
        }
        else if(cfg->id == ASCMD_CONFIG_STUN) {
            asc_config_stun_t *stun = (asc_config_stun_t *)cfg->data;
            if(cfg->flag & ASCMD_CONFIG_SAVE) {
                as_cfg_set(ASCFG_STUN, sizeof(asc_config_stun_t), (u8 *)stun);
            }
            if(cfg->flag & ASCMD_CONFIG_SET) {
                obs_stun_setup(stun);
            }
        }
        else if(cfg->id == ASCMD_CONFIG_ROI) {
            asc_config_roi_t *roi = (asc_config_roi_t *)cfg->data;
            if(cfg->flag & ASCMD_CONFIG_SAVE) {
                as_cfg_set(ASCFG_ROI, sizeof(asc_config_roi_t), (u8 *)roi);
            }
            if(cfg->flag & ASCMD_CONFIG_SET) {
                if(mkf_type == AS_TYPE_720P){
					setup_roi(1280, 720, roi);}
                else if (mkf_type == AS_TYPE_1080P)
				{
					setup_roi(1920, 1080, roi);
				}
				else{
					setup_roi(704, 480, roi);}
            }
        }
        else if(cfg->id == ASCMD_CONFIG_ISP) {
            asc_config_isp_t *isp = (asc_config_isp_t *)cfg->data;
            if(cfg->flag & ASCMD_CONFIG_SAVE) {
                as_cfg_set(ASCFG_ISP, sizeof(asc_config_isp_t), (u8 *)isp);
            }
            if(cfg->flag & ASCMD_CONFIG_SET) {
                if(mkf_type == AS_TYPE_720P)
                    setup_isp(isp);
            }
        }
        else {
            cmd_err("Unsupported config: %x\n", cfg->id);
        }
        off = noff;
    }

    return 0;
}

extern int as_data_get_ftest(asc_ftest_t *ft);

static int get_hello_ex(as_ctrl_t *asc, asc_d2s_hello_ex_t *h)
{
    asc_ftest_t ft;
    as_data_get_ftest(&ft);

    h->cmd.magic = ASCMD_MAGIC;
    h->cmd.version = ASCMD_VERSION;
    h->cmd.cmd = ASCMD_D2S_HELLO_EX;
    h->cmd.flag = 0;
    h->cmd.len = sizeof(asc_d2s_hello_ex_t);

    h->fw_ver = as_gen_fw_get_version_num();
    h->fw_build = FW_BUILD;
    memcpy(h->serial_number, ft.serial_number, 32);
    memcpy(h->mac, asc->my_mac, 6);
    h->model = mkf_model;
    h->type = mkf_type;
    h->ver = 0;
    h->id = get_id();
    h->link = ft.link;
    h->dio = ft.dio;
    h->mode = ft.mode;
    h->status = ft.status;

    u32 off = 0;
    // network
    asc_config_ex_t *cfg = (asc_config_ex_t *)h->data;
    cfg->id = ASCMD_CONFIG_NETWORK;
    cfg->flag = ASCMD_CONFIG_SAVE;
    cfg->size = sizeof(asc_config_network_t);
    asc_config_network_t *net = (asc_config_network_t *)cfg->data;
    as_cfg_get(ASCFG_IPADDR, 4, (u8 *)&net->ip);
    as_cfg_get(ASCFG_IPCONFIG, sizeof(ipconfig_t), (u8 *)&net->gateway);
    off += (sizeof(asc_config_ex_t) + cfg->size);
    // encoder 1
    cfg = (asc_config_ex_t *)(h->data+off);
    cfg->id = ASCMD_CONFIG_ENCODER;
    cfg->flag = ASCMD_CONFIG_SAVE | ASCMD_CONFIG_SET;
    cfg->size = sizeof(asc_config_encoder_t);
    asc_config_encoder_t *enc = (asc_config_encoder_t *)cfg->data;
    enc->ch = 0;
    enc->stream = 0;
    get_enc_ex(asc, enc);
    off += (sizeof(asc_config_ex_t) + cfg->size);
    // encoder 2
    cfg = (asc_config_ex_t *)(h->data+off);
    cfg->id = ASCMD_CONFIG_ENCODER;
    cfg->flag = ASCMD_CONFIG_SAVE | ASCMD_CONFIG_SET;
    cfg->size = sizeof(asc_config_encoder_t);
    enc = (asc_config_encoder_t *)cfg->data;
    enc->ch = 0;
    enc->stream = 1;
    get_enc_ex(asc, enc);
    off += (sizeof(asc_config_ex_t) + cfg->size);
    // stun
    cfg = (asc_config_ex_t *)(h->data+off);
    cfg->id = ASCMD_CONFIG_STUN;
    cfg->flag = ASCMD_CONFIG_SAVE | ASCMD_CONFIG_SET;
    cfg->size = sizeof(asc_config_stun_t);
    asc_config_stun_t *stun = (asc_config_stun_t *)cfg->data;
    obs_stun_info(stun);
    off += (sizeof(asc_config_ex_t) + cfg->size);
    // roi
    cfg = (asc_config_ex_t *)(h->data+off);
    cfg->id = ASCMD_CONFIG_ROI;
    cfg->flag = ASCMD_CONFIG_SAVE | ASCMD_CONFIG_SET;
    cfg->size = sizeof(asc_config_roi_t);
    asc_config_roi_t *roi = (asc_config_roi_t *)cfg->data;
    if(as_cfg_get(ASCFG_ROI, sizeof(asc_config_roi_t), (u8 *)roi) < 0)
        memset(roi, 0, sizeof(asc_config_roi_t));
    off += (sizeof(asc_config_ex_t) + cfg->size);
    // isp
    if(mkf_type == AS_TYPE_720P) {
        cfg = (asc_config_ex_t *)(h->data+off);
        cfg->id = ASCMD_CONFIG_ISP;
        cfg->flag = ASCMD_CONFIG_SAVE | ASCMD_CONFIG_SET;
        cfg->size = sizeof(asc_config_isp_t);
        asc_config_isp_t *isp = (asc_config_isp_t *)cfg->data;
        if(as_cfg_get(ASCFG_ISP, sizeof(asc_config_isp_t), (u8 *)isp) < 0)
            memset(isp, 0xff, sizeof(asc_config_isp_t));
        off += (sizeof(asc_config_ex_t) + cfg->size);
    }

    h->cmd.len += off;

    return h->cmd.len;
}

static int send_hello_ex(as_ctrl_t *asc, struct sockaddr_in *dest, struct sockaddr_in *bcast)
{
    asc_d2s_hello_ex_t *h = (asc_d2s_hello_ex_t *)(asc->sbuf);
    int hsize = get_hello_ex(asc, h);

    if(bcast && bcast->sin_addr.s_addr != 0 && bcast->sin_port != 0) {
        int ret = sendto(asc->sd, h, hsize, 0, (struct sockaddr *)bcast, sizeof(struct sockaddr_in));
        if(ret != hsize) {
            cmd_err("send HELLO_EX error, %d, %s\n", errno, strerror(errno));
            if(ret < 0)
                return -1;
        }
    }

    if(dest && dest->sin_addr.s_addr != 0 && dest->sin_port != 0) {
        int ret = sendto(asc->sd, h, hsize, 0, (struct sockaddr *)dest, sizeof(struct sockaddr_in));
        if(ret != hsize) {
            cmd_err("send HELLO_EX error, %d, %s\n", errno, strerror(errno));
            if(ret < 0)
                return -1;
        }
    }
    return 0;
}

// -----------------------------------------------------------------------------
static int cmd_parser(as_ctrl_t *asc)
{
    as_cmd_t *cmd = (as_cmd_t *)asc->rbuf;
    if(cmd->magic != ASCMD_MAGIC || cmd->version != ASCMD_VERSION) {
        cmd_err("Got unknown packet %d bytes, from %s\n", asc->r_size, inet_ntoa(asc->peer.sin_addr));
        return -1;
    }
    if(!ASCMD_S2D_VALID(cmd->cmd)) {
        cmd_err("Got unknown cmd: 0x%x, from %s\n", cmd->cmd, inet_ntoa(asc->peer.sin_addr));
        return -1;
    }
    cmd_msg("cmd:%x from %08x:%u %d\n", cmd->cmd, ntohl(asc->peer.sin_addr.s_addr), ntohs(asc->peer.sin_port), asc->r_size);

    switch(cmd->cmd) {
        case ASCMD_S2D_DISCOVERY:
            if(cmd->flag)
                send_hello_ex(asc, &asc->peer, &asc->bcast);
            else
                send_hello(asc, 0, &asc->bcast);
            break;
        case ASCMD_S2D_SETUP:
            if(asc->r_size < SETUP_SZ || cmd->len < SETUP_SZ) {
                cmd_err("SETUP len error: recv:%d, %u != %u, from %s\n", asc->r_size, cmd->len, SETUP_SZ, inet_ntoa(asc->peer.sin_addr));
                break;
            }
            cmd_err("SETUP recv:%d from %s: %x\n", asc->r_size, inet_ntoa(asc->peer.sin_addr), asc->peer.sin_port);
            if(cmd_setup(asc, (asc_s2d_setup_t *)cmd) == 0)
                send_hello(asc, 0, &asc->bcast);
            break;
        case ASCMD_S2D_SETUP_EX:
            if(asc->r_size < sizeof(asc_s2d_setup_ex_t) || asc->r_size != cmd->len) {
                cmd_err("SETUP EX len error: recv:%d != %u, from %s\n", asc->r_size, cmd->len, inet_ntoa(asc->peer.sin_addr));
                break;
            }
            cmd_err("SETUP EX recv:%d from %s: %x\n", asc->r_size, inet_ntoa(asc->peer.sin_addr), asc->peer.sin_port);
            if(cmd_setup_ex(asc, (asc_s2d_setup_ex_t *)cmd) == 0){
                printf("[cmd_setup_ex] send_hello_ex check change\n");
				send_hello_ex(asc, &asc->peer, &asc->bcast);}
            break;
        default:
            break;
    }
    return 0;
}

static void *cmd_thread(void *arg)
{
    cmd_msg("[CMD] thread start .....PID: %u\n", (u32)gettid());

    as_ctrl_t *asc = (as_ctrl_t *)arg;

    asc->my_addr.sin_family = PF_INET;
    asc->my_addr.sin_port = htons(CMD_DEV_PORT);
    asc->my_addr.sin_addr.s_addr = INADDR_ANY;
    asc->bcast.sin_family = PF_INET;
    asc->bcast.sin_port = htons(CMD_SERV_PORT);
    asc->bcast.sin_addr.s_addr = INADDR_BROADCAST;

    asc->sd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(asc->sd < 0) {
        cmd_err("BC socket error: %s", strerror(errno));
        goto _cmd_exit;
    }
    else {
        int val = 1;
        if(setsockopt(asc->sd, SOL_SOCKET, SO_BROADCAST, &val, sizeof(val)) < 0) {
            cmd_err("set SO_BROADCAST error: %s", strerror(errno));
            goto _cmd_exit;
        }
        if(setsockopt(asc->sd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0) {
            cmd_err("set SO_REUSEADDR error: %s", strerror(errno));
            goto _cmd_exit;
        }
        if(bind(asc->sd, (struct sockaddr *)&asc->my_addr, sizeof(struct sockaddr)) < 0) {
            cmd_err("bind error: %s", strerror(errno));
            goto _cmd_exit;
        }
    }


    //send_hello(asc, &asc->bcast);
    send_hello_ex(asc, 0, &asc->bcast);

    time_t last = 0;

    while(1) {
        fd_set rds;
        struct timeval tv;
        int retval;

        FD_ZERO(&rds);
        FD_SET(asc->sd, &rds);

        tv.tv_sec = 1;
        tv.tv_usec = 0;

        retval = select(asc->sd+1, &rds, 0, 0, &tv);

        if(retval < 0) {
            cmd_err("select error: %s\n", strerror(errno));
            continue;
        }
        else if(retval) {
            if(FD_ISSET(asc->sd, &rds)) {
                socklen_t addr_len = sizeof(asc->peer);
                asc->r_size = recvfrom(asc->sd, asc->rbuf, CMD_BSIZE, 0, (struct sockaddr *)&asc->peer, &addr_len);
                if(asc->r_size < 0) {
                    asc->recv_err++;
                    cmd_err("[%u] recvfrom error:%s\n", asc->recv_err, strerror(errno));
                    continue;
                }
                asc->recv_err = 0;
                if(asc->r_size)
                    cmd_parser(asc);
            }
        }

        time_t now = time(0);
        if(now < last || (now - last) > 30) {
            // broadcast HELLO if no connection in last 60 seconds
            last = now;
        }
		if (c_flag == 1)
		{
			printf("[CMD] Send change encoder \n");
			send_hello_ex(asc, &asc->peer, &asc->bcast);
			c_flag = 0;
		}
    }
_cmd_exit:
    if(asc->sd >= 0)
        close(asc->sd);

    pthread_exit("CMD thread exit");
}

// ----------------------------------------------------------------------------
// APIs
// ----------------------------------------------------------------------------
/*!
 * @brief       to init CMD thread
 * @return      0 on success, otherwise < 0
 */
int as_cmd_init(void)
{
    mkf_type =AS_TYPE_1080P; // (as_codec_is_720p()) ? AS_TYPE_720P : AS_TYPE_D1;
    cmd_msg("Codec type = %d\n", mkf_type);
    as_zero(&as_ctrl, sizeof(as_ctrl));
    get_hw_addr(ETH_NAME, &as_ctrl.raw_addr);
    as_copy(as_ctrl.my_mac, as_ctrl.raw_addr.sll_addr, 6);
    get_ip(ETH_NAME, &as_ctrl.my_ip);

    as_media_venc_attr_t *e1 = &as_ctrl.enc[0];
    asc_config_encoder_t cenc;
    if(as_cfg_get(ASCFG_ENCODER1, sizeof(asc_config_encoder_t), (u8 *)&cenc) < 0) {
        e1->type = AS_MEDIA_VENC_TYPE_H264_BP;
        e1->ch = 0;
        e1->stream_id = 0;
        e1->rc_type = AS_MEDIA_VENC_RC_VBR;
        if(mkf_type == AS_TYPE_720P) {
            e1->fps = 15;
            e1->gop = 15;
            e1->res = AS_MEDIA_VENC_RES_HD;
            e1->bitrate = 4000;
        }
        else {
            e1->fps = 1;
            e1->gop = 30;
            e1->res = AS_MEDIA_VENC_RES_FHD;
            e1->bitrate = 2000;
        }
    }
    else {
        e1->type = (cenc.vtype == ASF_VTYPE_H264) ? AS_MEDIA_VENC_TYPE_H264_BP : AS_MEDIA_VENC_TYPE_MJPEG;
        e1->ch = 0;
        e1->stream_id = 0;
        e1->fps = cenc.fps;
        e1->gop = (cenc.fps < 2) ? 2 : cenc.fps;
        e1->rc_type = AS_MEDIA_VENC_RC_VBR;
        e1->res = wh_to_res(cenc.width, cenc.height);
        e1->bitrate = cenc.bitrate / 1000;
        cmd_msg("[0] %x fps=%u, res=%u, br=%u\n", e1->type, e1->fps, e1->res, e1->bitrate);
    }
    if(my_set_enc(e1) < 0) {
        cmd_err("Enc 0 setup error\n");
    }

    e1 = &as_ctrl.enc[1];
    if(as_cfg_get(ASCFG_ENCODER2, sizeof(asc_config_encoder_t), (u8 *)&cenc) < 0) {
        e1->type = AS_MEDIA_VENC_TYPE_H264_BP;
        e1->ch = 0;
        e1->stream_id = 1;
        e1->rc_type = AS_MEDIA_VENC_RC_VBR;
        if(mkf_type == AS_TYPE_720P) {
            e1->fps = 5;
            e1->gop = 5;
            e1->res = AS_MEDIA_VENC_RES_HD;
            e1->bitrate = 500;
        }
        else {
            e1->fps = 1;
            e1->gop = 5;
            e1->res = AS_MEDIA_VENC_RES_FHD;
            e1->bitrate = 500;
        }
    }
    else {
        e1->type = (cenc.vtype == ASF_VTYPE_H264) ? AS_MEDIA_VENC_TYPE_H264_BP : AS_MEDIA_VENC_TYPE_MJPEG;
        e1->ch = 0;
        e1->stream_id = 1;
        e1->fps = cenc.fps;
        e1->gop = (cenc.fps < 2) ? 2 : cenc.fps;
        e1->rc_type = AS_MEDIA_VENC_RC_VBR;
        e1->res = wh_to_res(cenc.width, cenc.height);
        e1->bitrate = cenc.bitrate / 1000;
        cmd_msg("[1] %x fps=%u, res=%u, br=%u\n", e1->type, e1->fps, e1->res, e1->bitrate);
    }
    if(my_set_enc(e1) < 0) {
        cmd_err("Enc 0 setup error\n");
    }

    as_ctrl.streams = 2;

    renew_ip(&as_ctrl);

    // STUN
    asc_config_stun_t stun;
    as_cfg_get(ASCFG_STUN, sizeof(asc_config_stun_t), (u8 *)&stun);
    obs_init(as_ctrl.my_mac, as_gen_fw_get_version_num());
    obs_stun_setup(&stun);

    // ROI
#if 0
    asc_config_roi_t roi;
    if(as_cfg_get(ASCFG_ROI, sizeof(asc_config_roi_t), (u8 *)&roi) == 0) {
        if(mkf_type == AS_TYPE_720P){
			setup_roi(1280, 720, &roi);}
		else if (mkf_type == AS_TYPE_1080P)
		{
			setup_roi(1920, 1080, &roi);
		}
        else{
			setup_roi(704, 480, &roi);}
    }
#else
    if(mkf_type == AS_TYPE_720P){
		va_roi_setup(0, 1280, 720, 0);}
	else if (mkf_type == AS_TYPE_1080P)
	{
		va_roi_setup(0,1920, 1080, 0);
	}
    else{
		va_roi_setup(0, 704, 480, 0);}
#endif

    // ISP
    asc_config_isp_t isp;
    if(as_cfg_get(ASCFG_ISP, sizeof(asc_config_isp_t), (u8 *)&isp) == 0) {
        if(mkf_type == AS_TYPE_720P) {
            setup_isp(&isp);
        }
    }

    msleep(500);

    create_norm_thread(cmd_thread, &as_ctrl);

    return 0;
}

const char *vs1_get_userdata(int a)
{
    FILE *fp;
	char path[16];
	if (a==-1)
	{
		sprintf(path,"ROI.txt");
	} 
	else
	{
		sprintf(path,"ROI2.txt");
	}

	//printf("vs1_get_uesrdata\n\n");
	fp = fopen(path,"r");
	if (fp)
	{
		fread(user_data,4088,1,fp);
		fclose(fp);
	}	
	printf("%s\n",user_data);
    return user_data;
}

int vs1_set_userdata(char *buf, int size)
{
    if(size > ASC_USERDATA_SIZE)
        return -1;
    memset(user_data, 0, ASC_USERDATA_SIZE);
    memcpy(user_data, buf, size);
	FILE *fp;

	fp = fopen("ROI.txt","wb");
		if (fp)
		{
			fwrite(user_data,1,ASC_USERDATA_SIZE,fp);
			fclose(fp);
		}

    /*if(as_cfg_set(ASCFG_USER, ASC_USERDATA_SIZE, (u8 *)user_data) != 0)
        return -1;*/
    va_change();
    return 0;
}

int encoder_set(void)
{
	as_media_venc_attr_t amva[1];
	asc_config_encoder_t enc[1];
	enc->stream = 0;
	enc->ch = 0;enc->fps = 1;
	enc->bitrate = 1000000;enc->vtype = ASF_VTYPE_H264;
	enc->width = 1920;enc->height = 1080;
	enc->x = 0;enc->y = 0; 
	printf("set_encoder test\n");
	amva->type = (enc->vtype == ASF_VTYPE_H264) ? AS_MEDIA_VENC_TYPE_H264_BP : AS_MEDIA_VENC_TYPE_MJPEG;
	amva->ch = enc->ch;
	amva->stream_id = enc->stream;
	amva->fps = (enc->fps < 30) ? enc->fps : 30;
	amva->gop = (amva->fps < 2) ? 2 : amva->fps;
	amva->rc_type = AS_MEDIA_VENC_RC_VBR;
	amva->res = AS_MEDIA_VENC_RES_FHD;
	amva->bitrate = ((enc->bitrate/1000) > 10000) ? 10000 : (enc->bitrate/1000);
    if(amva->bitrate < 64)
        amva->bitrate = 64;

	as_cfg_set(ASCFG_ENCODER1+enc->stream, sizeof(asc_config_encoder_t), (u8 *)enc);
	my_set_enc(amva);

	c_flag = 1;
	return 0;
}

