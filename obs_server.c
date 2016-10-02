/*!
 *  @file       obs_server.c
 *  @version    1.0
 *  @date       06/30/2013
 *  @author     Jacky Hsu <sch0914@gmail.com>
 *  @note       Orbit Stream Server
 */

#include "platform.h"
#include "as_protocol.h"
#include "obs_stun.h"
#include "obs_protocol.h"
#include "as_dm.h"

#ifndef STANDALONE
#include "vs1_api.h"
#endif

// ----------------------------------------------------------------------------
// Debug
// ----------------------------------------------------------------------------
#define obs_err(format,...)  printf("[OBS]%s(%d): " format, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define obs_dbg(format,...)  printf("[OBS] " format, ##__VA_ARGS__)
#define PKT_DEBUG   0
#if PKT_DEBUG > 0
#define obs_pkt(format,...)  printf("[PKT] " format, ##__VA_ARGS__)
#else
#define obs_pkt(format,...) do; while(0)
#endif

#define obs_tc(format,...)  printf("[T]%s(%d): " format, __FUNCTION__, __LINE__, ##__VA_ARGS__)

// ----------------------------------------------------------------------------
// Constant, Type & Macro Definition
// ----------------------------------------------------------------------------
#define C_MAX       4
#define C_MASK      (0xf)
#define F_MAX		96
#define IOV_NR		4
#define RBUF_SZ		1500
#define PKT_DSZ		1400
#define DBUF_SZ     0x400000
#define STAT_NR     4
#define AU_QNR      2
#define RS_NR       8

typedef struct {

    u32     seq;
    u32     sec;
    u32     usec;
    u8      type;
    u8      sub_type;
    u8      iskey;
    u8      flag;
    u32     size;
    u32     fsize;
    u8      *ptr;
    u32     noff;
    u32     roff;
    u16     w;
    u16     h;
    u16     pseq_start;
    u16     pseq_end;

} q_frame_t;

typedef struct {

    int			id;
    int			sd;
    int			inuse;
    int			to_close;
    int			dead;

    int         turn_mode;
    int         turn_setup;
    int         state;
    u32         state_start;
    u32         state_cmd_start;

    u32         peer_id;
    int         c_idx;
    u32         c_ip;
    u16         c_port;
    u32         peer_ip[OBST_CONN_NR];
    u16         peer_port[OBST_CONN_NR];

    u32         turn_session;
    u32     	session;
    u8     		ch;
    u8     		stream_id;
    int			audio;

    // stream attr
    u8          vtype;
    u8          atype;
    u16         width;
    u16         height;
    u16         bitwidth;
    u16         samplerate;

    // video frame queue
    u32         a_idx;
    u32         r_idx;
    u32         w_idx;
    u32         w_seq;
    u32         r_seq;
    int         vr_iskey;   // video read frame is key
    int         v_skip;     // video queue in skip mode
    q_frame_t   qf[F_MAX];

    // video frame buffer
    u32     	a_off;
    u32     	r_off;
    u32     	w_off;
    u8          *d_buf;

    // audio frame buffer
    u32         au_ridx;
    u32         au_widx;
    u32         au_woff;
    u32         au_wnr;
    u32         au_usize;
    u32         au_size;
    u32         au_nr;
    int         au_inuse[AU_QNR];
    u8          au_buf[AU_QNR][RBUF_SZ];

    // sender
    u16     	pkt_seq;
    u16     	apkt_seq;
    u64         f_time;
    struct msghdr msg;
    struct iovec iov[2];
    u32         avg_Bps;
    u32         to_send;
    u64         last_send;

    // resend
    u32         rs_widx;
    u32         rs_ridx;
    u16         rs_q[RS_NR];

    // stat
    u32         fin_code;
    u64         send_start;
    int         send_stat_idx;
    u32         send_b_avg[STAT_NR];
    u32         send_p_avg[STAT_NR];
    u32         send_byte_avg;
    u32         send_pkt_avg;
    int         recv_stat_idx;
    u32         recv_b_avg[STAT_NR];
    u32         recv_p_avg[STAT_NR];
    u32         recv_byte_avg;
    u32         recv_pkt_avg;

    u32         pkt_sent;
    u32         pkt_dropped;
    u32         frame_sent;
    u32         frame_dropped;
    u32         byte_sent;
    u64         b_total;
    u32         p_total;
    u32         f_total;

    u32     	last_ack;
    obsp_stat_receiver_t  recv_report;

    // socket
    struct sockaddr_in  peer;
    int         r_size;
    u8     		rbuf[RBUF_SZ];
    u8     		sbuf[RBUF_SZ];

    // packet from server socket
    int         ext_recv;
    u32         ext_ip;
    u16         ext_port;
    u8     		ext_rbuf[RBUF_SZ];

} obs_client_t;

#define OBSC_STATE_INIT         0
#define OBSC_STATE_SYNC         1
#define OBSC_STATE_SYNC_ACK     2
#define OBSC_STATE_STREAM_REQ   3
#define OBSC_STATE_CONNECTED    4

typedef struct {

    int			sd;
    int         state;
    time_t      st_start;
    time_t      op_start;
    u32         c_wip[2];
    u16         c_wport[2];
    u32         session_base;
    int         using_turn;

    int         stun_status;
    u32         stun_req_ts[ASC_STUN_NR];
    u32         stun_resp_ts[ASC_STUN_NR];
    u32         pub_ip[ASC_STUN_NR];
    u16         pub_port[ASC_STUN_NR];

    u32         turn_region_id;
    u32         turn_ip;
    u16         turn_port;
    u32         turn_session;
    u32         turn_wip;
    u16         turn_wport;
    u32         turn_update_ts;
    u32         turn_traffic_ts;

    u32         reg_serv[2];
    u32         reg_req;
    u32         reg_resp;

    struct sockaddr_in  peer;
    int         r_size;
    u8     		rbuf[RBUF_SZ];
    u8     		sbuf[RBUF_SZ];

} obs_server_t ;

#define OBSS_STATE_INIT         0
#define OBSS_STATE_STUN_REQ     1
#define OBSS_STATE_TURN_INIT    2
#define OBSS_STATE_TURN_REG     3
#define OBSS_STATE_TURN_UPDATE  4
#define OBSS_STATE_WIP_READY    5
#define OBSS_STATE_CHECK_WAN    6

// ----------------------------------------------------------------------------
// Local Variables
// ----------------------------------------------------------------------------
static u32 fw_ver = 0;
static u8 my_mac[6] = {0};
static asc_config_stun_t stun_config;
static u32 stun_ex_ip = 0;
static u16 stun_ex_port = OBS_STUN_PORT + 1;
static obs_server_t obs_server;
static obs_client_t	obs_clients[C_MAX];
static u8 *client_dbuf[C_MAX] = {0};
static u16 obs_port = 0;
static u32 prefer_region_id = 0;

#define CH_MAX  4
#define S_MAX   2
static struct {

    u8  vtype[S_MAX];
    u8  atype;
    u16 width[S_MAX];
    u16 height[S_MAX];
    u16 bitwidth;
    u16 samplerate;

}stream_attr[CH_MAX];

static u32 c_dio = 0;

// ----------------------------------------------------------------------------
// Local Functions
// ----------------------------------------------------------------------------
static u32 csum(u32 *ptr, u32 count)
{
    u32 i;
    u32 sum = 0;
    for(i=0;i<count;i++)
        sum += ptr[i];
    return sum;
}

static int get_ip(u32 *ifaddr)
{
    *ifaddr = 0;

    struct ifreq ifr;
    strcpy(ifr.ifr_name, "eth0");

    int sd = socket(AF_INET, SOCK_STREAM, 0);
    if(sd < 0)
        return -1;
    if(ioctl(sd, SIOCGIFADDR, &ifr) < 0) {
        close(sd);
        return -1;
    }
    *ifaddr = ntohl(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr);
    close(sd);
    return 0;
}

// ----------------------------------------------------------------------------
// Send Stream
// ----------------------------------------------------------------------------
static int send_aframe(obs_client_t *c)
{
    obsp_packet_t *op = (obsp_packet_t *)c->sbuf;
    op->turn_session = c->turn_session;
    op->session = c->session;
    op->sequence = c->apkt_seq++;
    op->status = c_dio;
    op->flag = OBS_FLAG_AUDIO | OBS_FLAG_DIO;
    c->iov[0].iov_base = c->sbuf;
    c->iov[0].iov_len = sizeof(obsp_packet_t) + sizeof(obsp_audio_t);
    obsp_audio_t *oa = (obsp_audio_t *)(c->sbuf + sizeof(obsp_packet_t));
    oa->type = stream_attr[c->ch].atype;
    oa->num = c->au_nr;
    oa->unit_size = c->au_usize;
    c->iov[1].iov_base = c->au_buf[c->au_ridx];
    c->iov[1].iov_len = c->au_size;

    if(c->au_size) {
        int s_size = sendmsg(c->sd, &c->msg, 0);
        if(s_size < 0) {
            obs_err("[%d] sendmsg error: %s\n", c->id, strerror(errno));
            c->fin_code = OBSP_FIN_NET_ERR;
            c->inuse = 0;
            return -1;
        }
        if(s_size < c->au_size) {
            if(s_size)
                obs_err("[%d] sendmsg %d < %d\n", c->id, s_size, c->au_size);
            return -1;
        }
        c->b_total += c->au_size;
        c->byte_sent += c->au_size;
        c->pkt_sent++;
        c->p_total++;
        c->frame_sent++;
        c->f_total++;
        if(c->to_send > c->au_size)
            c->to_send -= c->au_size;
        else
            c->to_send = 0;
        obs_pkt("[%d] send #%u %x %x\n", c->id, op->sequence, op->flag, op->status);
    }

    c->au_inuse[c->au_ridx] = 0;
    c->au_ridx = (c->au_ridx + 1) % AU_QNR;

    return 0;
}

#define CSUM
static int send_packet(obs_client_t *c, q_frame_t *f, u32 seq)
{
    u32 roff = (seq - f->pseq_start) * PKT_DSZ;
    if(roff >= f->size)
        return 0;
    obsp_packet_t *op = (obsp_packet_t *)c->sbuf;
    op->turn_session = c->turn_session;
    op->session = c->session;
    op->status = 0;
    op->sequence = seq;
    op->flag = (f->iskey) ? (OBS_FLAG_KEY|OBS_FLAG_RESEND) : (OBS_FLAG_RESEND); // DIO is invalid here
    c->iov[0].iov_base = c->sbuf;
    if(roff)
        c->iov[0].iov_len = sizeof(obsp_packet_t);
    else {
        op->flag |= OBS_FLAG_START;
        c->iov[0].iov_len = sizeof(obsp_packet_t) + sizeof(obsp_frame_t);
        obsp_frame_t *of = (obsp_frame_t *)(c->sbuf + sizeof(obsp_packet_t));
#ifdef CSUM
        of->timestamp = csum((u32 *)f->ptr, f->size/4);
#else
        c->f_time = (((u64)f->sec) * 1000000) + f->usec;
        if(c->f_time == 0ULL)
            of->timestamp = 0;
        else
            of->timestamp = (u32)c->f_time;
#endif
        of->size = f->size;
    }
    c->iov[1].iov_base = f->ptr + roff;
    int dsize = f->size - roff;
    if(dsize > PKT_DSZ)
        dsize = PKT_DSZ;
    c->iov[1].iov_len = dsize;

    int s_size = sendmsg(c->sd, &c->msg, 0);
    if(s_size < 0) {
        obs_err("[%d] sendmsg error: %s\n", c->id, strerror(errno));
        c->fin_code = OBSP_FIN_NET_ERR;
        c->inuse = 0;
        return -1;
    }
    if(s_size < dsize) {
        if(s_size)
            obs_err("[%d] sendmsg %d < %d\n", c->id, s_size, dsize);
        return -1;
    }
    c->b_total += dsize;
    c->byte_sent += dsize;
    c->pkt_sent++;
    c->p_total++;
    obs_pkt("[%d] resend #%u %x\n", c->id, op->sequence, op->flag);
    if(c->to_send > dsize)
        c->to_send -= dsize;
    else
        c->to_send = 0;
    return 0;
}

static int send_vframe(obs_client_t *c, q_frame_t *f)
{
    obsp_packet_t *op = (obsp_packet_t *)c->sbuf;
    op->turn_session = c->turn_session;
    op->session = c->session;
    op->status = c_dio;
    c->iov[0].iov_base = c->sbuf;

    if(!f->roff) {
        u32 pkt = (f->size/PKT_DSZ) + ((f->size % PKT_DSZ) ? 1:0);
        f->pseq_start = c->pkt_seq;
        f->pseq_end = c->pkt_seq + pkt - 1;
        f->flag = (u8)(c_dio & 0xff);
    }

    while(f->roff < f->size) {
        op->sequence = c->pkt_seq;
        op->flag = (f->iskey) ? (OBS_FLAG_KEY|OBS_FLAG_DIO) : OBS_FLAG_DIO;
        if(f->roff) {
            c->iov[0].iov_len = sizeof(obsp_packet_t);
        }
        else {
            op->flag |= OBS_FLAG_START;
            c->iov[0].iov_len = sizeof(obsp_packet_t) + sizeof(obsp_frame_t);
            obsp_frame_t *of = (obsp_frame_t *)(c->sbuf + sizeof(obsp_packet_t));
#ifdef CSUM
            of->timestamp = csum((u32 *)f->ptr, f->size/4);
#else
            c->f_time = (((u64)f->sec) * 1000000) + f->usec;
            if(c->f_time == 0ULL)
                of->timestamp = 0;
            else
                of->timestamp = (u32)c->f_time;
#endif
            of->size = f->size;
        }
        c->iov[1].iov_base = f->ptr + f->roff;
        int dsize = f->size - f->roff;
        if(dsize > PKT_DSZ)
            dsize = PKT_DSZ;
        c->iov[1].iov_len = dsize;

        if(c->to_send < dsize)
            return -1;

        int s_size = sendmsg(c->sd, &c->msg, 0);
        if(s_size < 0) {
            obs_err("[%d] sendmsg error: %s\n", c->id, strerror(errno));
            c->fin_code = OBSP_FIN_NET_ERR;
            c->inuse = 0;
            return -1;
        }
        if(s_size < dsize) {
            obs_err("[%d] sendmsg %d < %d\n", c->id, s_size, dsize);
            return -1;
        }
        c->pkt_seq++;
        f->roff += dsize;
        c->b_total += dsize;
        c->byte_sent += dsize;
        c->pkt_sent++;
        c->p_total++;
        obs_pkt("[%d] send #%u %x %x\n", c->id, op->sequence, op->flag, op->status);
        if(c->to_send > dsize) {
            c->to_send -= dsize;
        }
        else {
            c->to_send = 0;
            if(f->roff < f->size)
                return -1;
        }
    }

    //obs_dbg("[%d] %x, %x\n", c->id, f->flag, op->flag);

    c->frame_sent++;
    c->f_total++;

    return 0;
}

static void reset_avbuf(obs_client_t *c)
{
    while(c->au_ridx != c->au_widx) {
        c->au_inuse[c->au_ridx] = 0;
        c->au_ridx = (c->au_ridx + 1) % AU_QNR;
    }

    c->rs_widx = c->rs_ridx = 0;

    if(c->w_idx == c->r_idx)
        return ;

    q_frame_t *f = &c->qf[c->r_idx];

    if(f->roff && f->roff < f->size) { // send the rest of last frame
        c->r_off = f->noff;
        c->r_idx = (c->r_idx + 1) % F_MAX;
        c->frame_dropped++;
    }

    c->vr_iskey = 1;
    while(c->r_idx != c->w_idx) {
        c->frame_dropped++;
        u32 pkt = (f->size/PKT_DSZ) + ((f->size % PKT_DSZ) ? 1:0);
        c->pkt_dropped += pkt;
        c->pkt_seq += pkt;
        c->r_off = f->noff;
        c->r_idx = (c->r_idx + 1) % F_MAX;
    }
    c->vr_iskey = 0;
}

static void resend_packet(obs_client_t *c)
{
    if(c->a_idx == c->r_idx) {
        obs_dbg("[%d] resend packets are recycled, rw=%u/%u\n", c->id, c->rs_ridx, c->rs_widx);
        c->rs_ridx = c->rs_widx;
        return;
    }
    u32 idx = c->a_idx;
    while(idx != c->r_idx) {
        q_frame_t *f = &c->qf[idx];
        if(f->pseq_start <= c->rs_q[c->rs_ridx] && f->pseq_end >= c->rs_q[c->rs_ridx]) {

            if(send_packet(c, f, c->rs_q[c->rs_ridx]) < 0)
                return ;

            obs_pkt("[%d] resend packet R:%u %u in #%u [%u-%u]\n", c->id, c->rs_ridx, c->rs_q[c->rs_ridx], idx, f->pseq_start, f->pseq_end);

            c->rs_ridx = (c->rs_ridx + 1) % RS_NR;
            if(c->rs_ridx == c->rs_widx || !c->to_send)
                return ;
            continue;
        }

        //obs_pkt("[%d] resend packet R:%u %u not in #%u [%u-%u]\n", c->id, c->rs_ridx, c->rs_q[c->rs_ridx], idx, f->pseq_start, f->pseq_end);

        idx = (idx + 1) % F_MAX;
    }
    if(c->rs_ridx != c->rs_widx) {
        obs_dbg("[%d] resend packet R:%u %u not in %u-%u, reset to W:%u\n", c->id, c->rs_ridx, c->rs_q[c->rs_ridx], c->a_idx, c->r_idx, c->rs_widx);
        c->rs_ridx = c->rs_widx;
    }
}

static int send_stream(obs_client_t *c)
{
    if(c->rs_widx != c->rs_ridx)
        resend_packet(c);

    if(c->w_idx == c->r_idx)
        return 0;

    q_frame_t *f = &c->qf[c->r_idx];

    if(f->roff && f->roff < f->size) { // send the rest of last frame
        if(send_vframe(c, f) < 0)
            return -1;
        obs_pkt("[%d] f send #%u (r=%u), key=%d\n", c->id, f->seq, c->r_seq, f->iskey);
        c->r_seq = f->seq + 1;
        c->r_off = f->noff;
        c->r_idx = (c->r_idx + 1) % F_MAX;
    }

    u32 delta = (c->w_idx >= c->r_idx) ? (c->w_idx - c->r_idx) : (c->w_idx + F_MAX - c->r_idx);
    u32 drop = 0;
    if(delta > 3) {
        u32 idx = c->w_idx;
        drop = (idx >= c->r_idx) ? (idx - c->r_idx) : (idx + F_MAX - c->r_idx);
        while(idx != c->r_idx) {
            if(c->qf[idx].iskey) {
                drop = (idx >= c->r_idx) ? (idx - c->r_idx) : (idx + F_MAX - c->r_idx);
                break;
            }
            idx = (idx) ? (idx-1) : (F_MAX-1);
        }
    }
    while(c->r_idx != c->w_idx) {
        f = &c->qf[c->r_idx];
        c->vr_iskey = f->iskey;
        if(drop || (f->iskey == 0 && (c->v_skip || (c->r_seq != f->seq)))) {
            c->frame_dropped++;
            u32 pkt = (f->size/PKT_DSZ) + ((f->size % PKT_DSZ) ? 1:0);
            c->pkt_dropped += pkt;
            c->pkt_seq += pkt;
            obs_pkt("[%d] f drop %u pkt @ fseq#%u (r=%u), drop=%u, key=%d, skip=%d\n", c->id, pkt, f->seq, c->r_seq, drop, f->iskey, c->v_skip);
            c->r_seq = f->seq;
            if(drop)
                drop--;
        }
        else {
            if(send_vframe(c, f) < 0) {
                return -1;
            }
            obs_pkt("[%d] f send #%u (r=%u), key=%d\n", c->id, f->seq, c->r_seq, f->iskey);
            c->r_seq = f->seq + 1;
        }

        c->r_off = f->noff;
        c->r_idx = (c->r_idx + 1) % F_MAX;
    }
    c->vr_iskey = 0;

    obs_pkt("[%d] f:%u,p:%u,b:%qu | Drop f:%u,p:%u (%u)\n", c->id, c->f_total, c->p_total, c->b_total,
            c->frame_dropped, c->pkt_dropped, c->pkt_seq);

    return 0;
}

// ----------------------------------------------------------------------------
// Send & Recv
// ----------------------------------------------------------------------------
static int send_fin(int id, int sd, u32 status, u32 turn_session)
{
    obsp_cmd_t cmd;
    cmd.session = turn_session;
    cmd.magic = OBSP_MAGIC;
    cmd.cmd = OBSP_PEER_FIN;
    cmd.param = status;
    if(send(sd, (char *)&cmd, sizeof(obsp_cmd_t), 0) < 0) {
        obs_err("[%d] send FIN error: %s\n", id, strerror(errno));
        return -1;
    }

    obs_pkt("[%d] sent FIN\n", id);

    return 0;
}

static int client_recvfrom(obs_client_t *c)
{
    fd_set rds;
    struct timeval tv;
    int retval;

    FD_ZERO(&rds);
    FD_SET(c->sd, &rds);

    tv.tv_sec = 0;
    tv.tv_usec = 250000;

    retval = select(c->sd+1, &rds, 0, 0, &tv);

    if(!c->inuse)
        return -1;

    if(retval < 0) {
        obs_err("[%d] select error: %s\n", c->id, strerror(errno));
        return -1;
    }
    else if(retval) {
        if(FD_ISSET(c->sd, &rds)) {
            socklen_t addr_len = sizeof(c->peer);
            c->r_size = recvfrom(c->sd, c->rbuf, RBUF_SZ, 0, (struct sockaddr *)&c->peer, &addr_len);
            if(c->r_size < 0) {
                obs_err("[%d] recvfrom error:(%d)%s\n", c->id, errno, strerror(errno));
                return -1;
            }
            return c->r_size;
        }
    }
    else if(c->ext_recv) {
        memcpy(c->rbuf, c->ext_rbuf, c->ext_recv);
        c->peer.sin_port = htons(c->ext_port);
        c->peer.sin_addr.s_addr = htonl(c->ext_ip);
        c->r_size = c->ext_recv;
        c->ext_recv = 0;
        return c->r_size;
    }
    return 0;
}

static int client_sendto(obs_client_t *c, u32 ip, u16 port, int size)
{
    struct sockaddr_in  peer;
    peer.sin_family = PF_INET;
    peer.sin_port = htons(port);
    peer.sin_addr.s_addr = htonl(ip);
    int s_size = sendto(c->sd, c->sbuf, size, 0, (struct sockaddr *)&peer, sizeof(struct sockaddr_in));
    if(s_size < 0) {
        obs_err("[%d] sendto error: %s\n", c->id, strerror(errno));
        return -1;
    }
    return s_size;
}

static int client_send_cmd(obs_client_t *c, u32 ip, u16 port, u32 cmd, u32 param)
{
    int s_size = 0;
    obsp_cmd_t *oc = (obsp_cmd_t *)c->sbuf;
    oc->session = c->turn_session;
    oc->magic = OBSP_MAGIC;
    oc->cmd = cmd;
    oc->param = param;
    if(ip && port) {
        struct sockaddr_in  peer;
        peer.sin_family = PF_INET;
        peer.sin_port = htons(port);
        peer.sin_addr.s_addr = htonl(ip);
        obs_dbg("[%d] send %x,%u to %x:%u (id=%u)\n", c->id, cmd, param, ip, port, c->peer_id);
        s_size = sendto(c->sd, c->sbuf, sizeof(obsp_cmd_t), 0, (struct sockaddr *)&peer, sizeof(struct sockaddr_in));
        if(s_size < 0) {
            obs_err("[%d] sendto error: %s\n", c->id, strerror(errno));
            return -1;
        }
    }
    else {
        s_size = send(c->sd, c->sbuf, sizeof(obsp_cmd_t), 0);
        if(s_size < 0) {
            obs_err("[%d] send error: %s\n", c->id, strerror(errno));
            return -1;
        }
    }
    return s_size;
}

static int client_send_stream_attr(obs_client_t *c)
{
    obsp_stream_attr_t *sa = (obsp_stream_attr_t *)c->sbuf;
    int a_size = sizeof(obsp_stream_attr_t);
    memset(sa, 0, a_size);
    sa->cmd.session = c->turn_session;
    sa->cmd.magic = OBSP_MAGIC;
    sa->cmd.cmd = OBSP_CMD_STREAM;
    sa->cmd.param = 0;
    sa->session = c->session;
    sa->vtype = stream_attr[c->ch].vtype[c->stream_id];
    sa->atype = (c->audio) ? stream_attr[c->ch].atype : OBS_UNKNOWN;
    sa->width = stream_attr[c->ch].width[c->stream_id];
    sa->height = stream_attr[c->ch].height[c->stream_id];
    sa->bitwidth = stream_attr[c->ch].bitwidth;
    sa->samplerate = stream_attr[c->ch].samplerate;
    if(send(c->sd, c->sbuf, a_size, 0) < a_size) {
        obs_err("[%d] send STREAM ACK ATTR error: %s\n", c->id, strerror(errno));
        c->fin_code = OBSP_FIN_NET_ERR;
        c->inuse = 0;
        return -1;
    }
    obs_dbg("[%d] send STREAM ATTR ACK (ch%u-%u(%d), s:%u)\n", c->id, c->ch, c->stream_id, c->audio, c->session);
    return 0;
}

static void client_reset(obs_client_t *c)
{
    c->last_ack = (u32)time(0);
    c->send_start = jh_msec();
    c->send_stat_idx = 0;
    jh_zero(c->send_b_avg, sizeof(c->send_b_avg));
    jh_zero(c->send_p_avg, sizeof(c->send_p_avg));
    c->send_byte_avg = c->send_pkt_avg = 0;
    c->recv_stat_idx = 0;
    jh_zero(c->recv_b_avg, sizeof(c->recv_b_avg));
    jh_zero(c->recv_p_avg, sizeof(c->recv_p_avg));
    c->recv_byte_avg = c->recv_pkt_avg = 0;
    c->au_ridx = c->au_widx = c->au_woff = c->au_wnr = 0;
    c->au_size = c->au_usize = c->au_nr = 0;
    jh_zero(c->au_inuse, sizeof(c->au_inuse));
    c->msg.msg_iov = c->iov;
    c->msg.msg_iovlen = 2;
}

static int stream_request(obs_client_t *c, obsp_stream_req_t *sr)
{
    u8 ch = sr->ch;
    u8 stream_id = sr->id & 0x0f;
    int audio = (sr->id & 0x80) ? 1 : 0;
    if(ch != c->ch || stream_id != c->stream_id) {
        c->v_skip = 1;
        c->ch = OBS_UNKNOWN;
        reset_avbuf(c);
        c->avg_Bps = 0;
    }
    else if(audio && !c->audio) {
        if(c->avg_Bps < 10000) {
            client_reset(c);
            c->avg_Bps = 10000;
        }
        else
            c->avg_Bps += 10000;
    }
    c->audio = audio;
    c->stream_id = stream_id;
    c->ch = ch;
    client_send_stream_attr(c);
    return 0;
}

// ----------------------------------------------------------------------------
// Hand Shaking
// ----------------------------------------------------------------------------
static int do_sync(obs_client_t *c)
{
    u32 start = (u32)time(0);
    int state = 0;

    obs_dbg("[%d] start SYNC to %u\n", c->id, c->peer_id);

    c->c_idx = -1;
    c->c_ip = 0;
    c->c_port = 0;

    while(1) {
        int i;
        int j = 0;

        if(state == 0) {
            // Send SYN
            for(i=0;i<OBST_CONN_NR;i++) {
                if(c->peer_ip[i] && c->peer_port[i]) {
                    if(client_send_cmd(c, c->peer_ip[i], c->peer_port[i], OBSP_PEER_HS, OBSP_PEER_SYNC) < 0)
                        return -1;
                    j++;
                }
            }
            if(!j) {
                obs_err("[%d] no valid peer, closed\n", c->id);
                return -1;
            }
        }
        else {
            // Send SYN+ACK
            if(client_send_cmd(c, c->c_ip, c->c_port, OBSP_PEER_HS, OBSP_PEER_SYNC_ACK) < 0)
                return -1;
        }

        int rsize = client_recvfrom(c);
        if(rsize) {
            obsp_cmd_t *oc = (obsp_cmd_t *)(c->rbuf);
            if(oc->magic != OBSP_MAGIC || rsize < sizeof(obsp_cmd_t))
                continue;
            u32 ip = ntohl(c->peer.sin_addr.s_addr);
            u16 port = ntohs(c->peer.sin_port);
            obs_pkt("[%d] Got 0x%x(%u) from %x:%u\n", c->id, oc->cmd, oc->param, ip, port);
            if(state == 0) {
                if(oc->cmd == OBSP_PEER_HS && oc->param >= OBSP_PEER_SYNC) {
                    for(i=0;i<OBST_CONN_NR;i++) {
                        if(c->peer_ip[i] && c->peer_port[i]) {
                            if(c->peer_ip[i] == ip && c->peer_port[i] == port) {
                                break;
                            }
                        }
                    }
                    if(i < OBST_CONN_NR) {
                        c->c_idx = i; // OBST_CONN_NR = LAN
                        c->c_ip = ip;
                        c->c_port = port;
                        state = 1;
                        obs_dbg("[%d] SYNC\n", c->id);
                    }
                }
            }
            else {
                if(oc->cmd == OBSP_PEER_HS && oc->param >= OBSP_PEER_SYNC_ACK) {
                    if(client_send_cmd(c, c->c_ip, c->c_port, OBSP_PEER_HS, OBSP_PEER_SYNC_ACK) < 0)
                        return -1;
                    return 0;
                }
                else if(oc->cmd == OBSP_CMD_STREAM) {
                    obs_dbg("[%d] got STREAM Req -> connected\n", c->id);
                    return 0;
                }
            }
        }
        else { // check timeout
            u32 now = (u32)time(0);
            if((now - start) > 5) {
                obs_err("[%d] timeout, closed\n", c->id);
                return -1;
            }
        }
    }

    return -1;
}

// ----------------------------------------------------------------------------
static void rstat_check(obs_client_t *c, obsp_stat_receiver_t *rr)
{
    if(rr->duration >= 500) {
        c->recv_b_avg[c->recv_stat_idx] = (rr->byte_received * 1000) / rr->duration;
        c->recv_p_avg[c->recv_stat_idx] = ((rr->vpkt_received+rr->apkt_received) * 1000) / rr->duration;
        c->recv_stat_idx = (c->recv_stat_idx + 1) % STAT_NR;
        int i;
        int j = 0;
        u32 b_sum = 0;
        u32 p_sum = 0;
        for(i=0;i<STAT_NR;i++) {
            if(c->recv_b_avg[i]) {
                b_sum += c->recv_b_avg[i];
                p_sum += c->recv_p_avg[i];
                j++;
            }
        }
        c->recv_byte_avg = (j) ? b_sum / j : 0;
        c->recv_pkt_avg = (j) ? p_sum / j : 0;
        if(c->recv_byte_avg < ((c->send_byte_avg * 8) / 10)) {
            c->avg_Bps = c->recv_byte_avg + 1024;
            c->v_skip = 1;
            if(c->avg_Bps < 8192)
                c->avg_Bps = 8192;
        }
        else {
            c->avg_Bps = (c->send_byte_avg * 12) / 10;
        }
        //
        // TODO: adjust!
        //
        obs_dbg("[%d] RECV AVG: %u bytes, %u pkts, Bps -> %u\n", c->id, c->recv_byte_avg, c->recv_pkt_avg, c->avg_Bps);
    }
    else {
        obs_dbg("[%d] got RR with duration = %u\n", c->id, rr->duration);
    }
}

static void resend_add(obs_client_t *c, u32 nr, u16 *seq)
{
    if(!nr || !seq)
        return;
    if(nr >= RS_NR) {
        obs_err("[%d] RESEND %u > %d packets is over limited\n", c->id, nr, RS_NR);
        return;
    }
    int i;
    for(i=0;i<nr;i++) {
        u32 next = (c->rs_widx + 1) % RS_NR;
        if(next == c->rs_ridx) {
            obs_dbg("[%d] too many RESEND, reset R @ %u\n", c->id, c->rs_ridx);
            c->rs_ridx = c->rs_widx;
        }
        obs_pkt("[%d] add RESEND %u @ %u\n", c->id, seq[i], c->rs_widx);
        c->rs_q[c->rs_widx] = seq[i];
        c->rs_widx = next;
    }
}

static int client_parser(obs_client_t *c)
{
    obsp_cmd_t *oc = (obsp_cmd_t *)(c->rbuf);

    if(oc->magic != OBSP_MAGIC || c->r_size < sizeof(obsp_cmd_t)) {
        obs_pkt("[%d] recv unknown pkt: M=%x, sz=%d\n", c->id, oc->magic, c->r_size);
        return 0;
    }

    if(oc->cmd == OBSP_PEER_HS) {
        if(oc->param == OBSP_PEER_SYNC_ACK) {
            if(client_send_cmd(c, c->c_ip, c->c_port, OBSP_PEER_HS, OBSP_PEER_SYNC_ACK) < 0)
                return -1;
        }
    }
    else if(oc->cmd == OBSP_PEER_FIN) {
        if(c->session != oc->param) {
            obs_err("[%d] wrong session: %u != %u\n", c->id, oc->param, c->session);
        }
        c->inuse = 0;
        obs_dbg("[%d] got FIN command\n", c->id);
        return -1;
    }
    else if(oc->cmd == OBSP_CMD_STREAM) {
        obs_dbg("[%d] got STREAM Req\n", c->id);
        stream_request(c, (obsp_stream_req_t *)c->rbuf);
    }
    else if(oc->cmd == OBSP_STAT_RECEIVER) {
        c->last_ack = (u32)time(0);
        memcpy(&c->recv_report, c->rbuf, sizeof(obsp_stat_receiver_t));
        rstat_check(c, &c->recv_report);
    }
    else if(oc->cmd == OBSP_CMD_DIO) {
        obsp_dio_t *od = (obsp_dio_t *)(&oc->param);
        obs_dbg("[%d] got DIO: c:%x, v=%x\n", c->id, od->check, od->value);
#ifndef STANDALONE
        vs1_set_do(od->value,0);
#endif
        od->value = c_dio;
        if(client_send_cmd(c, 0, 0, OBSP_CMD_DIO, oc->param) < 0)
            return -1;
    }
    else if(oc->cmd == OBSP_CMD_AOUT) {
        // TODO: check sequence ?
        obsp_audio_t *oa = (obsp_audio_t *)(c->rbuf + sizeof(obsp_cmd_t));
        if(oa->type == OBS_ATYPE_ADPCM_DVI4 && (oa->num * oa->unit_size) < PKT_DSZ) {
            int n;
            u8 *ptr = c->rbuf + sizeof(obsp_cmd_t) + sizeof(obsp_audio_t);
            for(n=0;n<oa->num;n++) {
#ifndef STANDALONE
                vs1_audio_out(ptr, oa->unit_size);
#endif
                ptr += oa->unit_size;
            }
        }
    }
    else if(oc->cmd == OBSP_CMD_RING_CTRL) {
        obs_dbg("[%d] got RING: %u\n", c->id, oc->param);
#ifndef STANDALONE
        vs1_set_ring(oc->param);
#endif
        if(client_send_cmd(c, 0, 0, OBSP_CMD_RING_CTRL, oc->param) < 0)
            return -1;
    }
    else if(oc->cmd == OBSP_CMD_RESEND) {
        obs_dbg("[%d] got RESEND: %u\n", c->id, oc->param);
        resend_add(c, oc->param, (u16 *)(c->rbuf + sizeof(obsp_cmd_t)));
    }

    return 0;
}

static int time_check(obs_client_t *c)
{
    u32 now = (u32)time(0);
    if(now > c->last_ack) {
        if((now - c->last_ack) > 10) {
            obs_dbg("[%d] no ack timeout\n", c->id);
            c->fin_code = OBSP_FIN_NO_ACK;
            c->inuse = 0;
            return -1;
        }
    }
    else if(now < c->last_ack) {
        c->last_ack = now;
    }

    u64 ms = jh_msec();
    if((ms - c->send_start) > 1000) {
        if(!c->byte_sent) {
            obsp_cmd_t *oc = (obsp_cmd_t *)(c->rbuf);
            obsp_dio_t *od = (obsp_dio_t *)(&oc->param);
            od->check = 0;
            od->value = c_dio;
            client_send_cmd(c, 0, 0, OBSP_CMD_DIO, oc->param);
            obs_dbg("[%d] send DIO: c:%x, v=%x (Keep Alive)\n", c->id, od->check, od->value);
        }
        c->send_b_avg[c->send_stat_idx] = (c->byte_sent * 1000) / (ms - c->send_start);
        c->send_p_avg[c->send_stat_idx] = (c->pkt_sent * 1000) / (ms - c->send_start);
        c->send_stat_idx = (c->send_stat_idx + 1) % STAT_NR;
        c->send_start = ms;
        c->byte_sent = c->pkt_sent = 0;
        int i;
        int j = 0;
        u32 b_sum = 0;
        u32 p_sum = 0;
        for(i=0;i<STAT_NR;i++) {
            if(c->send_b_avg[i]) {
                b_sum += c->send_b_avg[i];
                p_sum += c->send_p_avg[i];
                j++;
            }
        }
        c->send_byte_avg = (j) ? b_sum / j : 0;
        c->send_pkt_avg = (j) ? p_sum / j : 0;
        obs_dbg("[%d] SEND AVG: %u bytes, %u pkts\n", c->id, c->send_byte_avg, c->send_pkt_avg);
    }

    return 0;
}

static void client_check_queue(obs_client_t *c)
{
    if(c->r_idx != c->w_idx) {
        q_frame_t *f = &c->qf[c->r_idx];
        if(!f->roff || f->roff >= f->size) { // do not drop if the current frame is not finished
            u32 diff = (c->w_idx >= c->r_idx) ? (c->w_idx - c->r_idx) : (c->w_idx + F_MAX - c->r_idx);
            u32 dsize = (c->w_off >= c->r_off) ? (c->w_off - c->r_off) : (c->w_off + DBUF_SZ - c->r_off);
            if(c->v_skip || diff > 30 || dsize > (DBUF_SZ/2)) {
                u32 start = c->r_idx;
                while(c->r_idx != c->w_idx) {
                    q_frame_t *f = &c->qf[c->r_idx];
                    if(f->iskey && c->r_idx != start)
                        break;
                    c->r_off = f->noff;
                    c->r_idx = (c->r_idx + 1) % F_MAX;
                }
            }
        }
    }
    if(c->a_idx != c->r_idx) {
        u32 diff = (c->r_idx >= c->a_idx) ? (c->r_idx - c->a_idx) : (c->r_idx + F_MAX - c->a_idx);
        u32 dsize = (c->r_off >= c->a_off) ? (c->r_off - c->a_off) : (c->r_off + DBUF_SZ - c->a_off);
        if(diff >= 60 || dsize >= (DBUF_SZ/4)) {
            u32 start = c->a_idx;
            while(c->a_idx != c->r_idx) {
                q_frame_t *f = &c->qf[c->a_idx];
                if(f->iskey && c->a_idx != start)
                    break;
                c->a_off = f->noff;
                c->a_idx = (c->a_idx + 1) % F_MAX;
            }
        }
    }
}

// ----------------------------------------------------------------------------
static void *client_thread(void *arg)
{
    obs_client_t *c = (obs_client_t *)arg;

    int id = c->id;
    obs_dbg("[%d] thread start .....PID: %u\n", id, (u32)jh_gettid());

    int sd = c->sd = -1;

_client_init_:
    if(sd >= 0) {
        if(c->fin_code) {
            send_fin(id, sd, c->fin_code, c->turn_session);
            c->fin_code = 0;
        }
        close(sd);
        sd = -1;
        obs_err("[%d] closed\n", id);
    }

    while(!c->inuse || c->turn_mode) {
        jh_msleep(50);
    }

    sd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(sd < 0) {
        obs_err("[%d] socket error: %s\n", id, strerror(errno));
        goto _client_exit_;
    }
    else {
        int val = 1;
        if(setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0) {
            obs_err("[%d] set SO_REUSEADDR error: %s\n", id, strerror(errno));
            goto _client_init_;
        }

        struct sockaddr_in  my_addr;
        my_addr.sin_family = PF_INET;
        my_addr.sin_port = htons(obs_port);
        my_addr.sin_addr.s_addr = INADDR_ANY;
        if(bind(sd, (struct sockaddr *)&my_addr, sizeof(my_addr)) < 0) {
            obs_err("[%d] bind error: %s\n", id, strerror(errno));
            goto _client_init_;
        }
    }

    c->id = id;
    c->sd = sd;
    c->d_buf = client_dbuf[id];

    // start SYNC
    if(do_sync(c) < 0) {
        c->inuse = 0;
        obs_dbg("[%d] SYNC Failed with %u\n", id, c->peer_id);
        goto _client_init_;
    }
    else {
        struct sockaddr_in  peer;
        peer.sin_family = PF_INET;
        peer.sin_port = htons(c->c_port);
        peer.sin_addr.s_addr = htonl(c->c_ip);
        connect(sd, (struct sockaddr *)&peer, sizeof(peer));
    }

    obs_dbg("[%d] connected to 0x%x:%u(%d), id=%u\n", id, c->c_ip, c->c_port, c->c_idx, c->peer_id);

    client_reset(c);

    while(1) {
        fd_set rds, wds;
        struct timeval tv;
        int retval;

        FD_ZERO(&rds);
        FD_ZERO(&wds);
        FD_SET(sd, &rds);
        FD_SET(sd, &wds);

        tv.tv_sec = 0;
        tv.tv_usec = 20000;

        retval = select(sd+1, &rds, &wds, 0, &tv);

        if(c->to_close) {
            obs_dbg("[%d] closed\n", id);
            break;
        }
        if(!c->inuse) {
            obs_dbg("[%d] stopped\n", id);
            goto _client_init_;
        }

        if(retval < 0) {
            obs_err("[%d] select error: %s\n", id, strerror(errno));
            goto _client_init_;
        }
        else if(retval) {
            u64 ms = jh_msec();
            if(c->avg_Bps) {
                if(!c->last_send)
                    c->last_send = ms - 15;
                c->to_send = (u32)(((ms - c->last_send) * c->avg_Bps) / 1000);
            }
            else
                c->to_send = 0xffffffff;

            //obs_tc("[%qu-%qu=%u] w/r/a: %u/%u/%u, %u/%u\n", ms, c->last_send, (u32)(ms-c->last_send), c->w_idx, c->r_idx, c->a_idx, c->avg_Bps, c->to_send);

            if(FD_ISSET(sd, &rds)) {
                c->r_size = recv(c->sd, (char *)c->rbuf, RBUF_SZ, 0);
                if(c->r_size < 0) {
                    obs_err("[%d] recv error:(%d)%s\n", c->id, errno, strerror(errno));
                    c->fin_code = OBSP_FIN_NET_ERR;
                    c->inuse = 0;
                    goto _client_init_;
                }
                if(c->r_size) {
                    if(client_parser(c) < 0) {
                        goto _client_init_;
                    }
                }
            }

            if(FD_ISSET(sd, &wds)) {

                if(c->to_send > 500) {
                    if(c->au_inuse[c->au_ridx]) {
                        if(send_aframe(c) == 0)
                            c->last_send = ms;
                    }
                }
                if(c->to_send > 1000) {
                    if(send_stream(c) == 0 || c->to_send < 500)
                        jh_msleep(10);
                }
                else
                    jh_msleep(10);

                client_check_queue(c);
            }
        }
        else if(c->ext_recv) {
            if(c->c_ip == c->ext_ip && c->c_port == c->ext_port) {
                memcpy(c->rbuf, c->ext_rbuf, c->ext_recv);
                c->r_size = c->ext_recv;
                if(client_parser(c) < 0) {
                    goto _client_init_;
                }
            }
            c->ext_recv = 0;
        }

        if(time_check(c) < 0) {
            goto _client_init_;
        }
    }

_client_exit_:
    c->dead = 1;
    c->inuse = 0;
    if(sd >= 0)
        close(sd);
    obs_err("[%d] exit\n", id);
    return 0;
}

// ----------------------------------------------------------------------------
// TURN Thread
// ----------------------------------------------------------------------------
static int turn_send_reset(int sd, u32 session)
{
    obst_turn_cmd_t tc;
    obst_tc_update_t *tu = (obst_tc_update_t *)tc.param;
    memset(&tc, 0, sizeof(obst_turn_cmd_t));
    tc.session = session;
    tc.magic = OBST_MAGIC;
    tc.cmd = OBST_TURN_UPDATE;
    tu->peer_id = 0xff;
    tu->flags = 0xff;
    if(send(sd, (char *)&tc, sizeof(obst_turn_cmd_t), 0) < sizeof(obst_turn_cmd_t)) {
        obs_err("TURN send reset error: %s\n", strerror(errno));
        return -1;
    }
    obs_dbg("TURN send reset\n");
    return 0;
}

static int turn_send_update(obs_server_t *obs, obs_client_t *c)
{
    obst_turn_cmd_t *tc = (obst_turn_cmd_t *)c->sbuf;
    obst_tc_update_t *tu = (obst_tc_update_t *)tc->param;
    memset(tc, 0, sizeof(obst_turn_cmd_t));
    tc->session = obs->session_base;
    tc->magic = OBST_MAGIC;
    tc->cmd = OBST_TURN_UPDATE;
    tu->peer_id = c->id;
    tu->flags = 0;
    tu->ip = c->peer_ip[0];
    tu->port = c->peer_port[0];
    if(send(c->sd, c->sbuf, sizeof(obst_turn_cmd_t), 0) < sizeof(obst_turn_cmd_t)) {
        obs_err("[%d] send TURN UPDATE error: %s\n", c->id, strerror(errno));
        return -1;
    }
    obs_dbg("[%d] send TURN UPDATE: 0x%x:%u\n", c->id, c->peer_ip[0], c->peer_port[0]);
    return 0;
}

static int turn_c_parser(int sd, u8 *buf, int size, obs_server_t *obs)
{
    u32 session = *((u32 *)buf);

    if(session == obs->session_base) {
        obst_turn_cmd_t *tc = (obst_turn_cmd_t *)buf;
        if(tc->magic != OBST_MAGIC)
            return 0;
        if(tc->cmd == OBST_TURN_UPDATE) {
            obst_tc_update_t *tu = (obst_tc_update_t *)tc->param;
            if(tu->peer_id < C_MAX) {
                obs_client_t *c = &obs_clients[tu->peer_id];
                if(c->inuse && (tu->ip != c->peer_ip[0] || tu->port != c->peer_port[0])) {
                    obs_dbg("TURN UPDATE mismatch #%u, %x, %x:%u != %x:%u\n", tu->peer_id, tu->flags, tu->ip, tu->port, c->peer_ip[0], c->peer_port[0]);
                    turn_send_update(obs, c);
                    return 0;
                }
            }
            obs_dbg("TURN Got UPDATE RESP: #%u, %x, %x:%u\n", tu->peer_id, tu->flags, tu->ip, tu->port);
            obs->turn_update_ts = (u32)time(0);
        }
        return 0;
    }

    u32 idx = session - obs->session_base - 1;
    if(idx >= C_MAX)
        return -1;

    obs_client_t *c = &obs_clients[idx];
    if(!c->inuse)
        return -1;

    obsp_cmd_t *oc = (obsp_cmd_t *)buf;
    if(oc->magic != OBSP_MAGIC || size < sizeof(obsp_cmd_t)) {
        obs_pkt("[%d] recv unknown pkt: M=%x, sz=%d\n", c->id, oc->magic, size);
        return 0;
    }

    if(oc->cmd == OBSP_PEER_HS) {
        if(oc->param == OBSP_PEER_SYNC) {
            if(c->state < OBSC_STATE_SYNC_ACK) {
                c->state = OBSC_STATE_SYNC_ACK;
                obs_dbg("[%d] SYNC (TURN)\n", c->id);
            }
        }
        if(oc->param == OBSP_PEER_SYNC_ACK) {
            if(c->state < OBSC_STATE_STREAM_REQ) {
                c->state = OBSC_STATE_STREAM_REQ;
                obs_dbg("[%d] SYNC ACK (TURN)\n", c->id);
            }
        }
    }
    else if(oc->cmd == OBSP_CMD_STREAM) {
        if(c->state < OBSC_STATE_STREAM_REQ)
            c->state = OBSC_STATE_STREAM_REQ;
        obs_dbg("[%d] Got STREAM Req (TURN)\n", c->id);
        stream_request(c, (obsp_stream_req_t *)buf);
    }
    else if(oc->cmd == OBSP_PEER_FIN) {
        if(c->session != oc->param) {
            obs_err("[%d] FIN with wrong session: %u != %u (TURN)\n", c->id, oc->param, c->session);
        }
        c->inuse = 0;
        obs_dbg("[%d] Got FIN command (TURN)\n", c->id);
        return -1;
    }
    else if(oc->cmd == OBSP_STAT_RECEIVER) {
        c->last_ack = (u32)time(0);
        memcpy(&c->recv_report, buf, sizeof(obsp_stat_receiver_t));
        rstat_check(c, &c->recv_report);
    }
    else if(oc->cmd == OBSP_CMD_DIO) {
        obsp_dio_t *od = (obsp_dio_t *)(&oc->param);
        obs_dbg("[%d] Got DIO: c:%x, v=%x (TURN)\n", c->id, od->check, od->value);
#ifndef STANDALONE
        vs1_set_do(od->value,0);
#endif
        od->value = c_dio;
        if(client_send_cmd(c, 0, 0, OBSP_CMD_DIO, oc->param) < 0)
            return -1;
    }
    else if(oc->cmd == OBSP_CMD_AOUT) {
        // TODO: check sequence ?
        obsp_audio_t *oa = (obsp_audio_t *)(buf + sizeof(obsp_cmd_t));
        if(oa->type == OBS_ATYPE_ADPCM_DVI4 && (oa->num * oa->unit_size) < PKT_DSZ) {
            int n;
            u8 *ptr = buf + sizeof(obsp_cmd_t) + sizeof(obsp_audio_t);
            for(n=0;n<oa->num;n++) {
#ifndef STANDALONE
                vs1_audio_out(ptr, oa->unit_size);
#endif
                ptr += oa->unit_size;
            }
        }
    }
    else if(oc->cmd == OBSP_CMD_RING_CTRL) {
        obs_dbg("[%d] got RING: %u (TURN)\n", c->id, oc->param);
#ifndef STANDALONE
        vs1_set_ring(oc->param);
#endif
        if(client_send_cmd(c, 0, 0, OBSP_CMD_RING_CTRL, oc->param) < 0)
            return -1;
    }
    else if(oc->cmd == OBSP_CMD_RESEND) {
        obs_dbg("[%d] Got RESEND: %u (TURN)\n", c->id, oc->param);
        resend_add(c, oc->param, (u16 *)(buf + sizeof(obsp_cmd_t)));
    }

    return 0;
}

static void *turn_thread(void *arg)
{
    obs_server_t *obs = (obs_server_t *)arg;

    obs_dbg("TURN thread start .....PID: %u\n", (u32)jh_gettid());

    int sd = -1;
    u8 *rbuf = (u8 *)malloc(RBUF_SZ);

_turn_client_init_:
    if(sd >= 0) {
#if 0
        if(c->fin_code) {
            send_fin(id, sd, c->fin_code, c->turn_session);
            c->fin_code = 0;
        }
#endif
        close(sd);
        sd = -1;
        obs_err("TURN closed\n");
    }

    while(!obs->using_turn || obs->state < OBSS_STATE_WIP_READY)
        jh_msleep(50);

    sd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(sd < 0) {
        obs_err("TURN socket error: %s\n", strerror(errno));
        sleep(1);
        goto _turn_client_init_;
    }
    else {
        int val = 1;
        if(setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0) {
            obs_err("TURN set SO_REUSEADDR error: %s\n", strerror(errno));
            sleep(1);
            goto _turn_client_init_;
        }

        struct sockaddr_in  my_addr;
        my_addr.sin_family = PF_INET;
        my_addr.sin_port = htons(obs_port);
        my_addr.sin_addr.s_addr = INADDR_ANY;
        if(bind(sd, (struct sockaddr *)&my_addr, sizeof(my_addr)) < 0) {
            obs_err("TURN bind error: %s\n", strerror(errno));
            sleep(1);
            goto _turn_client_init_;
        }
        struct sockaddr_in  peer;
        peer.sin_family = PF_INET;
        peer.sin_port = htons(obs->turn_wport);
        peer.sin_addr.s_addr = htonl(obs->turn_wip);
        connect(sd, (struct sockaddr *)&peer, sizeof(peer));
        obs_dbg("TURN connected to 0x%x:%u\n", obs->turn_wip, obs->turn_wport);
    }

    obs->turn_update_ts = 0;
    obs->turn_traffic_ts = (u32)time(0);
    u32 last_sent = 0;
    u32 act_map = 0;
    u32 empty_map = 0;
    int i = 0;
    while(1) {
        fd_set rds, wds;
        struct timeval tv;
        int retval;
        int write_set = 0;

        FD_ZERO(&rds);
        FD_ZERO(&wds);
        FD_SET(sd, &rds);
        FD_SET(sd, &wds);

        tv.tv_sec = 0;
        tv.tv_usec = 20000;

        retval = select(sd+1, &rds, &wds, 0, &tv);

        u32 now = (u32)time(0);
        if(retval < 0) {
            obs_err("TURN select error: %s\n", strerror(errno));
            goto _turn_client_init_;
        } else if(retval) {
            if(FD_ISSET(sd, &rds)) {
                int rsize = recv(sd, (char *)rbuf, RBUF_SZ, 0);
                if(rsize >= sizeof(obsp_cmd_t)) {
                    turn_c_parser(sd, rbuf, rsize, obs);
                    obs->turn_traffic_ts = now;
                }
            }
            if(FD_ISSET(sd, &wds))
                write_set = 1;
        }

        if(!obs->using_turn || obs->state < OBSS_STATE_WIP_READY)
            goto _turn_client_init_;

        obs_client_t *c = &obs_clients[i];
        if(c->inuse) {
            act_map |= (1 << i);
            if(c->state == OBSC_STATE_INIT) {
                c->sd = sd;
                c->c_idx = -1;
                c->c_ip = obs->turn_wip;
                c->c_port = obs->turn_wport;
                c->state_start = now;
                c->state_cmd_start = 0;
                c->state = OBSC_STATE_SYNC;
                empty_map |= 1 << i;
                turn_send_update(obs, c);
            }
            else if(c->state == OBSC_STATE_SYNC) {
                if((now - c->state_start) > 10) {
                    obs_dbg("[%d] SYNC timeout (TURN)\n", c->id);
                    c->inuse = 0;
                }
                else {
                    if(now != c->state_cmd_start) {
                       if(client_send_cmd(c, 0, 0, OBSP_PEER_HS, OBSP_PEER_SYNC) < 0)
                           goto _turn_client_init_;
                       c->state_cmd_start = now;
                    }
                }
            }
            else if(c->state == OBSC_STATE_SYNC_ACK) {
                if((now - c->state_start) > 10) {
                    obs_dbg("[%d] SYNC ACK timeout (TURN)\n", c->id);
                    c->inuse = 0;
                }
                else {
                    if(now != c->state_cmd_start) {
                       if(client_send_cmd(c, 0, 0, OBSP_PEER_HS, OBSP_PEER_SYNC_ACK) < 0)
                           goto _turn_client_init_;
                       c->state_cmd_start = now;
                    }
                }
            }
            else if(c->state == OBSC_STATE_STREAM_REQ) {
                client_reset(c);
                c->state_start = now;
                c->state_cmd_start = 0;
                c->state = OBSC_STATE_CONNECTED;
                obs_dbg("[%d] Connected (TURN)\n", c->id);
            }
            else if(c->state == OBSC_STATE_CONNECTED) {
                if(write_set) {
                    u64 ms = jh_msec();
                    if(c->avg_Bps) {
                        if(!c->last_send)
                            c->last_send = ms - 15;
                        c->to_send = (u32)(((ms - c->last_send) * c->avg_Bps) / 1000);
                    }
                    else
                        c->to_send = 0xffffffff;
                    if(c->to_send > 500) {
                        if(c->au_inuse[c->au_ridx]) {
                            if(send_aframe(c) == 0)
                                c->last_send = ms;
                        }
                    }
                    if(c->to_send > 1000) {
                        if(send_stream(c) == 0 || c->to_send < 500)
                            empty_map |= 1 << i;
                        else
                            empty_map &= ~(1 << i);
                    }
                    else
                        empty_map |= 1 << i;

                    client_check_queue(c);

                    if(time_check(c) < 0)
                        c->inuse = 0;
                }
            }
        }
        else {
            empty_map |= 1 << i;
            act_map &= ~(1 << i);
        }

        if(act_map == 0) {
            if((now - obs->turn_update_ts) > 5) {
                if(now != last_sent) {
                    turn_send_reset(sd, obs->session_base);
                    last_sent = now;
                }
            }
        }

        if((now - obs->turn_traffic_ts) > 10) {
            // TURN server timeout
            obs_dbg("TURN Server TIMEOUT\n");
            obs->state = OBSS_STATE_INIT;
            goto _turn_client_init_;
        }

        if(empty_map == C_MASK)
            jh_msleep(10);

        i = (i+1) % C_MAX;
    }

//_turn_client_exit_:
    if(sd >= 0)
        close(sd);
    if(rbuf)
        free(rbuf);
    obs_err("TURN exit\n");
    pthread_exit("TURN exit");
    return 0;
}

// --------------------------------------------------------------------------------------
// Server
// --------------------------------------------------------------------------------------
static void conn_update(obs_client_t *c, obst_conn_t *cr)
{
    int i;
    c->peer_id = cr->peer_id;
    for(i=0;i<OBST_CONN_NR;i++) {
        c->peer_ip[i] = cr->peer_ip[i];
        c->peer_port[i] = cr->peer_port[i];
        obs_dbg("[%d] update [%d] %x:%u\n", c->id, i, c->peer_ip[i], c->peer_port[i]);
    }
}

static int conn_resp(obs_server_t *obs, obst_conn_t *oc, u32 status)
{
    obst_cmd_t *scmd = (obst_cmd_t *)obs->sbuf;
    obst_conn_t *soc = (obst_conn_t *)scmd->param;
    memset(scmd, 0, sizeof(obst_cmd_t));
    scmd->magic = OBST_MAGIC;
    scmd->cmd = OBST_CONN_REQ;
    memcpy(soc, oc, sizeof(obst_conn_t));
    soc->status = status;

    struct sockaddr_in addr;
    addr.sin_family = PF_INET;
    addr.sin_port = htons(OBS_STUN_PORT);

    int s_size = sendto(obs->sd, obs->sbuf, sizeof(obst_cmd_t), 0, (struct sockaddr *)&obs->peer, sizeof(struct sockaddr_in));
    if(s_size < 0) {
        obs_err("sendto error: %s\n", strerror(errno));
        return -1;
    }
    return (s_size == sizeof(obst_cmd_t)) ? 0 : -1;
}

static int conn_op(obs_server_t *obs, obst_conn_t *oc)
{
    int i;
    if(oc->peer_id == OBST_CONN_ALL && (oc->param & OBST_CONN_ON) == 0) { // ALL OFF
        obs_dbg("[CONN] ALL OFF\n");
        for(i=0;i<C_MAX;i++) {
            obs_client_t *c = &obs_clients[i];
            if(c->inuse) {
                c->fin_code = OBSP_FIN_LEFT;
                c->inuse = 0;
            }
        }
        return 0;
    }

    int f = -1;
    for(i=0;i<C_MAX;i++) {
        obs_client_t *c = &obs_clients[i];
        if(!c->inuse) {
            if(f < 0)
                f = i;
        }
        else if(c->peer_id == oc->peer_id) {
            if(!(oc->param & OBST_CONN_ON)) { // turn off
                c->fin_code = OBSP_FIN_LEFT;
                c->inuse = 0;
                obs_dbg("[CONN]<%d> %u OFF\n", i, oc->peer_id);
            }
            else {
                obs_dbg("[CONN]<%d> %u ON (AGAIN) \n", i, oc->peer_id);
                conn_update(c, oc);
            }
            return 0;
        }
    }
    if(!(oc->param & OBST_CONN_ON)) { // turn off
        obs_dbg("[CONN] %u OFF (UNKNOWN CLIENT) \n", oc->peer_id);
        return 0;
    }

    if(f < 0) {
        obs_err("[CONN] FULL, ID:%u\n", oc->peer_id);
        return conn_resp(obs, oc, OBST_STATUS_CONN_FULL);
    }

    obs_client_t *c = &obs_clients[f];
    memset(c, 0, sizeof(obs_client_t));
    conn_update(c, oc);
    obs_dbg("[CONN]<%d> %u ON \n", f, oc->peer_id);
    c->id = f;
    c->d_buf = client_dbuf[f];
    do {
        c->session = random();
    } while(!c->session || c->session == OBSP_MAGIC);
    c->turn_session = obs->session_base + 1 + f;
    c->turn_mode = obs->using_turn;
    c->ch = 0xff;
    c->stream_id = 0xff;
    c->vtype = OBS_UNKNOWN;
    c->atype = OBS_UNKNOWN;
    c->inuse = 1;

    return 0;
}

static int stun_req(obs_server_t *obs)
{
    obst_cmd_t *cmd = (obst_cmd_t *)obs->sbuf;
    obst_stun_t *req = (obst_stun_t *)cmd->param;
    memset(cmd, 0, sizeof(obst_cmd_t));
    cmd->magic = OBST_MAGIC;
    cmd->cmd = OBST_STUN_REQ;
    memcpy(req->mac, my_mac, 6);

    struct sockaddr_in addr;
    addr.sin_family = PF_INET;

    int i;
    int sent = 0;
    for(i=0;i<ASC_STUN_NR;i++) {
        if(stun_config.server[i] && stun_config.type[i] == ASC_STUN_TYPE_OBS) {
            if(i == 1 && stun_ex_ip && stun_ex_port) {
                addr.sin_addr.s_addr = htonl(stun_ex_ip);
                addr.sin_port = htons(stun_ex_port);
            }
            else {
                addr.sin_addr.s_addr = htonl(stun_config.server[i]);
                addr.sin_port = htons(OBS_STUN_PORT);
            }
            if(sendto(obs->sd, obs->sbuf, sizeof(obst_cmd_t), 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) < 0) {
                obs_err("[STUN]<%d> send REQ to 0x%x error: %s\n", i, stun_config.server[i], strerror(errno));
                obs->stun_req_ts[i] = 0;
            }
            else {
                obs->stun_req_ts[i] = (u32)time(0);
                if(i == 1 && stun_ex_ip && stun_ex_port)
                    obs_dbg("[STUN]<%d> send REQ to 0x%x:%u @ %u\n", i, stun_ex_ip, stun_ex_port, obs->stun_req_ts[i]);
                else
                    obs_dbg("[STUN]<%d> send REQ to 0x%x @ %u\n", i, stun_config.server[i], obs->stun_req_ts[i]);
                sent++;
            }
        }
    }
    return (sent) ? 0 : -1;
}

static int stun_serv_req(obs_server_t *obs)
{
    if(!stun_config.server[0] || stun_config.type[0] != ASC_STUN_TYPE_OBS)
        return 0;

    obst_cmd_t *cmd = (obst_cmd_t *)obs->sbuf;
    memset(cmd, 0, sizeof(obst_cmd_t));
    cmd->magic = OBST_MAGIC;
    cmd->cmd = OBST_STUN_SERV;
    struct sockaddr_in addr;
    addr.sin_family = PF_INET;
    addr.sin_addr.s_addr = htonl(stun_config.server[0]);
    addr.sin_port = htons(OBS_STUN_PORT);
    if(sendto(obs->sd, obs->sbuf, sizeof(obst_cmd_t), 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) < 0) {
        obs_err("[STUN] send SERV REQ to 0x%x error: %s\n", stun_config.server[0], strerror(errno));
        return -1;
    }
    else {
        obs_dbg("[STUN] send SERV REQ to 0x%x\n", stun_config.server[0]);
        return 0;
    }
}

static int stun_resp(obs_server_t *obs, obst_stun_t *resp)
{
    u32 pip = ntohl(obs->peer.sin_addr.s_addr);
    int i;
    for(i=0;i<ASC_STUN_NR;i++) {
        if(stun_config.server[i] && stun_config.server[i] == pip) {
            obs->pub_ip[i] = resp->ip;
            obs->pub_port[i] = resp->port;
            obs->stun_resp_ts[i] = (u32)time(0);
            obs_dbg("[STUN]<%d> 0x%x:%u @ %u\n", i, resp->ip, resp->port, obs->stun_resp_ts[i]);
            break;
        }
    }
    return 0;
}

static int dev_reg(obs_server_t *obs)
{
    obst_cmd_t *cmd = (obst_cmd_t *)obs->sbuf;
    obst_dev_t *dev = (obst_dev_t *)cmd->param;
    memset(cmd, 0, sizeof(obst_cmd_t));
    cmd->magic = OBST_MAGIC;
    cmd->cmd = OBST_DEV_REG;
    memcpy(dev->mac, my_mac, 6);

    struct sockaddr_in addr;
    addr.sin_family = PF_INET;
    addr.sin_port = htons(OBS_STUN_PORT);

    int i;
    int sent = 0;
    for(i=0;i<2;i++) {
        if(obs->c_wip[i] && obs->c_wport[i] && obs->reg_serv[i] && (obs->reg_resp & (1 << i)) == 0) {
            addr.sin_addr.s_addr = htonl(obs->reg_serv[i]);
            dev->dev_id = OBST_DEV_VS1;
            dev->pub_ip = obs->c_wip[i];
            dev->pub_port = obs->c_wport[i];
            dev->attr[0] = fw_ver;
            if(sendto(obs->sd, obs->sbuf, sizeof(obst_cmd_t), 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) < 0) {
                obs_err("[STUN]<%d> DEV REG to 0x%x error: %s\n", i, obs->reg_serv[i], strerror(errno));
            }
            else {
                obs_dbg("[STUN]<%d> DEV REG %x:%u to 0x%x\n", i, dev->pub_ip, dev->pub_port, obs->reg_serv[i]);
                sent++;
            }
            obs->reg_req |= (1 << i);
        }
    }
    return (sent) ? 0 : -1;
}

static int dev_reg_resp(obs_server_t *obs, obst_dev_t *dev)
{
    u32 pip = ntohl(obs->peer.sin_addr.s_addr);
    int i;
    for(i=0;i<2;i++) {
        if(obs->reg_serv[i] == pip) {
            obs_dbg("[STUN]<%d> DEV RESP from 0x%x\n", i, pip);
            obs->reg_resp |= (1 << i);
            break;
        }
    }
    return 0;
}

static int get_turn(obs_server_t *obs)
{
    obst_cmd_t *cmd = (obst_cmd_t *)obs->sbuf;
    obst_turn_t *turn = (obst_turn_t *)cmd->param;
    memset(cmd, 0, sizeof(obst_cmd_t));
    cmd->magic = OBST_MAGIC;
    cmd->cmd = OBST_GET_TURN;
    turn->region_id = prefer_region_id; // TODO:how to get region id? by config ?

    struct sockaddr_in addr;
    addr.sin_family = PF_INET;
    addr.sin_addr.s_addr = htonl((stun_config.server[0]) ? stun_config.server[0] : stun_config.server[1]);
    addr.sin_port = htons(OBS_STUN_PORT);

    if(sendto(obs->sd, obs->sbuf, sizeof(obst_cmd_t), 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) < 0) {
        obs_err("[STUN] GET TURN error: %s\n", strerror(errno));
        return -1;
    }
    else {
        obs_dbg("[STUN] GET TURN \n");
        return 0;
    }
}

static int turn_reg(obs_server_t *obs)
{
    obst_cmd_t *cmd = (obst_cmd_t *)obs->sbuf;
    obst_turn_reg_t *tr = (obst_turn_reg_t *)cmd->param;
    memset(cmd, 0, sizeof(obst_cmd_t));
    cmd->magic = OBST_MAGIC;
    cmd->cmd = OBST_TURN_REG;
    tr->session = obs->session_base;

    struct sockaddr_in addr;
    addr.sin_family = PF_INET;
    addr.sin_addr.s_addr = htonl(obs->turn_ip);
    addr.sin_port = htons(obs->turn_port);

    if(sendto(obs->sd, obs->sbuf, sizeof(obst_cmd_t), 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) < 0) {
        obs_err("[STUN] TURN REG error: %s\n", strerror(errno));
        return -1;
    }
    else {
        obs_dbg("[STUN] TURN REG: %x:%u\n", tr->ip, tr->port);
    }

    return 0;
}

static int stun_check(obs_server_t *obs)
{
    if(stun_config.server[0] && stun_config.server[1]) {
        if(obs->pub_ip[0] && obs->pub_ip[1]) {
            if(obs->pub_ip[0] == obs->pub_ip[1]) {
                if(obs->pub_port[0] != obs->pub_port[1]) {
                    if(obs->stun_resp_ts[0] >= obs->stun_req_ts[0] && obs->stun_resp_ts[1] >= obs->stun_req_ts[1]) {
                        obs_err("[STUN] port mapping error: %u != %u\n", obs->pub_port[0], obs->pub_port[1]);
                        return -1;
                    }
                }
                else {
                    return 1; // matched
                }
            }
        }
    }
    else if(stun_config.server[0]) { // only one
        if(obs->pub_ip[0] && obs->pub_port[0])
            return 1;
    }
    else if(stun_config.server[2]) { // only local or vpn
        if(obs->pub_ip[2] && obs->pub_port[2])
            return 1;
    }
    else
        return -1;
    return 0;
}

static int obs_parser(obs_server_t *obs)
{
    obst_cmd_t *oc = (obst_cmd_t *)obs->rbuf;
    if(oc->magic == OBST_MAGIC) {
        if(oc->cmd == OBST_STUN_REQ) {
            stun_resp(obs, (obst_stun_t *)oc->param);
        }
        else if(oc->cmd == OBST_DEV_REG) {
            dev_reg_resp(obs, (obst_dev_t *)oc->param);
        }
        else if(oc->cmd == OBST_GET_TURN) {
            obst_turn_t *turn = (obst_turn_t *)oc->param;
            if(turn->ip && turn->port) {
                obs->turn_ip = turn->ip;
                obs->turn_port = turn->port;
                obs->turn_region_id = turn->region_id;
            }
        }
        else if(oc->cmd == OBST_TURN_REG) {
            obst_turn_reg_t *tr = (obst_turn_reg_t *)oc->param;
            if(obs->turn_wip != tr->ip || obs->turn_wport != tr->port || obs->turn_session != tr->session) {
                obs_dbg("[TURN] session change: 0x%x:%u(%x) -> 0x%x:%u(%x)\n",
                        obs->turn_wip, obs->turn_wport, obs->turn_session, tr->ip, tr->port, tr->session);
                obs->turn_session = tr->session;
                obs->turn_wport = tr->port;
                obs->turn_wip = tr->ip;
            }
        }
        else if(oc->cmd == OBST_CONN_REQ) {
            conn_op(obs, (obst_conn_t *)oc->param);
        }
        else if(oc->cmd == OBST_STUN_SERV) {
            obs_dbg("[STUN] got SERV RESP = 0x%x:%u\n", oc->param[0], oc->param[1]);
            if(oc->param[0] && oc->param[1]) {
                stun_ex_ip = oc->param[0];
                stun_ex_port = oc->param[1];
            }
        }
    }
    else {
        obsp_cmd_t *opc = (obsp_cmd_t *)obs->rbuf;
        if(opc->cmd >= OBSP_PEER_HS && opc->cmd <= OBSP_STAT_RECEIVER) {
            u32 ip = ntohl(obs->peer.sin_addr.s_addr);
            u16 port = ntohs(obs->peer.sin_port);
            obs_pkt("Serv got OBSP from %x:%u\n", ip, port);
            int i;
            for(i=0;i<C_MAX;i++) {
                obs_client_t *c = &obs_clients[i];
                if(c->inuse) {
                    int j;
                    for(j=0;j<OBST_CONN_NR;j++) {
                        if(c->peer_ip[j] && c->peer_port[j]) {
                            if(c->peer_ip[j] == ip && c->peer_port[j] == port) {
                                break;
                            }
                        }
                    }
                    if(j < OBST_CONN_NR) {
                        if(!c->ext_recv) {
                            obs_pkt("pass packet to C#%d IN\n", i);
                            memcpy(c->ext_rbuf, obs->rbuf, obs->r_size);
                            c->ext_ip = ip;
                            c->ext_port = port;
                            c->ext_recv = obs->r_size;
                            obs_pkt("pass packet to C#%d\n", i);
                        }
                        else {
                            obs_pkt("C#%d ext buf full\n", i);
                        }
                        break;
                    }
                }
            }
        }
    }

    return 0;
}

static inline void reset_wan(obs_server_t *obs)
{
    obs->c_wip[0] = obs->c_wip[1] = 0;
    obs->c_wport[0] = obs->c_wport[1] = 0;
}

static inline void get_wan(obs_server_t *obs)
{
    if(obs->using_turn) {
        obs->c_wip[0] = obs->turn_wip;
        obs->c_wip[1] = 0;
        obs->c_wport[0] = obs->turn_wport;
        obs->c_wport[1] = 0;
    }
    else {
        obs->c_wip[0] = obs->pub_ip[0];
        obs->c_wip[1] = obs->pub_ip[2];
        obs->c_wport[0] = obs->pub_port[0];
        obs->c_wport[1] = obs->pub_port[2];
    }
}

static inline void reset_stun(obs_server_t *obs)
{
    int i;
    for(i=0;i<ASC_STUN_NR;i++) {
        obs->pub_ip[i] = 0;
        obs->pub_port[i] = 0;
        obs->stun_resp_ts[i] = 0;
        obs->stun_req_ts[i] = 0;
    }
    obs->stun_status = 0;
}

static inline void reset_turn(obs_server_t *obs)
{
    obs->using_turn = 0;
    obs->turn_region_id = 0;
    obs->turn_ip = 0;
    obs->turn_port = 0;
    obs->turn_session = 0;
    obs->turn_wip = 0;
    obs->turn_wport = 0;
}

static inline int client_running(obs_server_t *obs)
{
    int i;
    for(i=0;i<C_MAX;i++) {
        if(obs_clients[i].inuse)
            return 1;
    }
    return 0;
}

static inline int client_stop(obs_server_t *obs)
{
    int n = 0;
    int i;
    for(i=0;i<C_MAX;i++) {
        if(obs_clients[i].inuse) {
            obs_clients[i].inuse = 0;
            n++;
        }
    }
    if(n)
        obs_dbg("Reset %d client(s)\n", n);
    return n;
}

static int server_recv(obs_server_t *obs, time_t sec, time_t usec)
{
    fd_set rds;
    struct timeval tv;
    FD_ZERO(&rds);
    FD_SET(obs->sd, &rds);
    tv.tv_sec = sec;
    tv.tv_usec = usec;

    int ret = select(obs->sd+1, &rds, 0, 0, &tv);
    if(ret < 0) {
        obs_err("select error: %s\n", strerror(errno));
        return -1;
    }
    else if(ret) {
        if(FD_ISSET(obs->sd, &rds)) {
            socklen_t addr_len = sizeof(obs->peer);
            obs->r_size = recvfrom(obs->sd, obs->rbuf, RBUF_SZ, 0, (struct sockaddr *)&obs->peer, &addr_len);
            if(obs->r_size < 0) {
                obs_err("recvfrom error:%s\n", strerror(errno));
                return -1;
            }
            if(obs->r_size >= 8)
                obs_parser(obs);
        }
    }
    return 0;
}

static void *obs_thread(void *arg)
{
    obs_dbg("Server thread start .....PID: %u\n", (u32)jh_gettid());

    obs_server_t *obs = (obs_server_t *)arg;
    obs_port = (u16)(random() % 55000) + 10000;
    obs->sd = -1;

_obs_server_init_:
    while(!stun_config.server[0]) {
        sleep(1);
    }
    if(obs->sd >= 0) {
        obs_err("close socket\n");
        close(obs->sd);
        obs->sd = -1;
    }
    obs_port++;
    if(obs_port < 10000 || obs_port > 65000)
        obs_port = 10000;
    u32 org_session;
    do {
        org_session = random();
    } while(org_session < 0x10000000 || org_session > 0xf0000000 || org_session == obs->session_base);
    obs->session_base = org_session;

    obs_dbg("start service on port %u, session_base %u\n", obs_port, obs->session_base);

    obs->sd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(obs->sd < 0) {
        obs_err("socket error: %s", strerror(errno));
        sleep(1);
        goto _obs_server_init_;
    }
    else {
        struct sockaddr_in  my_addr;
        my_addr.sin_family = PF_INET;
        my_addr.sin_port = htons(obs_port);
        my_addr.sin_addr.s_addr = INADDR_ANY;
        if(bind(obs->sd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) < 0) {
            obs_err("bind to %u error: %s", obs_port, strerror(errno));
            goto _obs_server_init_;
        }
        int val = 1;
        if(setsockopt(obs->sd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0) {
            obs_err("set SO_REUSEADDR error: %s", strerror(errno));
            goto _obs_server_init_;
        }
    }

    obs_dbg("server socket = %d\n", obs->sd);

    client_stop(obs);
    reset_wan(obs);
    reset_stun(obs);
    reset_turn(obs);
    obs->state = OBSS_STATE_STUN_REQ;
    obs->st_start = time(0);
    obs->op_start = 0;

    while(1) {
        if(server_recv(obs, 0, 500000) < 0)
            goto _obs_server_init_;

        time_t now = time(0);
        if(obs->state == OBSS_STATE_STUN_REQ) {
            obs->stun_status = stun_check(obs);
//#define FORCE_TURN
#ifdef FORCE_TURN
            if(obs->stun_status > 0) {
                obs->stun_status = -1;
            }
#endif
            if(obs->stun_status > 0) {
                obs->using_turn = 0;
                obs->state = OBSS_STATE_WIP_READY;
                obs->reg_req = obs->reg_resp = 0;
                get_wan(obs);
                obs->st_start = now;
                obs->op_start = 0;
                obs_dbg("STUN OK\n");
            }
            else if(obs->stun_status < 0) {
                reset_turn(obs);
                obs->using_turn = 1;
                obs->state = OBSS_STATE_TURN_INIT;
                obs->st_start = now;
                obs->op_start = 0;
                obs_dbg("STUN Failed, try TURN\n");
            }
            else {
                if((now - obs->st_start) < 10) {
                    if((now - obs->op_start) > 1) {
                        if(stun_req(obs) < 0)
                            goto _obs_server_init_;
                        obs->op_start = now;
                    }
                }
                else {
                    stun_serv_req(obs);
                    if((now - obs->op_start) >= 10) {
                        reset_stun(obs);
                        if(stun_req(obs) < 0)
                            goto _obs_server_init_;
                        obs->op_start = now;
                    }
                    else if(obs->pub_ip[0] || obs->pub_ip[1]) { // resend if got only one
                        if(!obs->pub_ip[1] && stun_ex_ip) {
                            obs_dbg("STUN EX Fail, reset\n");
                            stun_ex_ip = 0;
                            stun_ex_port = 0;
                        }
                        if(stun_config.server[0] && stun_config.server[1]) {
                            if((now - obs->op_start) > 2) {
                                if(stun_req(obs) < 0)
                                    goto _obs_server_init_;
                                obs->op_start = now;
                            }
                        }
                    }
                }
                continue;
            }
        }
        if(obs->state == OBSS_STATE_TURN_INIT) {
            if(obs->turn_ip && obs->turn_port) {
                obs->state = OBSS_STATE_TURN_REG;
                obs->st_start = now;
                obs->op_start = 0;
                obs_dbg("Got TURN Server @ 0x%x:%u (%u)\n", obs->turn_ip, obs->turn_port, obs->turn_region_id);
            }
            else {
                if((now - obs->st_start) > 10)
                    goto _obs_server_init_;
                else if((now - obs->op_start) > 1) {
                    if(get_turn(obs) < 0)
                        goto _obs_server_init_;
                    obs->op_start = now;
                }
                continue;
            }
        }
        if(obs->state == OBSS_STATE_TURN_REG) {
            if(obs->turn_wip && obs->turn_wport) {
                obs->state = OBSS_STATE_WIP_READY;
                obs->reg_req = obs->reg_resp = 0;
                get_wan(obs);
                obs->st_start = now;
                obs->op_start = 0;
                obs_dbg("TURN Server Ready @ 0x%x:%u\n", obs->turn_wip, obs->turn_wport);
            }
            else {
                if((now - obs->st_start) > 10)
                    goto _obs_server_init_;
                else if((now - obs->op_start) > 1) {
                    if(turn_reg(obs) < 0)
                        goto _obs_server_init_;
                    obs->op_start = now;
                }
                continue;
            }
        }
        if(obs->state == OBSS_STATE_WIP_READY) {
            if((now - obs->st_start) > 30) {
                if(!client_running(obs)) {
                    reset_stun(obs);
                    stun_serv_req(obs);
                    obs->state = OBSS_STATE_CHECK_WAN;
                    obs->st_start = now;
                    obs->op_start = 0;
                    obs_dbg("Check WAN Status\n");
                }
                else {
                    obs->st_start = now;
                }
                continue;
            }
            if((now - obs->op_start) > 10) {
                obs->reg_req = obs->reg_resp = 0;
                if(dev_reg(obs) < 0)
                    goto _obs_server_init_;
                obs->op_start = now;
            }
            else if(obs->reg_req != obs->reg_resp) {
                if((now - obs->op_start) > 1) {
                    if(dev_reg(obs) < 0)
                        goto _obs_server_init_;
                    obs->op_start = now;
                }
            }
            continue;
        }
        if(obs->state == OBSS_STATE_CHECK_WAN) {
            obs->stun_status = stun_check(obs);
#ifdef FORCE_TURN
            if(obs->stun_status > 0) {
                obs->stun_status = -1;
            }
#endif
            if(obs->stun_status > 0) {
                obs->state = OBSS_STATE_WIP_READY;
                obs->reg_req = obs->reg_resp = 0;
                obs->st_start = now;
                obs->op_start = 0;
                if(obs->pub_port[0] == obs->c_wport[0] && obs->pub_ip[0] == obs->c_wip[0] &&
                   obs->pub_port[2] == obs->c_wport[1] && obs->pub_ip[2] == obs->c_wip[1]) {
                    // no changed
                    continue;
                }
                else {
                    if(obs->using_turn && client_running(obs))
                        continue;
                    obs_dbg("Got new wan ip & port (STUN), TURN=%d\n", obs->using_turn);
                    obs->using_turn = 0;
                    client_stop(obs);
                    get_wan(obs);
                }
            }
            else if(obs->stun_status < 0) {
                if(obs->using_turn) {
                    obs->state = OBSS_STATE_WIP_READY;
                    obs->reg_req = obs->reg_resp = 0;
                    obs->st_start = now;
                    obs->op_start = 0;
                }
                else {
                    obs_dbg("Check STUN failed (STUN->TURN)\n");
#if 1
                    goto _obs_server_init_;
#else
                    client_stop(obs);
                    reset_turn(obs);
                    obs->state = OBSS_STATE_TURN_INIT;
                    obs->st_start = now;
                    obs->op_start = 0;
                    obs->using_turn = 1;
#endif
                }
            }
            else {
                if((now - obs->st_start) < 15) {
                    if((now - obs->op_start) > 1) {
                        if(stun_req(obs) < 0)
                            goto _obs_server_init_;
                        obs->op_start = now;
                    }
                }
                else {
                    obs_dbg("Check wan timeout, reset\n");
                    client_stop(obs);
                    stun_serv_req(obs);
                    goto _obs_server_init_;
                }
            }
            continue;
        }
        if(obs->state == OBSS_STATE_INIT) {
            goto _obs_server_init_;
        }
    }
//_obs_server_exit_:
    obs->state = -1;
    if(obs->sd >= 0)
        close(obs->sd);

    pthread_exit("OBS server thread exit");
    return 0;
}

// ----------------------------------------------------------------------------
// APIs
// ----------------------------------------------------------------------------
int obs_init(u8 *mac, u32 fver)
{
    fw_ver = fver;
    memcpy(my_mac, mac, 6);
    memset(&stun_config, 0, sizeof(asc_config_stun_t));
    memset(&obs_server, 0, sizeof(obs_server));
    jh_thread_create(obs_thread, &obs_server);
    memset(obs_clients, 0, sizeof(obs_clients));
    int i;
    for(i=0;i<C_MAX;i++) {
        client_dbuf[i] = (u8 *)malloc(DBUF_SZ);
        obs_clients[i].id = i;
        jh_thread_create(client_thread, &obs_clients[i]);
    }
    jh_thread_create(turn_thread, &obs_server);

    return 0;
}

void obs_stun_setup(asc_config_stun_t *stun)
{
    memcpy(&stun_config, stun, sizeof(asc_config_stun_t));
    obs_server.reg_serv[0] = (stun_config.type[0] == ASC_STUN_TYPE_OBS) ? stun_config.server[0] : 0;
    obs_server.reg_serv[1] = (stun_config.type[2] == ASC_STUN_TYPE_OBS) ? stun_config.server[2] : 0;
    stun_ex_ip = 0;
    stun_ex_port = 0;
}

void obs_stun_info(asc_config_stun_t *stun)
{
    memcpy(stun, &stun_config, sizeof(asc_config_stun_t));
}

void obs_set_dio(u32 value)
{
    c_dio = value;
}

void obs_set_attr(u8 ch, u8 sid, u8 type, u8 sub_type, u16 w, u16 h)
{
    assert(ch < CH_MAX);
    assert(sid < S_MAX);
    if(type == OBS_TYPE_VIDEO) {
        stream_attr[ch].vtype[sid] = sub_type;
        stream_attr[ch].width[sid] = w;
        stream_attr[ch].height[sid] = h;
    }
    else if(type == OBS_TYPE_AUDIO) {
        stream_attr[ch].atype = sub_type;
        stream_attr[ch].bitwidth = w;
        stream_attr[ch].samplerate = h;
    }
}

int obs_video_in(dm_data_t *data)
{
    int i;
    for(i=0;i<C_MAX;i++) {
        obs_client_t *c = &obs_clients[i];
        if(!c->inuse)
            continue;
        if(c->ch != data->channel || data->id != c->stream_id)
            continue;

        c->w_seq++;

        u32 next = (c->w_idx + 1) % F_MAX;
        if(next == c->a_idx) {
            c->v_skip = 1;
            continue;
        }

        if(c->v_skip) {
            if(data->is_key && c->w_idx == c->r_idx)
                c->v_skip = 0;
            else
                continue;
        }

        q_frame_t *f = &c->qf[c->w_idx];
        f->type = ASF_TYPE_VIDEO;
        f->sub_type = stream_attr[data->channel].vtype[data->id];
        f->iskey = data->is_key;
        f->flag = (u8)(c_dio & 0xff);
        f->w = data->width;
        f->h = data->height;
        f->seq = c->w_seq;
        f->sec = (u32)data->enc_time.tv_sec;
        f->usec = (u32)data->enc_time.tv_usec;
        f->pseq_start = f->pseq_end = 0;

        u32 fsize = 0;
        u32 j;
        for(j=0;j<data->pl_num;j++)
            fsize += data->payloads[j].iov_len;

        f->size = fsize;
        fsize = (fsize + 7) & ~7;
        f->fsize = fsize;
        f->roff = 0;
        if(c->w_off >= c->a_off) {
            if(fsize > (DBUF_SZ - c->w_off)) {
                if(fsize >= c->a_off) {
                    c->v_skip = 1;
                    continue;
                }
                f->ptr = c->d_buf;
                c->w_off = fsize;
            }
            else {
                f->ptr = c->d_buf + c->w_off;
                c->w_off += fsize;
            }
        }
        else {
            if(fsize > (c->a_off - c->w_off - 1)) {
                c->v_skip = 1;
                continue;
            }
            f->ptr = c->d_buf + c->w_off;
            c->w_off += fsize;
        }
        f->noff = c->w_off;
        u32 off = 0;
        for(j=0;j<data->pl_num;j++) {
            memcpy(f->ptr+off, data->payloads[j].iov_base, data->payloads[j].iov_len);
            off += data->payloads[j].iov_len;
        }

        c->w_idx = next;
    }
    return 0;
}

int obs_audio_in(dm_data_t *data)
{
    int i;
    for(i=0;i<C_MAX;i++) {
        obs_client_t *c = &obs_clients[i];
        if(!c->inuse || c->ch != data->channel)
            continue;

        if(!c->audio) {
            if(c->au_size && c->au_woff) {
                c->au_woff = 0;
                c->au_wnr = 0;
            }
            continue;
        }

        if(c->au_usize != data->payloads[0].iov_len) {
            obs_err("audio unit size changed %u -> %u\n", c->au_usize, data->payloads[0].iov_len);
            c->au_usize = c->au_size = c->au_wnr = c->au_woff = 0;
        }
        if(!c->au_usize) {
            c->au_usize = data->payloads[0].iov_len;
            c->au_nr = (PKT_DSZ / c->au_usize);
            if(c->au_nr > 5) c->au_nr = 5;
            c->au_size = c->au_nr * c->au_usize;
            obs_dbg("audio unit size %u, nr=%u, pkt size=%u\n", c->au_usize, c->au_nr, c->au_size);
        }

        if(data->pl_num > 1)
            obs_err("cannot handle audio packet number = %u\n", data->pl_num);

        jh_copy(c->au_buf[c->au_widx] + c->au_woff, data->payloads[0].iov_base, c->au_usize);
        if(++c->au_wnr >= c->au_nr) {
            c->au_woff = 0;
            c->au_wnr = 0;
            u32 next = (c->au_widx + 1) % AU_QNR;
            if(!c->au_inuse[next]) {
                c->au_inuse[c->au_widx] = 1;
                c->au_widx = next;
            }
            else {
                obs_err("Audio queue full @ wr=%u/%u\n", c->au_widx, c->au_ridx);
            }
        }
        else {
            c->au_woff += c->au_usize;
        }
    }
    return 0;
}

