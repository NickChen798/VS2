/*!
 *  @file       as_config.c
 *  @version    1.0
 *  @date       01/11/2013
 *  @author     Jacky Hsu <jacky.hsu@altasec.com>
 *  @note       VS1 config
 *              Copyright (C) 2013 ALTASEC Corp.
 */

#include "as_headers.h"
#include "as_platform.h"
#include "as_generic.h"
#include "as_config.h"

// ----------------------------------------------------------------------------
// Debug
// ----------------------------------------------------------------------------
#define cfg_err(format,...)  printf("[CFG]%s(%d): " format, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define cfg_msg(format,...)  printf("[CFG] " format, ##__VA_ARGS__)

// ----------------------------------------------------------------------------
// Type Define
// ----------------------------------------------------------------------------
#define ASCFG_MAGIC     0x41436667  // 'ACfg'
#define ASCFG_VERSION   0x20130113
#define ASCFG_START     (sizeof(as_cfg_header_t))
#define ASCFG_SIZE      0x20000

typedef struct {

    u32     magic;
    u32     version;
    u32     resv[2];

}as_cfg_header_t;

#define ASCFG_MAXID     32
#define ASCFG_UNIT_MAX  4092
typedef struct {

    u16     size;       // total size include id (multiple of 8 bytes)
    u16     dsize;      // actual data size
    u32     id;
    u8      data[ASCFG_UNIT_MAX];

}as_cfg_item_t;

// ----------------------------------------------------------------------------
// Config ID
// ----------------------------------------------------------------------------
                        // id(8) size(16) check(8), check = id + sz_h + sz_l
//#define ASCFG_IPADDR    0x01000102
//#define ASCFG_IPCONFIG  0x02000204

// ----------------------------------------------------------------------------
// Local variable
// ----------------------------------------------------------------------------
static int cfg_init = 0;
static u32 cfg_offset = 0;

static as_cfg_item_t *cfg_items = 0;

// ----------------------------------------------------------------------------
// Local Functions
// ----------------------------------------------------------------------------
static int cfg_make_id(u8 cid, u16 dsize, u32 *id, u16 *size)
{
    if(cid >= ASCFG_MAXID || dsize > ASCFG_UNIT_MAX)
        return -1;
    u32 usz = ((dsize + 4 + 7) & ~7);
    *size = usz;
    u8 chk = (dsize >> 8) + (dsize & 0xff) + cid;
    *id = ((((u32)cid) << 24) | ((u32)dsize << 8) | chk);
    return 0;
}

static int cfg_check_id(u32 id, u8 *cid, u16 *size, u16 *dsize)
{
    if(!id)
        return -1;
    u8 nid = (id >> 24) & 0xff;
    if(nid >= ASCFG_MAXID)
        return -1;
    u32 ns = (id >> 8) & 0xffff;
    if(!ns)
        return -1;
    u8 chk = (ns >> 8) + (ns & 0xff) + nid;
    if(chk != (id & 0xff))
        return -1;
    *cid = nid;
    *dsize = ((u16)((ns + 4 + 7) & ~7)) - 4;
    *size = (u16)ns;

    if(*dsize > ASCFG_UNIT_MAX)
        return -1;
    return 0;
}

static int cfg_reset(void)
{
    as_cfg_header_t header;

    cfg_offset = 0;

    if(as_gen_cfg_erase() < 0) {
        cfg_err("Erase CFG error\n");
        return -1;
    }

    header.magic = ASCFG_MAGIC;
    header.version = ASCFG_VERSION;
    header.resv[0] = header.resv[1] = 0;

    if(as_gen_cfg_write((u8 *)&header, sizeof(as_cfg_header_t), 0) < 0) {
        cfg_err("Write CFG header error\n");
        return -1;
    }

    cfg_offset = ASCFG_START;

    return 0;
}

static int cfg_update_all(int reset);

static int cfg_update_item(u16 cid)
{
    if(cfg_offset == 0 || cid >= ASCFG_MAXID)
        return -1;
    as_cfg_item_t *cfg = &cfg_items[cid];
    if(!cfg->id || !cfg->size)
        return -1;
    if((cfg_offset + cfg->size) > ASCFG_SIZE)
        return cfg_update_all(1);

    if(as_gen_cfg_write((u8 *)(&cfg->id), cfg->size, cfg_offset) < 0) {
        cfg_err("Write CFG %d error @ 0x%x\n", cid, cfg_offset);
        return 1; // retry on next offset
    }
    cfg_msg("Update %u(0x%x) @ 0x%x\n", cid, cfg->size, cfg_offset);
    cfg_offset += cfg->size;

    return 0;
}

static int cfg_update_all(int reset)
{
    if(reset) {
        if(cfg_reset() < 0)
            return -1;
    }

    int i;
    for(i=0;i<ASCFG_MAXID;i++) {
        if(cfg_update_item(i) > 0) {
            if(cfg_update_item(i) != 0) {
                cfg_err("write %d error\n", i);
            }
        }
    }

    cfg_msg("Update All, next @ 0x%x\n", cfg_offset);
    return 0;
}

// ----------------------------------------------------------------------------
// APIs
// ----------------------------------------------------------------------------
/*!
 * @brief       init config
 * @return      0 on success, otherwise < 0
 */

int as_cfg_init(void)
{
    int i;
    FILE *fp;
    cfg_items = (as_cfg_item_t *)as_malloc(sizeof(as_cfg_item_t) * ASCFG_MAXID);
    assert(cfg_items);
    memset(cfg_items, 0, sizeof(as_cfg_item_t)*ASCFG_MAXID);

    as_cfg_header_t header;
    /*
    //Initial 
    fp=fopen("/root/ascfg.dat","rb");
    if(fp > 0) {
       fread(&header,sizeof(as_cfg_header_t),1,fp);
       fread(cfg_items,sizeof(as_cfg_item_t)*ASCFG_MAXID,1,fp);
      
       fclose(fp);
    } else {
       for(i=0;i<ASCFG_MAXID;i++) cfg_items[i].id=0xffffffff;
       fp=fopen("/root/ascfg.dat","wb");
       fwrite(&header,sizeof(as_cfg_header_t),1,fp);
       fwrite(cfg_items,sizeof(as_cfg_item_t)*ASCFG_MAXID,1,fp);
       fclose(fp);
       cfg_update_all(1);
    }
    printf("James as_cfg_init(); \r\n");
    */
    if(as_gen_cfg_read((u8 *)&header, sizeof(as_cfg_header_t), 0) < 0 ||
       header.magic != ASCFG_MAGIC || header.version != ASCFG_VERSION) {

        cfg_err("Header error : %x, %x\n", header.magic, header.version);

        if(cfg_reset() < 0)
            return -1;
    }

    cfg_offset = ASCFG_START;

    u32 id;
    u32 valid = 0;
    u8 *tbuf = as_malloc(ASCFG_UNIT_MAX);
    assert(tbuf);
    int wc = 0;
    u8 cid;
    u16 dsz, asz;

    do {
        if(as_gen_cfg_read((u8 *)&id, 4, cfg_offset) < 0) {
            cfg_err("Read CFG error @ 0x%x\n", cfg_offset);
            if(cfg_reset() < 0)
                break;
            if(valid)
                cfg_update_all(1);
            break;
        }

        //cfg_msg("ID = %08x @ %08x\n", id, cfg_offset);

        if(id == 0xffffffff) // start of empty data
            break;
        cfg_offset += 4;

        if(cfg_check_id(id, &cid, &asz, &dsz) < 0) {
            cfg_err("Wrong item: id=0x%x @ 0x%08x\n", id, cfg_offset-4);
            if(wc++ > 50) {
                if(cfg_reset() < 0)
                    break;
            }
            continue;
        }

        cfg_msg("CID = %02x, asz=%u, dsz=%u\n", cid, asz, dsz);

        if((cfg_offset + dsz) > ASCFG_SIZE) {
            cfg_err("Wrong size : id#%u (%u) @ 0x%x\n", cid, dsz, cfg_offset);
            if(wc++ > 50) {
                if(cfg_reset() < 0)
                    break;
            }
            continue;
        }

        as_cfg_item_t *cfg = &cfg_items[cid];

        if(as_gen_cfg_read(tbuf, dsz, cfg_offset) < 0) {
            cfg_err("Read CFG#%u(%u) Data error @ 0x%x\n", cid, dsz, cfg_offset);
        }
        else {
            memcpy(cfg->data, tbuf, dsz);
            cfg->dsize = dsz;
            cfg->size = asz;
            cfg->id = id;
            valid++;
        }
        cfg_offset += dsz;

    } while(cfg_offset < ASCFG_SIZE);

    free(tbuf);

    cfg_msg("Valid %u, Next offset = 0x%x\n", valid, cfg_offset);

    cfg_init = 1;

    return 0;
}

/*!
 * @brief       Get config data
 * @param[in]   cid     config id
 * @param[in]   dsize   config size
 * @param[out]  data    config data
 * @return      0 on success, otherwise < 0
 */
int as_cfg_get(u8 cid, u16 dsize, u8 *data)
{
    //printf("as_cfg_get cid:%d,dsize:%d\r\n",cid,dsize);
    // James_20160311
    if(cid==ASCFG_ENCODER1) {
       as_gen_cfg_encoder1_read(data,dsize,0);
       return 0;
    }
    memset(data, 0, dsize);
    if(!cfg_init || cid >= ASCFG_MAXID)
        return -1;

    as_cfg_item_t *cfg = &cfg_items[cid];
    if(!cfg->id || dsize > cfg->dsize)
        return -1;
    memcpy(data, cfg->data, dsize);
    return 0;
}

/*!
 * @brief       Set config data
 * @param[in]   cid     config id
 * @param[in]   dsize   config size
 * @param[in]   data    config data
 * @return      0 on success, otherwise < 0
 */
int as_cfg_set(u8 cid, u16 dsize, u8 *data)
{
    printf("as_cfg_set cid:%d,dsize:%d,data:%d\r\n",cid,dsize,data);
    // James_20160311
    if(cid==ASCFG_ENCODER1) {
    	printf("%d\n",as_gen_cfg_encoder1_write(data,dsize,0));    
       return 0;
    }
    if(!cfg_init)
        return -1;

    u32 id;
    u16 asize;
    if(cfg_make_id(cid, dsize, &id, &asize) < 0) {
        cfg_err("Wrong data: cid#%u, dsize=%u\n", cid, dsize);
        return -1;
    }

    cfg_msg("cid#%u, dsize=%u =>id=%08x, asz=%u\n", cid, dsize, id, asize);

    as_cfg_item_t *cfg = &cfg_items[cid];
    cfg->size = asize;
    cfg->dsize = dsize;
    cfg->id = id;
    memcpy(cfg->data, data, dsize);

    if(cfg_update_item(cid) > 0) {
        if(cfg_update_item(cid) != 0) {
            cfg_err("update cid#%u, dsize=%u error\n", cid, dsize);
            return -1;
        }
    }

    return 0;
}

