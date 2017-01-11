/*!
 *  @file       as_data.c
 *  @version    1.0
 *  @date       08/11/2012
 *  @author     Jacky Hsu <jacky.hsu@altasec.com>
 *  @note       Data management task
 *              Copyright (C) 2012 ALTASEC Corp.
 */

#include "as_headers.h"
#include "as_platform.h"
#include "as_dm.h"
#include "as_generic.h"
#include "as_media.h"
#include "as_protocol.h"
#include "obss_api.h"
#include "vs1_api.h"
#include "misc_api.h"

#include <linux/sockios.h>
#include <sys/wait.h>
// VS2 don't need
//#include <bluetooth/bluetooth.h>
//#include <bluetooth/rfcomm.h>
#define ADO_FLOW_CONTROL
// ----------------------------------------------------------------------------
// Debug
// ----------------------------------------------------------------------------
#define d_err(format,...)  printf("[D]%s(%d): " format, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define d_msg(format,...)  printf("[D] " format, ##__VA_ARGS__)

// ----------------------------------------------------------------------------
// Type Define
// ----------------------------------------------------------------------------
/*!
 *  @struct frame_t
 *  @brief  frame attribute
 */
typedef struct {

    u8      subtype;
    u8      pl;
    u8      pl_num;
    u8      is_key;
    u8      flags;
    u8      resv[3];
    u16     w;      //!< bitwidth of audio
    u16     h;      //!< sample rate of audio
    u32     seq;
    u32     sec;
    u32     usec;

} frame_t;

#define FNMASK          255
#define FN              256
#define FN_SIZE(X,Y)    (((X) - (Y)) & FNMASK)
#define PLMASK          255
#define PLN             256
/*!
 *  @struct data_queue_t
 *  @brief  data queue
 */
typedef struct {

    int     gotk;
    u16     w;
    u16     h;
    u32     seq;
    u32     wi;
    u32     ki;
    frame_t f[FN];

    u32     pli;
    struct iovec pl[PLN];

} data_queue_t;

/*!
 *  @struct client_t
 *  @brief  client attr
 */
typedef struct {

    int     cid;
    int     sd;
    int     ch;
    int     id;
    u16     mode;
    int     motion_only;
    int     ext_mode;

    u32     ari;
    u32     vri;
    int     gotk;
    u32     aseq;
    u32     vseq;
    as_frame_t  fh;
    u32     s_size[FN];

    u32     zero_count;
    u32     last_send;
    u32     status;
    u32     last_status;
    u8      *rbuf;
    u8      *sbuf;
    struct sockaddr_in  peer_addr;

} client_t;

#define CH_NR       1
#define STREAM_NR   4
#define CLIENT_NR   32

#define CMD_BSIZE   8192

#define ASCMD_SZ    sizeof(as_cmd_t)
#define ASFH_SZ     sizeof(as_frame_t)
#define ASYFH_SZ    sizeof(as_yframe_t)
#define ASSH_SZ     sizeof(as_status_t)
#define ASC_FT_SZ   sizeof(asc_ftest_t)

// ----------------------------------------------------------------------------
// Local variable
// ----------------------------------------------------------------------------
static int max_ch = 0;
static int max_stream = 0;
static int max_client = 0;
static data_queue_t     v_streams[CH_NR][STREAM_NR];
static data_queue_t     a_streams[CH_NR];
static client_t         clients[CLIENT_NR];
static client_t         clients1[CLIENT_NR];

static u32 status_led = 0;
static u32 v_loss = 0;
static u32 do_status = 0;
static u32 di_status = 0;

#define UPGRADE_SIZE    0x1000000
static int in_upgrade = 0;
static int upgrade_status = 0;
static u8 *upgrade_buf = 0;
static u32 upgrade_size = 0;
static int to_reset = 0;
static int in_reset = 0;

#define AUDIO_BSIZE     0x100000
static u8 *audio_buf = 0;
static u32 audio_woff = 0;

static int motion_status = 0;

static u32 obs_flags = 0;

// BT RFCOMM out
#define BT_OUT_SZ   1024
static pthread_mutex_t bt_mutex = PTHREAD_MUTEX_INITIALIZER;
static int  bt_out_inuse = 0;
static int  bt_out_cmd = 0;
static int  bt_out_status = 0;
static u8   bt_out_ch;
static u8   bt_out_mac[6];
static char bt_out_pin[64];
static int  bt_out_size = 0;
static u8   bt_out_data[BT_OUT_SZ];

// Audio output
static int ao_on = 0;

// ROI
static int roi_cid = -1;

// ----------------------------------------------------------------------------
// Local Functions
// ----------------------------------------------------------------------------
static inline void update_obs_flags(void)
{
    u32 cflags = di_status | (do_status << 2) | (v_loss ? ASF_FLAG_VLOSS : 0) | (motion_status ? ASF_FLAG_MOTION : 0);
    if(cflags != obs_flags) {
        obs_set_dio(cflags);
        obs_flags = cflags;
    }
}

static int c_recv(client_t *cc, u8 *buf, int size)
{
    int r_total = 0;

    while(r_total < size) {
        int r_size = recv(cc->sd, buf+r_total, size-r_total, 0);
        if(r_size < 0) {
            d_err("[%d] recv error:%s\n", cc->cid, strerror(errno));
            return -1;
        }
        else if(r_size == 0)
            break;
        r_total += r_size;
    }
    if(r_total != size)
        d_err("[%d] recv error: exp=%u, got=%u\n", cc->cid, size, r_total);
    return r_total;
}

static void do_isp(u16 *data, u16 num)
{
    int i;
    num = (num > 256) ? 256 : num;
    for(i=0;i<num;i++) {
        u32 reg = data[i*2];
        u8 value = data[i*2+1] & 0xff;
        if(as_gen_write_nt99140(reg, &value) < 0) {
            d_err("ISP write error @ 0x%x = %x\n", reg, value);
            break;
        }
        d_msg("ISP write 0x%x to 0x%04x\n", value, reg);
    }
    d_msg("ISP %u written\n", i);
}

static void set_do(u32 status)
{
    u32 mask = (status >> 8) & 0xff;
    status &= 15;
    if(mask) {
        if((mask & 2) == 0)
        status = (status & ~2) | ((do_status & 2) ? 2 : 0);
        else
            //s_gen_set_do(status & 2);
            as_gen_set_green_led(status & 2); // DO2
        if((mask & 1) == 0)
        status = (status & ~1) | ((do_status & 1) ? 1 : 0);
        else
            as_gen_set_red_led(status & 1); // DO1
          //  as_gen_set_do(status & 1);

        if((mask & 4) == 0)
        status = (status & ~4) | ((do_status & 4) ? 4 : 0);
        else
          //  as_gen_set_red_led(status & 1); // DO3
            as_gen_set_do(status & 4);

        if((mask & 8) == 0)
        status = (status & ~8) | ((do_status & 8) ? 8 : 0);
        else
          //  as_gen_set_red_led(status & 1); // DO4
            as_gen_set_do(status & 8);
 
   }
    else {
			as_gen_set_do(status);
        //if (status == 1 ||status == 2){
        //	as_gen_set_red_led(status & 1); // DO1
        //	as_gen_set_green_led(status & 2);} // DO2
        //else if (status == 8 ||status == 4){
        //	as_gen_set_green_led1(status & 4); // DO3
        //	as_gen_set_red_led1(status & 8);} // DO4

    }

    if(do_status != status) {
        d_msg("DO change: %x -> %x\n", do_status, status);
        do_status = status;
    }
    update_obs_flags();
}

static int my_recv(client_t *cc, int size, u8 *buf)
{
    int r_total = 0;

    while(r_total < size) {
        int r_size = recv(cc->sd, buf+r_total, size-r_total, 0);
        if(r_size < 0) {
            d_err("[%d] recv error:%s\n", cc->cid, strerror(errno));
            break;
        }
        else if(r_size == 0) {
            if(++cc->zero_count > 10) {
                d_err("[%d] recv 0 byte\n", cc->cid);
                break;
            }
        }
        r_total += r_size;
    }
    if(r_total != size) {
        d_err("[%d] recv error: exp=%u, got=%u\n", cc->cid, size, r_total);
        return -1;
    }
    return 0;
}

static int send_cmd(client_t *cc, u16 cid, u16 flag)
{
    as_cmd_t *cmd = (as_cmd_t *)cc->sbuf;
    cmd->magic = ASCMD_MAGIC;
    cmd->version = ASCMD_VERSION;
    cmd->cmd = cid;
    cmd->flag = flag;
    cmd->len = ASCMD_SZ;
    if(send(cc->sd, cmd, ASCMD_SZ, 0) != ASCMD_SZ) {
        d_err("[%d] send failed: %s\n", cc->cid, strerror(errno));
        return -1;
    }
    return 0;
}

static int send_cmd_data(client_t *cc, u16 cid, u16 flag, u8 *buf, int bsize)
{
    as_cmd_t *cmd = (as_cmd_t *)cc->sbuf;
    cmd->magic = ASCMD_MAGIC;
    cmd->version = ASCMD_VERSION;
    cmd->cmd = cid;
    cmd->flag = flag;
    cmd->len = ASCMD_SZ + ((buf) ? bsize : 0);
    if(send(cc->sd, cmd, ASCMD_SZ, 0) != ASCMD_SZ) {
        d_err("[%d] send failed: %s\n", cc->cid, strerror(errno));
        return -1;
    }
    if(bsize && buf) {
        if(send(cc->sd, buf, bsize, 0) != bsize) {
            d_err("[%d] send failed: %s\n", cc->cid, strerror(errno));
            return -1;
        }
    }
    return 0;
}

static int bt_rfcomm(client_t *cc, u32 size)
{
    asc_bt_rfcomm_t *br = (asc_bt_rfcomm_t *)cc->rbuf;

    if(size > (CMD_BSIZE-ASCMD_SZ) || my_recv(cc, size, cc->rbuf+ASCMD_SZ) < 0) {
        send_cmd(cc, ASCMD_D2S_BT_RFCOMM, (u16)-1);
        return -1;
    }
    if(br->size >= BT_OUT_SZ) {
        send_cmd(cc, ASCMD_D2S_BT_RFCOMM, (u16)-1);
        return -1;
    }
    memcpy(bt_out_mac, br->mac, 6);
    bt_out_ch = br->channel;
    strncpy(bt_out_pin, br->pin, 63);
    bt_out_pin[63] = 0;
    bt_out_size = br->size;
    memcpy(bt_out_data, br->data, br->size);
    bt_out_status = 1;
    bt_out_cmd = 1;
    send_cmd(cc, ASCMD_D2S_BT_RFCOMM, (u16)0);
    return 0;
}

static int mf_test(client_t *cc, u16 id, u16 param)
{
    u16 flag = 0;
    switch(id) {
        case 0: // RS485
            if(param == 0) {
                if(as_gen_rs485_test() < 0) {
                    flag = 0xffff;
                    d_err("RS485 TEST FAILED\n");
                }
                else {
                    flag = 0;
                    d_msg("RS485 TEST OK\n");
                }
            }
            break;
        case 1: // USB
            if(param == 0) {
                int st;
                if(as_gen_usb_status(&st) < 0) {
                    flag =0xffff;
                    d_err("USB TEST FAILED\n");
                }
                else {
                    if(st < 0) {
                        flag = 0xfffe;
                        d_msg("USB TEST RESULT FAILED\n");
                    } else {
                        flag = 0;
                        d_msg("USB TEST OK\n");
                    }
                }
            }
            break;
        default:
            d_err("unknown mftest id = %u, p=%u\n", id, param);
            break;
    }
    send_cmd(cc, ASCMD_MFTEST+id, flag);
    return 0;
}

#ifdef ADO_FLOW_CONTROL
#define ADO_AVG     50
#define ADO_HILVL   60
static u32 ado_last = 0;
static u32 ado_count = 0;
static u32 ado_drop = 0;
#endif

static int do_audio_out(client_t *cc, u16 cmd, u32 size)
{
    if(!size)
        return 0;

    if(size > AUDIO_BSIZE) {
        d_err("[%d] audio out oversized : %u\n", cc->cid, size);
        // recv all ?
        return -1;
    }

    int r_total = 0;

    while(r_total < size) {
        int r_size = recv(cc->sd, audio_buf+r_total, size-r_total, 0);
        if(r_size < 0) {
            d_err("[%d] recv error:%s\n", cc->cid, strerror(errno));
            break;
        }
        else if(r_size == 0) {
            if(++cc->zero_count > 10) {
                d_err("[%d] recv 0 byte\n", cc->cid);
                break;
            }
        }
        r_total += r_size;
    }
    if(r_total != size) {
        d_err("[%d] recv error: exp=%u, got=%u\n", cc->cid, size, r_total);
        return -1;
    }
    cc->zero_count = 0;

#ifdef ADO_FLOW_CONTROL
    ado_count++;
    u32 now = (u32)time(0);
    if(now != ado_last) {
        if(ado_drop)
            d_err("[%d:%u] %u, %u dropped\n", cc->cid, now, ado_count, ado_drop);
        ado_count = 0;
        ado_drop = 0;
        ado_last = now;
    }
    if(ado_count > ADO_HILVL) {
        ado_drop++;
        return 0;
    }
#endif

    //d_dbg("[%d] audio out recv %u\n", cc->cid, size);
    //d_msg("[JM][%d]audio out recv %u\n", cc->cid, size);
    dm_data_t data;

    if(cmd == 0) { // raw ADPCM data
        data.type = DM_DT_AUDIO;
        data.sub_type = PT_DVI4;
        data.pl_num = 1;
        data.payloads[0].iov_base = audio_buf;
        data.payloads[0].iov_len = size;
        data.samplerate = 22000;
        data.bitwidth = 16;
/*
        if(ao_on)
            ao_on = 0;
*/
        media_adec_out(&data);
    }
    else { // FH + DATA
        d_msg("[JM]FH + DATA\n");
    }

    return 0;
}

static int do_ftest(client_t *cc, u16 cmd, u32 size)
{
    if(size) {
        int r_size = recv(cc->sd, cc->rbuf+ASCMD_SZ, size, 0);
        if(r_size < 0)
            d_err("[%d] recv error:%s\n", cc->cid, strerror(errno));
        else if(r_size != size) {
            d_err("[%d] recv error: exp=%u, got=%u\n", cc->cid, size, r_size);
            return -1;
        }
    }

    asc_ftest_t *ft = (asc_ftest_t *)cc->rbuf;

    if(cmd == 0) {
        d_msg("FTEST: test only\n");
        set_do((ft->dio & 0x0c) >> 2);

        if(ft->dio & 0x80) {
            as_gen_set_status_led(1);
            status_led = 1;
        }
        else {
            as_gen_set_status_led(0);
            status_led = 0;
        }
        if(ft->dio & 0x40) {
           if(ft->dio & 0x20)
                as_gen_set_video_channel(1);
           else as_gen_set_video_channel(0);
        }
        d_msg("Set DIO=%u (%x %x %x)\n", ft->dio, di_status, do_status, status_led);
    }
    else if(cmd == 1) {

        d_msg("Set MAC: %02x-%02x-%02x-%02x-%02x-%02x\n", ft->mac[0], ft->mac[1], ft->mac[2], ft->mac[3], ft->mac[4], ft->mac[5]);
        d_msg("Set SN: %30s\n", ft->serial_number);

        as_gen_set_mac(ft->mac);
        as_gen_set_serial_num(ft->serial_number);

        msleep(50);
    }

    as_zero(ft, ASC_FT_SZ);
    ft->cmd.magic = ASCMD_MAGIC;
    ft->cmd.version = ASCMD_VERSION;
    ft->cmd.cmd = ASCMD_D2S_FTEST;
    ft->cmd.flag = 0;
    ft->cmd.len = ASC_FT_SZ;

    as_gen_get_mac(ft->mac);
    as_gen_get_serial_num(ft->serial_number, 32);
    d_msg("Get MAC: %02x-%02x-%02x-%02x-%02x-%02x\n", ft->mac[0], ft->mac[1], ft->mac[2], ft->mac[3], ft->mac[4], ft->mac[5]);
    d_msg("Get SN: %30s\n", ft->serial_number);
    ft->link = 0;
    if(v_loss)
        ft->link |= 0x04;
    ft->dio = di_status | (do_status << 2);
    if(status_led)
        ft->dio |= 0x80;
    d_msg("Get DIO=%u (%x %x %x)\n", ft->dio, di_status, do_status, status_led);

    u32 id = 0;
    as_gen_read_dip_switch(&id);
    ft->id = id & 0xff;
    d_msg("Get ID=%u\n", ft->id);

    if(in_upgrade == 0) {
        ft->mode = ASS_MODE_NORMAL;
        ft->status = 0;
    }
    else if(in_upgrade == 1) {
        u32 status = 0;
        if(as_gen_fw_status(&status) < 0) {
            ft->mode = ASS_MODE_UPGRADED;
            ft->status = ASS_STATUS_FAILED;
        }
        else {
            ft->mode = ASS_MODE_UPGRADING;
            ft->status = (status < 100) ? status : 100;
        }
    }
    else {
        ft->mode = ASS_MODE_UPGRADED;
        ft->status = upgrade_status;
    }
    d_msg("Get Upgrade status= %u, %u\n", ft->mode, ft->status);

    if(send(cc->sd, ft, ASC_FT_SZ, 0) != ASC_FT_SZ) {
        d_err("[%d] send failed: %s\n", cc->cid, strerror(errno));
        return -1;
    }

    return 0;
}

static void do_upgrade(client_t *cc, u32 size)
{
    in_upgrade = 0;
    upgrade_status = 0;

    if(!size) {
        in_upgrade = 2;
        upgrade_status = ASS_STATUS_RECV_ERR;
        return;
    }

    u32 off = 0;
    do {
        int r_size = recv(cc->sd, upgrade_buf+off, size-off, 0);
        if(r_size < 0) {
            d_err("[%d] recv error:%s\n", cc->cid, strerror(errno));
            upgrade_status = ASS_STATUS_RECV_ERR;
            in_upgrade = 2;
            return;
        }
        else if(r_size == 0)
            msleep(50);
        off += r_size;
    }while(off < size);

    upgrade_size = size;
    in_upgrade = 1;
}

static int set_userdata(client_t *cc, u32 size)
{
    int rsize = 0;
    int off = ASCMD_SZ;
    while(rsize < size) {
        int ret = recv(cc->sd, (char *)cc->rbuf+off, size-rsize, 0);
        if(ret < 0) {
            d_err("[%d] recv error:%s\n", cc->cid, strerror(errno));
            return -1;
        }
        off += ret;
        rsize += ret;
    }
    cc->rbuf[off] = 0;
    d_msg("Set UserData: %d\n", size);
    if(vs1_set_userdata(cc->rbuf+ASCMD_SZ, size) == 0)
        send_cmd_data(cc, ASCMD_D2S_USERDATA, 1, 0, 0);
    //else
    //    send_cmd_data(cc, ASCMD_D2S_USERDATA, 0xffff, 0, 0);
    return 0;
}

static int send_audio(client_t *cc)
{


    data_queue_t *dq = &a_streams[cc->ch];
    frame_t *f = 0;

    if(cc->ari >= FN || (FN_SIZE(dq->wi, cc->ari) > 10)) {
        if(cc->ari < FN)
            d_err("[%d] audio gap too large, w/r=%u/%u\n", cc->cid, dq->wi, cc->ari);
        cc->ari = dq->wi;
        cc->aseq = 0;
    }

    if(cc->ari == dq->wi)
        return 0;

    f = &dq->f[cc->ari];

    if(cc->aseq && cc->aseq != f->seq) {
        d_err("[%d] wrong seq: %u != %u @ %u\n", cc->cid, f->seq, cc->aseq, cc->ari);
        cc->ari = dq->wi;
        cc->aseq = 0;
        return 0;
    }

    as_frame_t *fh = &cc->fh;
    fh->type = ASF_TYPE_AUDIO;
    fh->magic = ASF_MAGIC;
    fh->size = 0;
    fh->sec = f->sec;
    fh->usec = f->usec;
    fh->sub_type = f->subtype;
    fh->width = f->w;
    fh->height = f->h;
    fh->padding = 0;
    fh->flags = f->flags;

    int i;
    for(i=0;i<f->pl_num;i++) {
        int j = (f->pl + i) & PLMASK;
        cc->s_size[i] = dq->pl[j].iov_len;
        fh->size += dq->pl[j].iov_len;
    }

    if(fh->size) {

        if(send(cc->sd, fh, ASFH_SZ, 0) != ASFH_SZ) {
            d_err("[%d] send failed: %s\n", cc->cid, strerror(errno));
            return -1;
        }

        u32 sent = 0;
        for(i=0;i<f->pl_num;i++) {
            int j = (f->pl + i) & PLMASK;
            if(cc->s_size[i] != dq->pl[j].iov_len) {
                d_err("[%d] send size mismatch\n", cc->cid);
                return -1;
            }
            int ret = send(cc->sd, dq->pl[j].iov_base, cc->s_size[i], 0);
            if(ret != cc->s_size[i]) {
                d_err("[%d] send failed: %s\n", cc->cid, strerror(errno));
                return -1;
            }
            sent += ret;
        }
        if(sent < fh->size) {
            d_err("[%d] send size mismatch: %u != %u\n", cc->cid, sent, fh->size);
            return -1;
        }
        //d_msg("[%d] #%u sent %u\n", cc->cid, cc->ari, fh->size);
    }

    cc->ari = (cc->ari + 1) & FNMASK;
    cc->aseq = (cc->aseq) ? cc->aseq+1 : f->seq+1;

    return fh->size;
}

static int send_video(client_t *cc)
{
    data_queue_t *dq = &v_streams[cc->ch][cc->id];
    frame_t *f = 0;
   // d_err("[%d] w/k/r=%u/%u:%u\n", cc->cid, dq->wi, dq->ki, cc->vri);

    if(cc->vri >= FN || (FN_SIZE(dq->wi, cc->vri) > 15)) {
        if(cc->vri < FN)
            d_err("[%d] video gap too large, w/r=%u/%u\n", cc->cid, dq->wi, cc->vri);
        cc->vri = dq->wi;
        cc->gotk = cc->vseq = 0;
    }

    if(cc->vri == dq->wi)
        return 0;

    f = &dq->f[cc->vri];

    if(cc->vseq && cc->vseq != f->seq) {
        d_err("[%d] wrong seq: %u != %u @ %u\n", cc->cid, f->seq, cc->vseq, cc->vri);
        cc->vri = dq->wi;
        cc->gotk = cc->vseq = 0;
        return 0;
    }

    f = &dq->f[cc->vri];
    as_frame_t *fh = &cc->fh;
    fh->type = ASF_TYPE_VIDEO;
    fh->magic = ASF_MAGIC;
    fh->size = 0;
    fh->sec = f->sec;
    fh->usec = f->usec;
    fh->sub_type = f->subtype;
    fh->width = f->w;
    fh->height = f->h;
    fh->padding = 0;
    if(f->is_key) {
        fh->flags = f->flags | 0x80;
        cc->gotk = 1;
    }
    else
        fh->flags = f->flags;

    int i;
    for(i=0;i<f->pl_num;i++) {
        int j = (f->pl + i) & PLMASK;
        cc->s_size[i] = dq->pl[j].iov_len;
        fh->size += dq->pl[j].iov_len;
    }

    if(cc->gotk && fh->size) {
        if(send(cc->sd, fh, ASFH_SZ, 0) != ASFH_SZ) {
            d_err("[%d] send failed: %s\n", cc->cid, strerror(errno));
            return -1;
        }
        u32 sent = 0;
        for(i=0;i<f->pl_num;i++) {
            int j = (f->pl + i) & PLMASK;
            if(cc->s_size[i] != dq->pl[j].iov_len) {
                d_err("[%d] send size mismatch\n", cc->cid);
                return -1;
            }
            int ret = send(cc->sd, dq->pl[j].iov_base, cc->s_size[i], 0);
            if(ret != cc->s_size[i]) {
                d_err("[%d] send failed: %s\n", cc->cid, strerror(errno));
                return -1;
            }
            sent += ret;
        }
        if(sent < fh->size) {
            d_err("[%d] send size mismatch: %u != %u\n", cc->cid, sent, fh->size);
            return -1;
        }
        //d_msg("[%d] #%u sent %u\n", cc->cid, cc->vri, fh->size);
    }

    cc->vri = (cc->vri + 1) & FNMASK;
    cc->vseq = (cc->vseq) ? cc->vseq+1 : f->seq+1;

    return fh->size;
}

static int send_status(client_t *cc)
{
    as_status_t *fh = (as_status_t *)&cc->fh;
    fh->type = ASF_TYPE_STATUS;
    fh->magic = ASF_MAGIC;
    fh->size = 0;
    fh->sec = (u32)time(0);
    fh->usec = 0;
    fh->sub_type = 0;
    fh->padding = 0;
    fh->flags = di_status | (do_status << 2) | (v_loss ? ASF_FLAG_VLOSS : 0) | (motion_status ? ASF_FLAG_MOTION : 0);
    if(in_upgrade == 0) {
        fh->mode = ASS_MODE_NORMAL;
        fh->status = 0;
    }
    else if(in_upgrade == 1) {
        u32 status = 0;
        if(as_gen_fw_status(&status) < 0) {
            fh->mode = ASS_MODE_UPGRADED;
            fh->status = ASS_STATUS_FAILED;
        }
        else {
            fh->mode = ASS_MODE_UPGRADING;
            fh->status = (status < 100) ? status : 100;
        }
    }
    else {
        fh->mode = ASS_MODE_UPGRADED;
        fh->status = upgrade_status;
    }

    if(send(cc->sd, fh, ASSH_SZ, 0) != ASSH_SZ) {
        d_err("[%d] send failed: %s\n", cc->cid, strerror(errno));
        return -1;
    }
    //d_err("[%d] send status @ %u\n", cc->cid, (u32)time(0));

    return ASSH_SZ;
}

static int send_data(client_t *cc)
{
    int sent = 0;

    if(!cc->motion_only || motion_status) {
        if(cc->mode & ASCMD_S2D_STREAM_AUDIO) {
            int ret = 0;
            do {
                ret = send_audio(cc);
                if(ret < 0)
                    return -1;
                sent += ret;
            } while(ret > 0);
        }
        if(cc->mode & ASCMD_S2D_STREAM_META) {
        }
        if(cc->mode & ASCMD_S2D_STREAM_VIDEO) {
            
            int ret = 0;
            do {
                ret = send_video(cc);
                if(ret < 0)
                    return -1;
                sent += ret;
            } while(ret > 0);
        }
    }
    if((cc->motion_only && !motion_status) || (cc->mode & ASCMD_S2D_STREAM_STATUS)) { // status only
        u32 now = (u32)time(0);
        u32 status = di_status | (do_status << 2) | (v_loss ? ASF_FLAG_VLOSS : 0) | (motion_status ? ASF_FLAG_MOTION : 0);
        if(now != cc->last_status || status != cc->status) {
            int ret = send_status(cc);
            if(ret < 0)
                return -1;
            sent += ret;
            cc->status = status;
            cc->last_status = now;
        }
    }

    if(sent)
        cc->last_send = (u32)time(0);

    return 0;
}

static int client_close(client_t *cc)
{
    cc->mode = 0;
    cc->motion_only = 0;
    if(cc->ext_mode && cc->cid == roi_cid)
        roi_cid = -1;
    cc->ext_mode = 0;
    close(cc->sd);
    cc->sd = -1;
    d_err("[%d] closed\n", cc->cid);
    return 0;
}

static void *client_thread(void *arg)
{
    client_t *cc = (client_t *)arg;

    d_msg("[C] #%d thread start .....PID: %u\n", cc->cid, (u32)gettid());

    cc->rbuf = as_alloc(CMD_BSIZE);
    cc->sbuf = as_alloc(CMD_BSIZE);
    if(!cc->rbuf || !cc->sbuf) {
        d_err("OOM: %s", strerror(errno));
        goto _client_exit;
    }

    time_t last = 0;

    while(1) {
        if(cc->sd < 0) {
            last = 0;
            msleep(50);
            continue;
        }
        if(cc->ext_mode) {
            if(cc->cid != roi_cid)
                client_close(cc);
            msleep(50);
            continue;
        }

        fd_set rds, wds, eds;
        struct timeval tv;
        int retval;

        FD_ZERO(&rds);
        FD_SET(cc->sd, &rds);
        FD_SET(cc->sd, &wds);
        FD_SET(cc->sd, &eds);

        tv.tv_sec = 1;
        tv.tv_usec = 0;

        retval = select(cc->sd+1, &rds, &wds, &eds, &tv);

        time_t now = time(0);

        if(retval < 0) {
            d_err("[%d] select error: %s\n", cc->cid, strerror(errno));
            client_close(cc);
            continue;
        }
        else if(retval) {

            if(FD_ISSET(cc->sd, &eds)) {
                d_err("[%d] exception: %s\n", cc->cid, strerror(errno));
                client_close(cc);
                continue;
            }
            if(FD_ISSET(cc->sd, &rds)) {

                int r_total = 0;
                while(r_total < ASCMD_SZ) {
                    int r_size = recv(cc->sd, cc->rbuf+r_total, ASCMD_SZ-r_total, 0);
                    if(r_size < 0) {
                        d_err("[%d] recv error:%s\n", cc->cid, strerror(errno));
                        break;
                    }
                    if(r_size == 0) {
                        if(++cc->zero_count > 10) {
                            d_err("[%d] recv 0 byte\n", cc->cid);
                            break;
                        }
                    }
                    r_total += r_size;
                }
                if(r_total != ASCMD_SZ) {
                    d_err("[%d] recv cmd error: got=%u\n", cc->cid, r_total);
                    client_close(cc);
                    continue;
                }

                else {

                    cc->zero_count = 0;

                    as_cmd_t *cmd = (as_cmd_t *)cc->rbuf;
					if(cmd->magic != ASCMD_MAGIC || (cmd->version != ASCMD_VERSION && cmd->version != ASCMD_PAVO && cmd->version != ASCMD_TLVO && cmd->version != ASCMD_ONVO)) {
						d_err("Got unknown packet %d bytes (%x %x) (%x %x)\n", r_total, cmd->magic, cmd->version,ASCMD_MAGIC ,ASCMD_VERSION);
					}
		    else if(cmd->cmd == ASCMD_S2D_AUDIO_OUT) {
                        //d_msg("Got Audio out\n");
                        if(in_upgrade != 1) {
                            do_audio_out(cc, cmd->flag, cmd->len - sizeof(as_cmd_t));
                        }
                    }
                    else if(cmd->cmd == ASCMD_S2D_STREAM) {
                        cc->mode = ASCMD_S2D_STREAM_MODE(cmd->flag);
                        cc->motion_only = ASCMD_S2D_STREAM_MOTION_ONLY(cmd->flag);
                        cc->ch = ASCMD_S2D_STREAM_CH(cmd->flag);
                        cc->id = ASCMD_S2D_STREAM_ID(cmd->flag);
                        cc->ari = cc->vri = FN;
                        if(cc->mode & ASCMD_S2D_STREAM_VIDEO) {
                            if(cc->ch >= max_ch || cc->id >= max_stream) {
                                d_err("wrong ch & id: %d, %d (flag=%x)\n", cc->ch, cc->id, cmd->flag);
                                cc->mode &= ~ASCMD_S2D_STREAM_VIDEO;
                            }
                        }
                        if(cc->mode & ASCMD_S2D_STREAM_AUDIO) {
                            if(cc->ch >= max_ch) {
                                d_err("wrong audio ch: %d (flag=%x)\n", cc->ch, cmd->flag);
                                cc->mode &= ~ASCMD_S2D_STREAM_AUDIO;
                            }
                        }
                        if(!cc->mode) {
                            d_err("wrong mode:0x%x ,flag=%x\n", cc->mode, cmd->flag);
                            client_close(cc);
                            continue;
                        }
                        d_msg("[%d] Mode = %x (motion_only=%d)\n", cc->cid, cc->mode, cc->motion_only);
                    }
                    else if(cmd->cmd == ASCMD_S2D_ROI_MODE) {
                        cc->ext_mode = 1;
                        roi_cid = cc->cid;
                        d_msg("[%d] ROI Mode\n", cc->cid);
                        continue;
                    }
                    else if(cmd->cmd == ASCMD_S2D_DO) {
                        d_msg("Got DO: %x\n", cmd->flag);
                        set_do(cmd->flag);
                    }/*
                    else if(cmd->cmd == ASCMD_S2D_AUDIO_OUT) {
                        //d_msg("Got Audio out\n");
                        if(in_upgrade != 1) {
                            do_audio_out(cc, cmd->flag, cmd->len - sizeof(as_cmd_t));
                        }
                    }*/
                    else if(cmd->cmd == ASCMD_S2D_FTEST) {
                       
                        d_msg("Got FTEST\n");
                        if(do_ftest(cc, cmd->flag, cmd->len - sizeof(as_cmd_t)) < 0) {
                            client_close(cc);
                            continue;
                        }
                        
                    }
                    else if(cmd->cmd == ASCMD_S2D_UPGRADE) {
                        d_msg("Got UPGRADE\n");
                        if(in_reset)
                            continue;
                        if(in_upgrade != 1) {
                            do_upgrade(cc, cmd->len - sizeof(as_cmd_t));
                            if(in_upgrade == 1)
                                cc->mode = 0;
                        }
                        else
                            d_err("Already in Upgrading...\n");
                    }
                    else if(cmd->cmd == ASCMD_S2D_RESET) {
                        d_msg("Got RESET\n");
                        cc->mode = 0;
                        in_reset = 1;
                        msleep(50);
                        if(in_upgrade != 1) {
                            as_gen_wdt_reboot();
                            while(1);
                        }
                        else {
                            d_err("Upgrading, RESET later\n");
                            to_reset = 1;
                        }
                    }
                    else if(cmd->cmd == ASCMD_S2D_BT_RFCOMM) {
                        int bt_inuse = 0;
                        pthread_mutex_lock(&bt_mutex);
                        if(bt_out_inuse)
                            bt_inuse = 1;
                        else
                            bt_out_inuse = 1;
                        pthread_mutex_unlock(&bt_mutex);
                        if(bt_inuse) {
                            d_msg("BT RFCOMM - BUSY\n");
                            send_cmd(cc, ASCMD_D2S_BT_RFCOMM, (u16)-2);
                            client_close(cc);
                            continue;
                        }
                        else {
                            if(bt_rfcomm(cc, cmd->len-ASCMD_SZ) < 0) {
                                d_msg("BT RFCOMM - FAILED\n");
                                client_close(cc);
                                pthread_mutex_lock(&bt_mutex);
                                bt_out_inuse = 0;
                                pthread_mutex_unlock(&bt_mutex);
                                continue;
                            }
                            d_msg("BT RFCOMM - ACCEPT\n");
                        }
                    }
                    else if(cmd->cmd == ASCMD_S2D_BT_STATUS) {
                        d_msg("BT STATUS=%d\n", bt_out_status);
                        if(send_cmd(cc, ASCMD_D2S_BT_STATUS, (u16)bt_out_status) < 0) {
                            client_close(cc);
                            continue;
                        }
                    }
                    else if(cmd->cmd == ASCMD_S2D_RING_CTRL) {
                        d_msg("Ring : %u @ %u\n", cmd->flag, (u32)now);
                        ao_on = (cmd->flag) ? (u32)now : 0;
                    }
                    else if(cmd->cmd == ASCMD_S2D_ISP) {
                        d_msg("ISP : %u\n", cmd->flag);
                        if(cmd->flag > 255)
                            continue;
                        if(c_recv(cc, cc->rbuf+ASCMD_SZ, (int)(cmd->flag*4)) < 0) {
                            d_err("[%d] recv ISP data failed\n", cc->cid);
                            client_close(cc);
                            continue;
                        }
                        do_isp((u16 *)(cc->rbuf+ASCMD_SZ), cmd->flag);
                    }
                    else if(cmd->cmd == ASCMD_S2D_USERDATA) {
                        d_msg("UserData: %u, %u\n", cmd->flag, cmd->len-ASCMD_SZ);
                        if(cmd->flag) {
                            if(set_userdata(cc, cmd->len-ASCMD_SZ) < 0) {
                                d_err("[%d] Set userdata failed\n", cc->cid);
                                client_close(cc);
                                continue;
                            }
                        }
                        else {
                            const char *ud = vs1_get_userdata(-1); 
                            int udsz = (ud) ? strlen(ud) : 0;
                            d_msg("Get UserData: %d\n", udsz);
                            if(ud)
                                send_cmd_data(cc, ASCMD_D2S_USERDATA, 0, ud, udsz);
                            else
                                send_cmd_data(cc, ASCMD_D2S_USERDATA, 0, 0, 0);
                        }
                    }
                    else if(cmd->cmd == ASCMD_S2D_RS485) {
                        d_msg("RS485 IN\n");
                        rs485_proc(cc->sd);
                        d_msg("RS485 OUT\n");
                        client_close(cc);
                        continue;
                    }
                    else if(ASCMD_IS_MFTEST(cmd->cmd)) {
                        mf_test(cc, cmd->cmd & 0xfff, cmd->flag);
                    }
                }
            }
            if(FD_ISSET(cc->sd, &wds)) {
                if(cc->mode) {
                    int sent = send_data(cc);
                    if(sent < 0) {
                        d_err("[%d] close\n", cc->cid);
                        client_close(cc);
                        continue;
                    }
                    if(sent)
                        last = now;
                    else
                        msleep(10);
                }
                else {
                    if(now < last || (now - last) > 2) {
                        //send_status(cc); // conflict with MFTEST
                        last = now;
                    }
                    msleep(50);
                }
            }
        }
        else { // timeout
        }
    }
_client_exit:
    if(cc->sd >= 0)
        close(cc->sd);
    if(cc->sbuf)
        free(cc->sbuf);
    if(cc->rbuf)
        free(cc->rbuf);

    pthread_exit("CMD thread exit");
}

static void *serv_thread8010(void *arg)
{
    d_msg("[S] thread start .....PID: %u\n", (u32)gettid());
	int re_flag = 0;
    int sd = socket(PF_INET, SOCK_STREAM, 0);
    if(sd < 0) {
        d_err("Serv socket error: %s", strerror(errno));
        goto _serv_exit;
    }
    else {
        int val = 1;
        if(setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0) {
            d_err("set SO_REUSEADDR error: %s", strerror(errno));
            goto _serv_exit;
        }
        struct sockaddr_in  my_addr;
        my_addr.sin_family = PF_INET;
        my_addr.sin_port = htons(DATA_PORT);
        my_addr.sin_addr.s_addr = INADDR_ANY;
        if(bind(sd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) < 0) {
            d_err("bind error: %s", strerror(errno));
            goto _serv_exit;
        }
        if(listen(sd, 32) < 0) {
            d_err("listen error: %s", strerror(errno));
            goto _serv_exit;
        }
    }

    while(1) {
        int new_sd;
        socklen_t addr_size = sizeof(struct sockaddr_in);
        struct sockaddr_in  peer_addr;
		if(re_flag==10){re_flag=0;system("reboot");}
        if((new_sd = accept(sd, (struct sockaddr *)&peer_addr, &addr_size)) < 0) {
            re_flag++;
            d_err("accept error: %s\n", strerror(errno));
            msleep(500);
            continue;
        }

        d_msg("Accept from %s:%u\n", inet_ntoa(peer_addr.sin_addr), peer_addr.sin_port);

        int i;
        for(i=0;i<max_client;i++) {
            if(clients[i].sd < 0)
                break;
        }

        if(i >= max_client) {
            d_err("client # over max:%u\n", max_client);
            close(new_sd);
            continue;
        }

        client_t *cc = &clients[i];
        cc->mode = 0;
        cc->ch = cc->id = -1;
        cc->ari = cc->vri = FN;
        as_copy(&cc->peer_addr, &peer_addr, sizeof(peer_addr));
        d_msg("New client @ %d from %s\n", i, inet_ntoa(peer_addr.sin_addr));
        cc->sd = new_sd;
    }
_serv_exit:
    if(sd >= 0)
        close(sd);

    pthread_exit("[S] thread exit");
}

static void *serv_thread8011(void *arg)
{
	d_msg("[S] thread start .....PID: %u\n", (u32)gettid());

	int sd = socket(PF_INET, SOCK_STREAM, 0);
	if(sd < 0) {
		d_err("Serv socket error: %s", strerror(errno));
		goto _serv_exit;
	}
	else {
		int val = 1;
		if(setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0) {
			d_err("set SO_REUSEADDR error: %s", strerror(errno));
			goto _serv_exit;
		}
		struct sockaddr_in  my_addr;
		my_addr.sin_family = PF_INET;
		my_addr.sin_port = htons(8011);
		my_addr.sin_addr.s_addr = INADDR_ANY;
		if(bind(sd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) < 0) {
			d_err("bind error: %s", strerror(errno));
			goto _serv_exit;
		}
		if(listen(sd, 32) < 0) {
			d_err("listen error: %s", strerror(errno));
			goto _serv_exit;
		}
	}

	while(1) {
		int new_sd;
		socklen_t addr_size = sizeof(struct sockaddr_in);
		struct sockaddr_in  peer_addr;
		if((new_sd = accept(sd, (struct sockaddr *)&peer_addr, &addr_size)) < 0) {
			d_err("accept error: %s\n", strerror(errno));
			msleep(500);
			continue;
		}

		d_msg("Accept from %s:%u\n", inet_ntoa(peer_addr.sin_addr), peer_addr.sin_port);

		int i;
		for(i=0;i<max_client;i++) {
			if(clients[i].sd < 0)
				break;
		}

		if(i >= max_client) {
			d_err("client # over max:%u\n", max_client);
			close(new_sd);
			continue;
		}

		client_t *cc = &clients[i];
		cc->mode = 0;
		cc->ch = cc->id = -1;
		cc->ari = cc->vri = FN;
		as_copy(&cc->peer_addr, &peer_addr, sizeof(peer_addr));
		d_msg("New client @ %d from %s\n", i, inet_ntoa(peer_addr.sin_addr));
		cc->sd = new_sd;
	}
_serv_exit:
	if(sd >= 0)
		close(sd);

	pthread_exit("[S] thread exit");
}

// -----------------------------------------------------------------------------
// Bluetooth
// -----------------------------------------------------------------------------
static int bt_recv(int sd, int size, u8 *buf)
{
    int r_total = 0;
    int rerr = 0;

    while(r_total < size) {
        int r_size = read(sd, buf+r_total, size-r_total);
        if(r_size < 0) {
            d_err("[B] recv error:%s\n", strerror(errno));
            break;
        }
        else if(r_size == 0) {
            if(++rerr > 10)
                break;
            msleep(10);
        }
        else
            rerr = 0;
        r_total += r_size;
    }
    if(r_total != size) {
        d_err("[B] recv error: exp=%u, got=%u\n", size, r_total);
        return -1;
    }
    return 0;
}

static int bt_send(int sd, u8 *buf, u16 cid, u16 flag)
{
    as_cmd_t *cmd = (as_cmd_t *)buf;
    cmd->magic = ASCMD_MAGIC;
    cmd->version = ASCMD_VERSION;
    cmd->cmd = cid;
    cmd->flag = flag;
    cmd->len = ASCMD_SZ;
    if(write(sd, buf, ASCMD_SZ) != ASCMD_SZ) {
        d_err("[B] send failed: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

static int bt_parse_rfcomm(int sd, u8 *buf, int size)
{
    asc_bt_rfcomm_t *br = (asc_bt_rfcomm_t *)buf;

    if(size > (CMD_BSIZE-ASCMD_SZ) || bt_recv(sd, size, buf+ASCMD_SZ) < 0) {
        bt_send(sd, buf, ASCMD_D2S_BT_RFCOMM, (u16)-1);
        return -1;
    }
    if(br->size >= BT_OUT_SZ) {
        bt_send(sd, buf, ASCMD_D2S_BT_RFCOMM, (u16)-1);
        return -1;
    }
    memcpy(bt_out_mac, br->mac, 6);
    bt_out_ch = br->channel;
    strncpy(bt_out_pin, br->pin, 63);
    bt_out_pin[63] = 0;
    bt_out_size = br->size;
    memcpy(bt_out_data, br->data, br->size);
    bt_out_status = 1;
    bt_out_cmd = 1;
    bt_send(sd, buf, ASCMD_D2S_BT_RFCOMM, (u16)0);
    return 0;
}
/*
static void *bluez_thread(void *arg)
{
    d_msg("[B] thread start .....PID: %u\n", (u32)gettid());

    struct sockaddr_rc loc_addr = { 0 }, rem_addr = { 0 };
    char buf[1024] = { 0 };
    int s = -1;
    int c = -1;

    sleep(2);

_bluez_init:

    if(s >= 0) {
        close(s);
        s = -1;
    }

    s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
    if(s < 0) {
        d_err("[B] Open bluetooth socket failed\n");
        while(1)
            sleep(1);
    }
    d_msg("[B] server socket = %d\n", s);

    // bind socket to port 1 of the first available local bluetooth adapter
    loc_addr.rc_family = AF_BLUETOOTH;
    loc_addr.rc_bdaddr = *BDADDR_ANY;
    loc_addr.rc_channel = (uint8_t) 1;
    if(bind(s, (struct sockaddr *)&loc_addr, sizeof(loc_addr)) < 0) {
        d_err("[B] Bind bluetooth socket failed\n");
        sleep(1);
        goto _bluez_init;
    }

    if(listen(s, 1) < 0) {
        d_err("[B] Listen bluetooth socket failed\n");
        sleep(1);
        goto _bluez_init;
    }

_bt_serv_start_:
    if(c >= 0) {
        close(c);
        c = -1;
    }

    d_msg("[B] waiting for new connection\n");

    socklen_t opt = sizeof(rem_addr);
    c = accept(s, (struct sockaddr *)&rem_addr, &opt);
    if(c < 0) {
        d_err("[B] Accept bluetooth socket failed\n");
        sleep(1);
        goto _bluez_init;
    }

    ba2str( &rem_addr.rc_bdaddr, buf );
    d_msg("[B] accepted connection from %s (%d)\n", buf, c);
    memset(buf, 0, sizeof(buf));

    int zero_count = 0;
    time_t last = 0;
    u8 flags = 0;

    while(1) {

        fd_set rds, wds, eds;
        struct timeval tv;
        int retval;

        FD_ZERO(&rds);
        FD_SET(c, &rds);
        FD_SET(c, &wds);
        FD_SET(c, &eds);

        tv.tv_sec = 0;
        tv.tv_usec = 50000;

        retval = select(c+1, &rds, &wds, &eds, &tv);

        time_t now = time(0);

        if(retval < 0) {
            d_err("[B] select error: %s\n", strerror(errno));
            goto _bt_serv_start_;
        }
        else if(retval) {

            if(FD_ISSET(c, &eds)) {
                d_err("[B] exception: %s\n", strerror(errno));
                goto _bt_serv_start_;
            }
            if(FD_ISSET(c, &rds)) {

                int r_total = 0;
                while(r_total < ASCMD_SZ) {
                    int r_size = read(c, buf+r_total, ASCMD_SZ-r_total);
                    if(r_size < 0) {
                        d_err("[B] read error:%s\n", strerror(errno));
                        break;
                    }
                    if(r_size == 0) {
                        if(++zero_count > 10) {
                            d_err("[B] read 0 byte\n");
                            break;
                        }
                    }
                    r_total += r_size;
                }

                if(r_total != ASCMD_SZ) {
                    d_err("[B] read cmd error: got=%u\n", r_total);
                    goto _bt_serv_start_;
                }
                else {

                    zero_count = 0;

                    as_cmd_t *cmd = (as_cmd_t *)buf;
                    if(cmd->magic != ASCMD_MAGIC || cmd->version != ASCMD_VERSION) {
                        d_err("[B]Got unknown packet %d bytes (%x %x)\n", r_total, cmd->magic, cmd->version);
                    }
                    else if(cmd->cmd == ASCMD_S2D_DO) {
                        d_msg("[B]Got DO: %x\n", cmd->flag);
                        set_do(cmd->flag);
                        last = 0;
                    }
                    else if(cmd->cmd == ASCMD_S2D_BT_RFCOMM) {
                        int bt_inuse = 0;
                        pthread_mutex_lock(&bt_mutex);
                        if(bt_out_inuse)
                            bt_inuse = 1;
                        else
                            bt_out_inuse = 1;
                        pthread_mutex_unlock(&bt_mutex);
                        if(bt_inuse) {
                            d_msg("[B]BT RFCOMM - BUSY\n");
                            bt_send(c, buf, ASCMD_D2S_BT_RFCOMM, (u16)-2);
                            goto _bt_serv_start_;
                        }
                        else {
                            if(bt_parse_rfcomm(c, buf, cmd->len-ASCMD_SZ) < 0) {
                                d_msg("[B]BT RFCOMM - FAILED\n");
                                pthread_mutex_lock(&bt_mutex);
                                bt_out_inuse = 0;
                                pthread_mutex_unlock(&bt_mutex);
                                goto _bt_serv_start_;
                            }
                            d_msg("[B]BT RFCOMM - ACCEPT\n");
                        }
                    }
                    else if(cmd->cmd == ASCMD_S2D_BT_STATUS) {
                        d_msg("[B]BT STATUS=%d\n", bt_out_status);
                        if(bt_send(c, buf, ASCMD_D2S_BT_STATUS, (u16)bt_out_status) < 0)
                            goto _bt_serv_start_;
                    }
                    else if(cmd->cmd == ASCMD_S2D_RING_CTRL) {
                        d_msg("Ring : %u @ %u (BT)\n", cmd->flag, (u32)now);
                        ao_on = (cmd->flag) ? (u32)now : 0;
                    }
                    else if(cmd->cmd == ASCMD_S2D_RS485) {
                        d_msg("RS485 IN (BT)\n");
                        rs485_proc(c);
                        d_msg("RS485 OUT (BT)\n");
                        goto _bt_serv_start_;
                    }
                    else {
                        d_msg("[B]Command 0x%x is not supported in Bluetooth\n", cmd->cmd);
                    }
                }
            }
            if(FD_ISSET(c, &wds)) {
                u8 cflags = di_status | (do_status << 2) | (v_loss ? ASF_FLAG_VLOSS : 0) | (motion_status ? ASF_FLAG_MOTION : 0);
                if(cflags != flags || (now - last) > 1) {
                    as_status_t *fh = (as_status_t *)buf;
                    fh->type = ASF_TYPE_STATUS;
                    fh->magic = ASF_MAGIC;
                    fh->size = 0;
                    fh->sec = (u32)time(0);
                    fh->usec = 0;
                    fh->sub_type = 0;
                    fh->padding = 0;
                    fh->flags = cflags;
                    if(in_upgrade == 0) {
                        fh->mode = ASS_MODE_NORMAL;
                        fh->status = 0;
                    }
                    if(write(c, buf, ASSH_SZ) != ASSH_SZ) {
                        d_err("[B] send failed: %s\n", strerror(errno));
                    }
                    last = now;
                    flags = cflags;
                }
                else
                    msleep(20);
            }
        }
        else { // timeout
            msleep(50);
        }
    }

    close(c);
    close(s);

    pthread_exit("[B] thread exit");
}

static void *bt_out_thread(void *arg)
{
    d_msg("[BO] thread start .....PID: %u\n", (u32)gettid());

    struct sockaddr_rc addr = { 0 };
    int sd = -1;
    bt_out_status = 0; // idle

    while(1) {
        bt_out_cmd = 0;
        pthread_mutex_lock(&bt_mutex);
        bt_out_inuse = 0;
        pthread_mutex_unlock(&bt_mutex);
        if(sd >= 0) {
            close(sd);
            sd = -1;
        }

        d_msg("[BO] Wait for BT RFCOMM command\n");

        while(!bt_out_cmd)
            msleep(100);

        bt_out_status = 1; // connecting

        d_msg("[BO] Got BT RFCOMM command\n");

        if(bt_out_pin[0]) {
            FILE *fp = fopen("/etc/bluetooth/pin.txt", "w");
            if(fp) {
                int wlen = strlen(bt_out_pin);
                fwrite(bt_out_pin, 1, wlen, fp);
                fflush(fp);
                fclose(fp);
            }
        }

        // allocate a socket
        sd = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
        if(sd < 0) {
            bt_out_status = -1; // socket failed
            continue;
        }

        // set the connection parameters (who to connect to)
        addr.rc_family = AF_BLUETOOTH;
        addr.rc_channel = bt_out_ch;
        baswap(&addr.rc_bdaddr, (bdaddr_t *)bt_out_mac);

        char mac[32];
        ba2str(&addr.rc_bdaddr, mac);
        d_msg("[BO] Try to connect: %s ch%d\n", mac, addr.rc_channel);

        // put socket in non-blocking mode
        //sock_flags = fcntl( sd, F_GETFL, 0 );
        //fcntl( sd, F_SETFL, sock_flags | O_NONBLOCK );

        // initiate connection attempt
        int status = connect(sd, (struct sockaddr *)&addr, sizeof(addr));
        if( status < 0 ) {
            d_err("[BO] connect failed: %s\n", strerror(errno));
            bt_out_status = -2; // connect failed
            continue;
        }

        bt_out_status = 2; // sending
        d_msg("[BO] Connected\n");

        fd_set wfd;
        struct timeval tv;
        FD_ZERO(&wfd);
        FD_SET(sd, &wfd);
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        status = select(sd + 1, NULL, &wfd, NULL, &tv);
        if( status > 0 && FD_ISSET( sd, &wfd ) )
            status = send(sd, bt_out_data, bt_out_size, 0);
        if( status < 0 ) {
            d_err("[BO] Sent failed: %s\n", strerror(errno));
            bt_out_status = -3; // send failed
        }
        else {
            d_err("[BO] Command sent = %d\n", status);
            bt_out_status = 3; // done
        }
    }

    close(sd);

    pthread_exit("[BO] thread exit");
}
*/
// -----------------------------------------------------------------------------
static void *upgrade_thread(void *arg)
{
    d_msg("[UG] thread start .....PID: %u\n", (u32)gettid());

    upgrade_buf = as_malloc(UPGRADE_SIZE);
    assert(upgrade_buf);

    in_upgrade = 0;
    upgrade_status = 0;
    upgrade_size = 0;

    while(1) {

        if(in_upgrade == 1) {
            if(upgrade_size) {
                d_msg("Upgrade start... %u bytes\n", upgrade_size);
#if 1
                    FILE *fp = fopen("upgrade.bin", "w");
                    if(fp) {
                        fwrite(upgrade_buf, 1, upgrade_size, fp);
                        fflush(fp);
                        fclose(fp);
                        sleep(1);
                        d_err("dump fw file finished\n");
                    }
#endif
/*
                int ret = as_gen_fw_upgrade(upgrade_buf, upgrade_size);
                if(ret < 0) {
                    d_err("Upgrade failed = %d\n", ret);
#if 1
                    FILE *fp = fopen("test.bin", "w");
                    if(fp) {
                        fwrite(upgrade_buf, 1, upgrade_size, fp);
                        fflush(fp);
                        fclose(fp);
                        sleep(1);
                        d_err("dump fw file finished\n");
                    }
#endif
                    if(ret < -1)
                        upgrade_status = ASS_STATUS_WRONG_FILE;
                    else
                        upgrade_status = ASS_STATUS_FAILED;
 
               }
                else {
*/
					//system("rm /root/vimg");
                    system("sync");
                    system("sync");
                    d_msg("Upgrade success\n");
                    upgrade_status = ASS_STATUS_OK;
                    to_reset = 1;
   //             }

                if(to_reset) {
                    in_reset = 1;
                    system("reboot");
                    //as_gen_wdt_reboot();
                    while(1);
                    to_reset = 0;
                }

                upgrade_size = 0;
                in_upgrade = 2;
            }
            else
                msleep(50);
        }

        msleep(500);
    }

    pthread_exit("[UG] thread exit");
}

// -----------------------------------------------------------------------------
static void *poll_thread(void *arg)
{
    d_msg("[IO] thread start .....PID: %u\n", (u32)gettid());

    sleep(1);

    time_t last = 0;
    time_t last_vloss = 0;

    as_gen_set_status_led(1);
    status_led = 1;

#define DN      2
#define DBMASK  7
    u32 d[DN];

    while(1) {
        u32 c = di_status;
        u32 s = 0;
        time_t now = time(0);

        as_gen_read_alarm_in(&s);
        if(s != c) {
            int i;
            for(i=0;i<DN;i++) {
                d[i] <<= 1;
                if(s & (1<<i))
                    d[i] |= 1;
                u32 st = d[i] & DBMASK;
                if(st == DBMASK)
                    c |= (1<<i);
                if(st == 0)
                    c &= ~(1<<i);
            }
            if(c != di_status) {
                d_msg("DI change: %x -> %x\n", di_status, c);
                di_status = c;
            }
        }

        update_obs_flags();

        if(in_upgrade == 1) {
            if(now != last) {
                status_led ^= 1;
                as_gen_set_status_led(status_led);
                last = now;
            }
        }
        // MFTEST need to turn on/off
        //else if(!status_led) {
        //    as_gen_set_status_led(1);
        //    status_led = 1;
        //}

        if(now != last_vloss) {
            u32 vloss = 0;
            as_media_read_vloss(&vloss);
            if(vloss != v_loss) {
                d_msg("Video Loss=%x -> %x\n", vloss, v_loss);
                v_loss = vloss;
            }
            last_vloss = now;
        }

        msleep(50);
    }

    pthread_exit("[IO] thread exit");
}

static void *ao_thread(void *arg)
{
    d_msg("[AO] thread start .....PID: %u\n", (u32)gettid());

    u8 *buf = 0;
    FILE *fp = fopen("output.adpcm", "rb");
    if(!fp) {
        d_err("Cannot open audio file\n");
        goto _ao_exit_;
    }

    u32 fsize = 0;
    fseek(fp, 0, SEEK_END);
    fsize = (u32)ftell(fp);
    fseek(fp, 0, SEEK_SET);
    buf = (u8 *)malloc(fsize);
    if(!buf) {
        d_err("OOM\n");
        goto _ao_exit_;
    }
    if(fread(buf, 1, fsize, fp) != fsize) {
        d_err("Read error: %s\n", strerror(errno));
        goto _ao_exit_;
    }
    fclose(fp);
    fp = 0;

    d_msg("audio data size = %u\n", fsize);

    u32 off = 0;
    dm_data_t data;
    data.type = DM_DT_AUDIO;
    data.sub_type = PT_DVI4;
    data.pl_num = 1;
    data.samplerate = 16000;
    data.bitwidth = 16;

    while(1) {
        if(!ao_on) {
            off = 0;
            msleep(100);
            continue;
        }


        if(((u32)time(0) - ao_on) > 60) {
            d_msg("AO stopped after 60 seconds\n");
            ao_on = 0;
            continue;
        }

        if((off+168) > fsize) {
            off = 0;
            msleep(1000);
        }

        data.payloads[0].iov_base = buf+off+4;
        data.payloads[0].iov_len = 164;
        media_adec_out(&data);

        off += 168;

        msleep(10); // need more accurate ?
    }

_ao_exit_:
    if(fp)
        fclose(fp);
    if(buf)
        free(buf);
    pthread_exit("[AO] thread exit");
    return 0;
}

static void sigchild(int signo)
{
    pid_t pid;
    while ( ( pid = waitpid( -1, &signo, WNOHANG ) ) > 0 ) {
        d_err( "pid = %d\n", pid );
    }
}

// ----------------------------------------------------------------------------
// main
// ----------------------------------------------------------------------------
int as_data_in(dm_data_t *data)
{
    assert(data->channel < max_ch);
    assert(data->id < max_stream);

    data_queue_t *dq;
    frame_t *f;

    if(data->type == DM_DT_VIDEO) {
        dq = &v_streams[data->channel][data->id];
        f = &dq->f[dq->wi];
        if(data->is_key) {
            dq->gotk = 1;
            dq->ki = dq->wi;
            f->is_key = 1;
            dq->w = data->width;
            dq->h = data->height;
        }
        else {
            if(!dq->gotk)
                return 0;
            f->is_key = 0;
        }
        f->w = dq->w;
        f->h = dq->h;
        if(data->sub_type == VID_H264)
            f->subtype = ASF_VTYPE_H264;
        else if(data->sub_type == VID_MJPEG)
            f->subtype = ASF_VTYPE_MJPEG;
        else {
            d_err("Unsupport video type: %x\n", data->sub_type);
            return 0;
        }
    }
    else if(data->type == DM_DT_AUDIO) {
        dq = &a_streams[data->channel];
        f = &dq->f[dq->wi];
        f->w = dq->w = data->bitwidth;
        f->h = dq->h = data->samplerate;
        switch(data->sub_type) {
            case PT_PCMA:
                f->subtype = ASF_ATYPE_G711A;
                break;
            case PT_PCMU:
                f->subtype = ASF_ATYPE_G711U;
                break;
            case PT_DVI4:
                f->subtype = ASF_ATYPE_ADPCM_DVI4;
                break;
            default:
                d_err("Unsupport audio type: %x\n", data->sub_type);
                return 0;
        }
    }
    else
        return 0;

    f->pl = dq->pli;
    f->pl_num = data->pl_num;
    f->seq = dq->seq++;
    f->sec = (u32)data->enc_time.tv_sec;
    f->usec = (u32)data->enc_time.tv_usec;
    f->flags = di_status | (do_status << 2);
    if(v_loss & (1 << data->channel))
        f->flags |= ASF_FLAG_VLOSS;
    if(motion_status)
        f->flags |= ASF_FLAG_MOTION;

    //u32 size = 0;
    u32 i = dq->pli;
    u32 j = 0;
    while(j < data->pl_num) {
        dq->pl[i].iov_base = data->payloads[j].iov_base;
        dq->pl[i].iov_len = data->payloads[j].iov_len;
        //size += data->payloads[j].iov_len;
        j++;
        i = (i + 1) & PLMASK;
    }

    dq->pli = i;
    dq->wi = (dq->wi + 1) & FNMASK;

    //d_msg("[%u-%d] w/k=%u/%u,seq=%u,#=%u,sz=%u\n", data->channel, data->id, dq->wi, dq->ki, dq->seq,data->pl_num,size);

    if(data->type == DM_DT_VIDEO)
        obs_video_in(data);
    else
        obs_audio_in(data);

    return 0;
}

int vs1_set_do(u32 status,int flag)
{
	printf("\n\n[TEST] LED_Lights = %d\n\n",status);
	if (flag == 0)
	{
    	set_do(status);
	}
    return 0;
}

u32 vs1_get_di(void)
{
    return di_status;
}

int vs1_get_motion_client(void)
{
    int i;
    for(i=0;i<max_client;i++) {
        if(clients[i].sd < 0 || !clients[i].mode)
            continue;
        if(clients[i].motion_only)
            return 1;
    }
    return 0;
}

int vs1_set_motion(int status)
{
    motion_status = (status) ? 1 : 0;
    return 0;
}

int vs1_audio_out(unsigned char *buf, int size)
{
    // TODO: type check !?
    dm_data_t data;
    data.type = DM_DT_AUDIO;
    data.sub_type = PT_DVI4;
    data.pl_num = 1;
    data.payloads[0].iov_base = buf;
    data.payloads[0].iov_len = size;
    data.samplerate = 16000;
    data.bitwidth = 16;

    if(ao_on)
        ao_on = 0;

    return media_adec_out(&data);
}

int vs1_set_ring(u32 on)
{
    ao_on = (on) ? (u32)time(0) : 0;
    return 0;
}

int as_data_init(int ch_nr, int stream_nr, int client_nr)
{
    assert(ch_nr <= CH_NR);
    assert(stream_nr <= STREAM_NR);
    assert(client_nr <= CLIENT_NR);

    signal( SIGPIPE, SIG_IGN );
    signal( SIGCHLD, sigchild );

    max_ch = ch_nr;
    max_stream = stream_nr;
    max_client = client_nr;

    as_gen_read_alarm_in(&di_status);
 //   as_gen_set_do(0x00); //DO1/DO2/DO3/DO4

    as_gen_set_red_led(0); // DO1
    as_gen_set_green_led(0); // DO2

    do_status = 0;
    d_msg("Init DI=%x\n", di_status);
    as_media_read_vloss(&v_loss);
    d_msg("Video Loss=%x\n", v_loss);

    as_zero(v_streams, sizeof(v_streams));
    as_zero(a_streams, sizeof(a_streams));
    as_zero(clients, sizeof(clients));

    audio_buf = as_malloc(AUDIO_BSIZE);

    int i;
    printf("max_client : %d\n",max_client);
    for(i=0;i<max_client;i++) {
        clients[i].cid = i;
        clients[i].sd = -1;
        create_norm_thread(client_thread, &clients[i]);
    }
    create_norm_thread(serv_thread8010, 0);
	create_norm_thread(serv_thread8011, 0);
    create_norm_thread(poll_thread, 0);
    create_norm_thread(upgrade_thread, 0);
  //  create_norm_thread(bluez_thread, 0);
  //  create_norm_thread(bt_out_thread, 0);
  //  create_norm_thread(ao_thread, 0);

    return 0;
}

int as_data_get_ftest(asc_ftest_t *ft)
{
    as_zero(ft, ASC_FT_SZ);
    ft->cmd.magic = ASCMD_MAGIC;
    ft->cmd.version = ASCMD_VERSION;
    ft->cmd.cmd = ASCMD_D2S_FTEST;
    ft->cmd.flag = 0;
    ft->cmd.len = ASC_FT_SZ;

    as_gen_get_mac(ft->mac);
    as_gen_get_serial_num(ft->serial_number, 32);
    ft->link = 0;
    if(v_loss)
        ft->link |= 0x04;
    ft->dio = di_status | (do_status << 2);
    if(status_led)
        ft->dio |= 0x80;
    u32 id = 0;
    as_gen_read_dip_switch(&id);
    ft->id = id & 0xff;
    if(in_upgrade == 0) {
        ft->mode = ASS_MODE_NORMAL;
        ft->status = 0;
    }
    else if(in_upgrade == 1) {
        u32 status = 0;
        if(as_gen_fw_status(&status) < 0) {
            ft->mode = ASS_MODE_UPGRADED;
            ft->status = ASS_STATUS_FAILED;
        }
        else {
            ft->mode = ASS_MODE_UPGRADING;
            ft->status = (status < 100) ? status : 100;
        }
    }
    else {
        ft->mode = ASS_MODE_UPGRADED;
        ft->status = upgrade_status;
    }
    return 0;
}

int vs1_roi_out(roi_info_t *roi)
{
	printf("[ROI_OUT] CH0%d IN.\n",roi->id);
	int	t_count = 0;
    if(roi_cid < 0 || roi_cid >= max_client)
        return -1;

    int sd = clients[roi_cid].sd;
    if(sd < 0)
        return -1;

#ifndef ASYF_MAGIC
    as_frame_t fh;
    fh.type = ASF_TYPE_VIDEO;
    fh.magic = ASF_MAGIC;
    fh.sec = roi->sec;
    fh.usec = roi->usec;
    fh.sub_type = ASF_VTYPE_GRAY;
    fh.padding = roi->id;
    if(!roi || !roi->status) {
        fh.width = 0;
        fh.height = 0;
        fh.size = 0;
        fh.flags = 0;
    }
    else {
        fh.width = roi->right - roi->left + 1;
        fh.height = roi->bottom - roi->top + 1;
        fh.flags = (roi->status) ? ASF_FLAG_MOTION : 0;
        fh.size = fh.width * fh.height * CD_GROUP_NR;
    }
    if(send(sd, &fh, ASFH_SZ, 0) != ASFH_SZ) {
        d_err("[ROI] send failed: %s\n", strerror(errno));
        roi_cid = -1;
        return -1;
    }
#else
    as_yframe_t fh;
    fh.magic = ASYF_MAGIC;
    fh.sec = roi->sec;
    fh.usec = roi->usec;
    fh.id = roi->id;

    if(!roi || !roi->status) {
        fh.width = fh.height = 0;
        fh.x = fh.y = 0;
        fh.size = 0;
        fh.flags = 0;
    }
    else {
        fh.width = roi->right - roi->left + 1;
        fh.height = roi->bottom - roi->top + 1;
        fh.flags = (roi->status) ? ASF_FLAG_MOTION : 0;
        fh.size = fh.width * fh.height * CD_GROUP_NR;
        fh.x = roi->left;
        fh.y = roi->top;
    }
	//printf("\n\n\n\n[D] fh.sec = %u,fh.usec = %u,fh.flags = %u,fh.size = %u\n\n\n\n",fh.sec,fh.usec,fh.flags,fh.size);
	if(send(sd, &fh, ASYFH_SZ, 0) != ASYFH_SZ) {
        d_err("[ROI] send failed: %s\n", strerror(errno));
        roi_cid = -1;
        return -1;
    }
#endif

    if(!fh.size)
        return 0;

	//FILE *fp;

	//printf("roi->bottom =%d,roi->height=%d,roi->id=%d,roi->left=%d,roi->right=%d,roi->top=%d,roi->width=%d\n",roi->bottom,roi->height,roi->id,roi->left,roi->right,roi->top,roi->width);
	//printf("fh->width=%d,fh->height=%d,fh->size=%d,fh->x=%d,fh->y=%d\n",fh.width,fh.height,fh.size,fh.x,fh.y);
	printf("[ROI_OUT] CH0%d ASYF_MAGIC 6.\n",roi->id);
    int i;
    for(i=0;i<CD_GROUP_NR;i++) {

        int line = roi->top;
        char *data = (char *)roi->data[i];	
		//fp=fopen("/dev/VS2_test.obi","wb");
		//if (fp)
		//{
		//	fwrite(roi->data[i],1,1920*1080,fp);
		//	fclose( fp );
		//}
        while(line <= roi->bottom) {

            fd_set wds;
            struct timeval tv;
			struct timeval to;
            int retval;

            FD_ZERO(&wds);
            FD_SET(sd, &wds);

			to.tv_sec = 10;
			to.tv_usec = 0;

            tv.tv_sec = 0;
            tv.tv_usec = 100000;
			//printf("[ROI_OUT] CH0%d ASYF_MAGIC 8.\n",roi->id);


            retval = select(sd+1, 0, &wds, 0, &tv);

            if(retval < 0) {
                d_err("[ROI] select error: %s\n", strerror(errno));
                roi_cid = -1;
                return -1;
            }
            else if(retval) {
				//printf("[ROI_OUT] CH0%d ASYF_MAGIC 9.\n",roi->id);
                if(FD_ISSET(sd, &wds)) {
					
                    char *p = ((char *)data) + line*roi->width + roi->left;					
                    int size = fh.width;
                    int total = 0;
                    while(total < size) {
                        int ret = send(sd, p+total, size-total, 0);
                        if(ret < 0) {
                            d_err("[ROI] send failed @ #%d, line#%d, %u/%u: %s\n", i, line, total, size, strerror(errno));
                            roi_cid = -1;
                            return -1;
                        }
                        total+=ret;
                    }
					//printf("[ROI_OUT] CH0%d ASYF_MAGIC 10.\n",roi->id);
                    line++;

                }
            }
			else
			{
				t_count++;
				if (t_count==5)
				{
					return -1;
				}
				
			}
        }
    }

    return 0;

}

