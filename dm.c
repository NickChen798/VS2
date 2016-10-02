/*!
 *  @file       dm.c
 *  @version    0.01
 *  @author     James Chien <james@phototracq.com>
 */

#include "as_types.h"
#include "va_api.h"
#include "as_dm.h"
#include "as_headers.h"
#include "as_platform.h"
#include "as_generic.h"
#include "as_media.h"
#include "as_config.h"
dm_callback_t h264in=NULL;
dm_callback_t yuvin=NULL;

typedef int (*ProcessYUV)(unsigned char *buffer,unsigned long cnt);
typedef int (*ProcessH264)(unsigned char *buffer,unsigned long cnt);
// ----------------------------------------------------------------------------
// APIs
// ----------------------------------------------------------------------------
/*!
 * @brief   Initialization
 * @return  0 on success, otherwise failed
 */
unsigned char HFileBuffer[2*1024*1024];
int h264i[2]={0,0};
int yuvi[2]={0,0};
int FileExist(char *fname) {
    struct stat st;
    return(stat(fname,&st)==0);
}
unsigned char pop=0;
int BuildH264(int inx,dm_data_t *data) {
        int i;
        char fname[30];
        char fnameNext[30];
        FILE *fp;
	data->channel=0;
/*
        if(pop==0) {
           pop=1;
           
        } else {
           pop=0;
	   data->channel=1;
        }
*/
	data->id=0;
	data->type=1;
	data->sub_type=0;
	data->is_key=1;
	data->diff_time=0;
	data->width=1920;
        if(inx==0)
	    data->height=1080;
        else data->height=1088;
	data->samplerate=0;
	data->bitwidth=8;
	data->flags=0;
	data->title_len=0;
	data->pl_num=1;
        if(h264i[inx]==2) sprintf(fnameNext,"/dev/vi%d_%02d.264",inx,0);
        else sprintf(fnameNext,"/dev/vi%d_%02d.264",inx,h264i[inx]+1);
        if(FileExist(fnameNext)==0) return 0;
        sprintf(fname,"/dev/vi%d_%02d.264",inx,h264i[inx]);
        if(FileExist(fname)==0) return 0;
        fp=fopen(fname,"rb");
        if(fp==NULL) return 0;
        data->payloads[0].iov_len=fread(HFileBuffer,1,2*1024*1024,fp);
        fclose(fp);
        data->payloads[0].iov_base=HFileBuffer;
        sprintf(fname,"rm /dev/vi%d_%02d.264",inx,h264i[i]);
        system(fname);
        if(h264i[inx] == 2 ) h264i[inx]=0;
        else h264i[inx]++;
        return 1;
}
int BuildYUV(int inx,dm_data_t *data) {
        int i;
        char fname[30];
        char fnameNext[30];
        FILE *fp;
	data->channel=0;
	data->id=3;
	data->type=1;
	data->sub_type=64;
	data->is_key=1;
	data->diff_time=0;
	data->width=1920;
	data->height=1080;
	data->samplerate=inx;
	data->bitwidth=8;
	data->flags=0;
	data->title_len=0;
	data->pl_num=1;

        if(yuvi[inx]==2) sprintf(fnameNext,"/dev/vi%d_%02d.yuv",inx,0);
        else sprintf(fnameNext,"/dev/vi%d_%02d.yuv",inx,yuvi[inx]+1);
        if(FileExist(fnameNext)==0) return 0;
        sprintf(fname,"/dev/vi%d_%02d.yuv",inx,yuvi[inx]);
        if(FileExist(fname)==0) return 0;
        fp=fopen(fname,"rb");
        if(fp==NULL) return 0;
        data->payloads[0].iov_len=fread(HFileBuffer,1,2*1024*1024,fp);
        fclose(fp);
        data->payloads[0].iov_base=HFileBuffer;
        sprintf(fname,"rm /dev/vi%d_%02d.yuv",inx,yuvi[inx]);
        system(fname);
        if(yuvi[inx] == 2 ) yuvi[inx]=0;
        else yuvi[inx]++;
        return 1;
}
void RunMain();
static void *h264in_thread(void *arg)
{
    int i;
    dm_data_t data;
    as_gen_set_video_channel(0);
    //system("/root/vimg 2 &");
    //RunMain();
    while(1) {
        usleep(50000); //200ms
        if(h264in!=NULL) {
           if(BuildH264(0,&data)==1) {
              h264in(&data);  
           }
/*
           if(BuildH264(1,&data)==1) {
              h264in(&data);  
           }
*/
        }

        if(yuvin!=NULL) {
           if(BuildYUV(0,&data)==1) {
              yuvin(&data);  
           }
/*
           if(BuildYUV(1,&data)==1) {
              yuvin(&data);  
           }
*/
        }

    }
}
/*
static void *yuvin_thread(void *arg)
{
    while(1) {
        sleep(1);
    }
}
*/
// ----------------------------------------------------------------------------
// Event Input APIs
// ----------------------------------------------------------------------------
/*!
 * @brief   alarm state
 * @param[in]   id      alarm id
 * @param[in]   status  status: b0=on/off, b7=locked
 * @param[in]   trigger trigger map
 * @return  0 on success, otherwise failed
 */
int dm_alarm(u8 id, u8 status, u32 trigger) {
}

/*!
 * @brief   motion state
 * @param[in]   id      motion id
 * @param[in]   status  status: b0=on/off, b7=locked
 * @param[in]   trigger trigger map
 * @return  0 on success, otherwise failed
 */
int dm_motion(u8 id, u8 status, u32 trigger) {
}

/*!
 * @brief   vloss state
 * @param[in]   id      vloss id
 * @param[in]   status  status: b0=on/off, b7=locked
 * @param[in]   trigger trigger map
 * @return  0 on success, otherwise failed
 */
int dm_vloss(u8 id, u8 status, u32 trigger) {
}

/*!
 * @brief   event state
 * @param[in]   id      event id
 * @param[in]   status  status: b0=on/off, b7=locked
 * @param[in]   trigger trigger map
 * @return  0 on success, otherwise failed
 */
int dm_event(u8 id, u8 status, u32 trigger) {
}

/*!
 * @brief   camera flags
 * @param[in]   id      camera id
 * @param[in]   flags   b3:event, b2:vloss, b1:motion, b0:alarm
 * @return  0 on success, otherwise failed
 */
int dm_camera_flags(u8 id, u8 flags) {
}

// ----------------------------------------------------------------------------
// Data Input APIs
// ----------------------------------------------------------------------------
/*!
 * @brief   Data entry
 * @param[in]   data        data attributes
 * @return  0 on success, otherwise failed
 */
int dm_data_in(dm_data_t *data) {
}

/*!
 * @brief   Motion Data entry
 * @param[in]   data        data attributes
 * @return  0 on success, otherwise failed
 */
int dm_md_in(dm_data_t *data) {
}

/*!
 * @brief   register a live stream callback
 * @param[in]   cb      callback function
 * @return  0 on success, otherwise failed
 */
int dm_reg_live_cb(dm_callback_t cb) {
}

/*!
 * @brief   Register a live stream callback for AS Stream
 * @param[in]   cb      callback function
 * @return  0 on success, otherwise failed
 */
int vimg_reg_yuv_in(ProcessYUV py); //
int vimg_reg_h264_in(ProcessH264 ph);
int vimg_reg_yuv_in2(ProcessYUV py); //
int vimg_reg_h264_in2(ProcessH264 ph);
void vimg_SetCh(unsigned char Ch);
dm_data_t yuvdata;
dm_data_t h264data;
dm_data_t yuvdata2;
dm_data_t h264data2;
int vimg_SetEnc(unsigned short w,unsigned short h,unsigned char fps,unsigned short kbps);
unsigned short vimg_genWidth();
unsigned short vimg_genHeight();
////
int dm_SetEnc(unsigned char stream,unsigned short w,unsigned short h,unsigned char fps,unsigned short kbps) {
        if(stream==0) {
          printf("set enc: res:%dx%d,%d , %d\r\n",w,h,fps, kbps); 
          vimg_SetEnc(w,h,fps,kbps);
        }
}
void vimg_SetYUV_Res(int res);
int uYUVres=1; //0:960 1:1080
void dm_SetYUV_Res(int res) {
    vimg_SetYUV_Res(res);
    uYUVres=res;
    
}
int dm_h264_in(unsigned char *buffer,unsigned long cnt) {
  //      printf("dm h264 Size:%ld\r\n",cnt);
	dm_data_t *data=&h264data;
	data->channel=0;
	data->id=0;
	data->type=1;
	data->sub_type=0;
	data->is_key=1;
	data->diff_time=0;
	data->width=vimg_genWidth();
	data->height=vimg_genHeight();
	data->samplerate=0;
	data->bitwidth=8;
	data->flags=0;
	data->title_len=0;
	data->pl_num=1;
        data->payloads[0].iov_len=cnt;
        data->payloads[0].iov_base=buffer;
        if(h264in!=NULL) h264in(&h264data);
   //     vimg_SetCh(1);
        return 1;
}
int dm_h264_in2(unsigned char *buffer,unsigned long cnt) {
  //      printf("dm h264 Size:%ld\r\n",cnt);
	dm_data_t *data=&h264data;
	data->channel=0;
	data->id=0;
	data->type=1;
	data->sub_type=0;
	data->is_key=1;
	data->diff_time=0;
	data->width=vimg_genWidth();
	data->height=vimg_genHeight();
	data->samplerate=0;
	data->bitwidth=8;
	data->flags=0;
	data->title_len=0;
	data->pl_num=1;
    data->payloads[0].iov_len=cnt;
    data->payloads[0].iov_base=buffer;
    if(h264in!=NULL) h264in(&h264data);
    return 1;
}
int dm_yuv_in(unsigned char *buffer,unsigned long cnt) {
        printf("dm yuv Size:%ld\r\n",cnt);
        dm_data_t *data=&yuvdata;
	data->channel=0;
	data->id=3;
	data->type=1;
	data->sub_type=64;
	data->is_key=1;
	data->diff_time=0;
	data->width=1920;
	data->height=1088;	
	/*
	if(uYUVres==0) {
	       data->width=960;
           data->height=540;
	} else {
		   data->width=1920;
	   	   data->height=1088;
	}
	*/
	/*
    if(vimg_genWidth()!=1920) {
           data->width=960;
           data->height=540;
        } else {
	   data->width=1920;
	   data->height=1088;
	}
	*/
	data->samplerate=0;
	data->bitwidth=8;
	data->flags=0;
	data->title_len=0;
	data->pl_num=1;
        data->payloads[0].iov_len=cnt;
        data->payloads[0].iov_base=buffer;
        if(yuvin!=NULL) yuvin(&yuvdata);
        return 1;
}
int dm_yuv_in2(unsigned char *buffer,unsigned long cnt) {
        printf("dm yuv Size:%ld\r\n",cnt);
        dm_data_t *data=&yuvdata;
	data->channel=0;
	data->id=3;
	data->type=1;
	data->sub_type=64;
	data->is_key=1;
	data->diff_time=0;
	data->width=960;
    data->height=540;
	/*
	if(uYUVres==0) {
	       data->width=960;
           data->height=540;
	} else {
		   data->width=1920;
	   	   data->height=1088;
	}
	*/
	/*
        if(vimg_genWidth()!=1920) {
           data->width=960;
           data->height=540;
        } else {
	   data->width=1920;
	   data->height=1088;
	}
	*/
	data->samplerate=0;
	data->bitwidth=8;
	data->flags=0;
	data->title_len=0;
	data->pl_num=1;
        data->payloads[0].iov_len=cnt;
        data->payloads[0].iov_base=buffer;
        if(yuvin!=NULL) yuvin(&yuvdata);
        return 1;
}
int dm_reg_aslive_cb(dm_callback_t cb) {
    h264in=cb;
    printf("vimg_reg_h264_in\r\n");
    vimg_reg_h264_in(&dm_h264_in);
    vimg_reg_h264_in2(&dm_h264_in2);
    as_gen_set_video_channel(0);
    //dm_SetEnc(0,704,480,1,64);
    //RunMain();
    
   // create_norm_thread(h264in_thread, 0);
}

/*!
 * @brief   Register a live yuv stream callback
 * @param[in]   cb      callback function
 * @return  0 on success, otherwise failed
 */

int dm_reg_yuv_cb(dm_callback_t cb) {
    yuvin=cb;
    printf("vimg_reg_yuv_in\r\n");
    vimg_reg_yuv_in(&dm_yuv_in);
    vimg_reg_yuv_in2(&dm_yuv_in2);
	RunMain();
}
int dm_SetCh(unsigned char mode) {
    vimg_SetCh(mode);
}
// ----------------------------------------------------------------------------
// Init APIs
// ----------------------------------------------------------------------------
/*!
 * @brief   Data Manager Input initialization
 * @param[in]   ch      max channel
 * @param[in]   stream  max stream per channel
 * @return  0 on success, otherwise failed
 */
int dm_init(u32 ch, u32 stream) {
   printf("dm_init ch:%ld,stream:%ld\r\n",ch,stream);

}





