/*!
 *  @file       va_ipc.c
 *  @version
 *  @date
 *  @author
 *  @note
 *
 */

// ----------------------------------------------------------------------------
// Include Files
// ----------------------------------------------------------------------------
#include "as_headers.h"
#include "as_platform.h"
#include "as_protocol.h"
#include "vs1_api.h"
#include "va_api.h"
#include "as_dm2.h"
#include "as_generic.h"

#include "hi_common.h"
#include "hi_comm_video.h"
#include "hi_comm_sys.h"
#include "hi_comm_vi.h"
#include "hi_comm_venc.h"
#include "hi_comm_aenc.h"
#include "hi_comm_adec.h"
#include "hi_comm_ai.h"
#include "hi_comm_ao.h"
#include "hi_comm_md.h"
#include "mpi_vb.h"
#include "mpi_sys.h"
#include "mpi_vi.h"
#include "mpi_vpp.h"
#include "mpi_venc.h"
#include "mpi_ai.h"
#include "mpi_ao.h"
#include "mpi_aenc.h"
#include "mpi_adec.h"
#include "mpi_md.h"
#include "hi_type.h"
#include "loadbmp.h"
#include "tde_interface.h"
#include "misc_api.h"

#include "OBWinDef.h"
/*#include "OBErrMsg.h"
#include "OBImgPrc.h"*/
#include "CarDetect.h"
#include <dirent.h> 
#include <stdio.h>


// ----------------------------------------------------------------------------
// Debug
//#define WRITE2USB
//#define WRITE2FILE
//#define TEMP_BUF 
// ----------------------------------------------------------------------------
#define va_err(format,...)  printf("[VA]%s(%d): " format, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define va_dbg(format,...)  printf("[VA] " format, ##__VA_ARGS__)

#define WDT_MARGIN_TIME		60
#define DETECT_1920
#define ROI_1
// ----------------------------------------------------------------------------
// Type Definition
// ----------------------------------------------------------------------------
typedef struct {

    int top;
    int left;
    int bottom;
    int right;

} va_rect_t;

#define ROI_MAX 4
typedef struct {

    int width;
    int height;
    int number;
    va_rect_t roi[ROI_MAX];

} cd_setup_t;

// ----------------------------------------------------------------------------
// Locals
// ----------------------------------------------------------------------------
#define RBUF_SZ     1024
static cd_setup_t  cds_curr;
static cd_setup_t  cds_new;
static int cds_setup4 = 0;
static int cds_setup3 = 0;
static int cds_setup2 = 0;
static int cds_setup = 0;

#define ETH_NAME	"eth0"
#define BSIZE (1920*1080)
static VB_POOL pool = VB_INVALID_POOLID;

typedef struct
{
    int inuse;
    VB_BLK blk;
    u32 phy;
    u8 *virt;
    u32 size;
	u32 samplerate;
    u16 width;
    u16 height;
    time_t sec;
    time_t usec;
}buffer_detail;

typedef struct
{
	int cd_idx ;
	int w_idx ;
	buffer_detail	buffers[CD_GROUP_NR];
	int temp_inuse ;
}thread_buffer;

typedef struct
{
	int ch ;
	int c_ip ;
	int status ;
}l_status;

static thread_buffer buffer_info[ROI_MAX];
static int roi_width = 0;
static int roi_height = 0;


#define msleep(MS) do {                             \
struct timespec ms_delay;                       \
	ms_delay.tv_sec = ((MS)>=1000) ? (MS)/1000 : 0; \
	ms_delay.tv_nsec = ((MS)%1000) * 1000000;       \
	nanosleep(&ms_delay, NULL);                     \
} while(0)

#define YUV_SIZE        0x180000
static void *img_buffer1_1920=0;
static void *img_buffer1_960=0;
static void *img_buffer2_1920=0;
static void *img_buffer2_960=0;
static void *temp_buffer=0;
//static char *yuv_buf = 0;
static int yuv_size = 0;
static int yuv_width = 0;
static int yuv_height = 0;
static int yuv_inuse = 0;
static int yuv_enable = 0;
static int flag =0;
static int mflag =0;
static int gflag =0;
static int hour =24;
static int total_roi = 0;
static int detect_roi[4];
static int src1_num = 0;
static int src2_num = 0;
int frame_count = 0;
int yuv_count = 0;
int mode = 0;
int socket_type = 0;
int server_ip = 0;
int led_status = 0;
int led_lights = 0;
int led_flag = 0;

pthread_mutex_t mutex,mutex1; 

//int confidence = 0;
// ----------------------------------------------------------------------------
#define ROI_NR  2
#define SAVE_MAX 10
#define CMD_SIZE 2048
static uart_attr_t uac;

static struct {//son_wave=4;son_id=3
	int enable;
	
	int ROI_L;
	int ROI_T;
	int ROI_R;
	int ROI_B;
	int center_x;
	int center_y;

	int ROI_flag_left;
	int ROI_low_gray;
	int ROI_high_gray;

	int detect;
}usr_data[ROI_MAX];

#define DETECT_CONFIDENCE 2
static int tmp_state[ROI_MAX];
static int state_count[ROI_MAX];
static l_status	led_s[SAVE_MAX];

//static usr_data_info usr_data[ROI_MAX];
// ----------------------------------------------------------------------------
//1920 YUV to 960 YUV
void Scale960YUV(HI_CHAR *p) {
	HI_CHAR *dst,*src;
	unsigned int i,j;
	src=p;
	dst=p;
	for(i=0;i<540;i++) {
		for(j=0;j<1920/2;j++) {
			dst[j]=src[j*2];
		}
		dst+=960;
		src+=1920;
		src+=1920;
	}
}
// ----------------------------------------------------------------------------
//外掛USB寫LOG
void *serch_thread(void *arg)
{
	struct timeval tt;
	struct tm *now;
	char	time_buffer[128];
	char	r_time[16];
	char	load_time[32];
	char	day[16];
	char	path[16];
	int		hour,reboottime;
	int		sflag = 0;
	int		ret;

	gettimeofday(&tt, NULL);
	now = localtime(&tt.tv_sec);
	strftime(r_time,80,"%H",now);
	strftime(time_buffer,80,"%Y%m%d",now);
	sprintf(path,"/dbg/%s%02d",time_buffer,atoi(r_time));
	if((ret=mkdir(path,S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH))!=0)
	{
		printf("mkdir /dbg fail\n");
	}
	while (1)
	{	
		gettimeofday(&tt, NULL);
		now = localtime(&tt.tv_sec);
		strftime(r_time,80,"%H",now);
		hour = atoi(r_time);		
		if (hour == 0 && sflag ==1)
		{ 
			system("reboot");
		}
		else
		{
			strftime(r_time,80,"%H",now);
			strftime(day,80,"%d",now);
			strftime(time_buffer,80,"%Y%m%d",now);
			strftime(load_time,80,"%Y%m",now);
			if (r_time == 23)
			{
				sprintf(path,"/dbg/%s%02d%02d",load_time,(atoi(day)+1),(atoi(r_time)-23));
				if((ret=mkdir(path,S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH))!=0)
				{
					printf("mkdir /dbg fail\n");
				}
			}
			else
			{
				sprintf(path,"/dbg/%s%02d",time_buffer,(atoi(r_time)+1));
				if((ret=mkdir(path,S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH))!=0)
				{
					printf("mkdir /dbg fail\n");
				}
			}

			sleep(1);

		}
		sprintf(path,"rm -rf /dbg/%s%02d",time_buffer,(atoi(r_time)-1));
		system(path);
		sleep(3600);
		sflag = 1;
	}
	return 0 ;
}
// ----------------------------------------------------------------------------
//獲得IP
static int get_ip(const char *interface, u32 *ifaddr)
{
	struct ifreq ifr;
	strcpy(ifr.ifr_name, interface);
	*ifaddr = 0;

	int sd = socket(AF_INET, SOCK_STREAM, 0);
	if(sd < 0) {
		printf("[SCK] Open INET socket error\n");
		return -1;
	}

	if(ioctl(sd, SIOCGIFADDR, &ifr) < 0) {
		printf("[SCK] Could not get IF address for %s\n", ifr.ifr_name);
		close(sd);
		return -1;
	}
	*ifaddr = ntohl(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr);

	printf("[SCK] %s IP is 0x%x , %d\n", interface, *ifaddr, *ifaddr);

	close(sd);
	return 0;
}
// ----------------------------------------------------------------------------
//紀錄燈號
int write_led_lights(int lights)
{
	char out_msg[128];
	int i;
	printf("[SCK] Enter write_led_lights\n");
	FILE *fp;
	fp=fopen("led_light.txt","w+");
	if (fp)
	{
		sprintf(out_msg,"%d\n",lights);
		fwrite(out_msg,strlen(out_msg),1,fp);
		fclose(fp);
	}
	return 0;
}
// ----------------------------------------------------------------------------
//輸出LOG
int write_led_log(void)
{
	char out_msg[128];
	int i;
	printf("[SCK] Enter write_led_log\n");
	FILE *fp;
	fp=fopen("led.log","w+");
	if (fp)
	{
		for (i=0;i<10;i++)
		{
			printf("[SCK] ch = %d, c_ip = %d, status = %d\n",led_s[i].ch,led_s[i].c_ip,led_s[i].status);
			sprintf(out_msg,"ch = %d, c_ip = %d, status = %d\n",led_s[i].ch,led_s[i].c_ip,led_s[i].status);
			fwrite(out_msg,strlen(out_msg),1,fp);
		}
		fclose(fp);
	}
	return 0;
}
// ----------------------------------------------------------------------------
//Socket Server 決定燈號
static void *Socket_thread(void *arg)
{
	printf("[SCK] thread start .....PID: %u\n", (u32)gettid());
	int socket_desc , client_sock , c , read_size,ch,i,n,x;
	int count = 0;
	int	cout = 1;
	struct sockaddr_in server , client;
	char log_msg[64];
	char client_message[128];
	char msg[64];
	char *tmp_msg[64];
	char *tmp;
	//Create socket
_cmd_init:
	led_s[0].ch = 0;
	led_s[0].c_ip = 0;
	socket_desc = socket(AF_INET , SOCK_STREAM , 0);
	if (socket_desc == -1)
	{
		printf("[SCK] Could not create socket\n");
	}
	printf("[SCK] Socket created\n");

	//Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons( 8012 );

	//Bind
	if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
	{
		//print the error message
		printf("[SCK] bind failed. Error\n");
		sleep(1);
		goto _cmd_init;
	}
	printf("[SCK] bind done\n");

	//Listen
	listen(socket_desc , 32);

	//Accept and incoming connection
	printf("[SCK] Waiting for incoming connections...\n");
	c = sizeof(struct sockaddr_in);

	//accept connection from an incoming client
	while(1)
	{
		client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c);
		if (client_sock < 0)
		{
			printf("[SCK] accept failed\n");
			goto _cmd_init;
		}
		printf("[SCK] Connection accepted\n");

		//Receive a message from client
		if (read_size = recv(client_sock , client_message , 128 , 0) > 0 )
		{
			//Send the message back to client
			led_s[0].status = led_status ;
			if (led_s[0].status == 0)
			{
				vs1_set_do((led_lights/10),0);
			}
			n = 0;
			tmp = strtok(client_message, "|");
			tmp_msg[0] = tmp;
			x = strlen(tmp_msg[0]);
			tmp = tmp + x + 1;
			printf("[SCK] [SECTION] tmp_msg[%d] = %s . \n", 0, tmp_msg[0]);
			if (strcmp(tmp_msg[0],"LED_Status")==0)
			{
				while( (tmp_msg[cout]=strtok(tmp, "|") ) != 0)
				{
					x = strlen(tmp_msg[cout]);
					tmp = tmp + x + 1;
					printf("[SCK] [SECTION] tmp_msg[%d] = %s . \n", cout, tmp_msg[cout]);
					cout ++;
				}
				ch = atoi(tmp_msg[1]);
				led_s[ch].ch = ch;
				led_s[ch].c_ip = atoi(tmp_msg[2]);
				led_s[ch].status = atoi(tmp_msg[3]);
				if (ch > count){count = ch;}
				if (led_s[ch].status == 0 && mflag == 0)
				{
					vs1_set_do((led_lights/10),0);
				}
				for (i=0;i<=count;i++)
				{
					if (led_s[i].status > 0){n++;}
				}
				if (n == (count+1) && gflag == 0)
				{
					vs1_set_do((led_lights%10),0);
				}
				write_led_log();
				sprintf(msg,"|LED_Status_OK||");
				printf("msg\n\n\n");
				//write(client_sock , msg , strlen(msg));
			}
			else if (strcmp(tmp_msg[0],"LED_Lights")==0)
			{
				while( (tmp_msg[cout]=strtok(tmp, "|") ) != 0)
				{
					x = strlen(tmp_msg[cout]);
					tmp = tmp + x + 1;
					printf("[SCK] [SECTION] tmp_msg[%d] = %s . \n", cout, tmp_msg[cout]);
					cout ++;
				}
				write_led_lights(atoi(tmp_msg[1]));
				led_lights = atoi(tmp_msg[1]);
				sprintf(msg,"|LED_Lights_OK||");
				write(client_sock , msg , strlen(msg));
			}
			cout = 1;
			close(client_sock);
		}
		if (mflag == 1)
		{
			vs1_set_do((led_lights%10),0);
		} 
		else if(gflag == 1)
		{
			vs1_set_do((led_lights/10),0);
		}
		if(read_size == 0)
		{
			printf("[SCK] Client disconnected\n");
		}
		else if(read_size == -1)
		{
			printf("[SCK] recv failed\n");
		}
		msleep(100);
	}
}
//-----------------------------------------------------------------------------
//Socket client 傳送燈號
static client_thread(void *arg)
{//|LED_Status|ch|c_ip|status|
	int sock;
	char ip[32];
	u32     my_ip;
	struct sockaddr_in server;
	struct timeval timeout;
	timeout.tv_sec=10;    
	timeout.tv_usec=0;
	char message[128] , server_reply[128], tmp[128];
	sprintf(ip,"10.53.43.%d",(server_ip%100));
	printf("[SCK] [Client] Socket created ip = %s\n",ip);
	get_ip("eth0",&my_ip);
	printf("ip is %d",my_ip);
	while(1)
	{
_Create_socket:	
		//Create socket
		sock = socket(AF_INET , SOCK_STREAM , 0);
		if (sock == -1)
		{
			printf("[SCK] [Client] Could not create socket\n");
		}
		server.sin_addr.s_addr = inet_addr(ip);
		server.sin_family = AF_INET;
		server.sin_port = htons( 8012 );
		//Connect to remote server
		if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < 0)
		{
			printf("[SCK] [Client] connect failed. Error %s\n",strerror(errno));
			sleep(60);
			goto _Create_socket;
		}
		printf("[SCK] [Client] Connected\n");
		if (mflag == 1)
		{
			led_status = 1;
		} 
		else if(gflag == 1)
		{
			led_status = 0;
		}
		//keep communicating with server
		if (led_flag > 0)
		{
			sprintf(message,"|LED_Lights|%d|",led_lights);
			printf("[SCK] [Client] Enter message : %s\n",message);
			led_flag = 0;
		}
		else
		{
			sprintf(message,"|LED_Status|%d|%d|%d|",(server_ip/100),(my_ip-171256576),led_status);
			printf("[SCK] [Client] Enter message : %s\n",message);
		}
		//set timeout
		int result = setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,(char *)&timeout.tv_sec,sizeof(struct timeval));
		if (result < 0)
		{
			perror("setsockopt"); 
		}

		//Send some data
		if( send(sock , message , strlen(message) , 0) < 0)
		{
			printf("[SCK] [Client] Send failed\n");
		}
		//Receive a reply from the server
		if( recv(sock , server_reply , 2000 , 0) < 0)
		{
			printf("[SCK] [Client] recv failed\n");
			goto _Create_socket;
		}
		printf("[SCK] [Client] Server reply : %s \n",server_reply);
		close(sock);
		sleep(1);
	}	
}
//-----------------------------------------------------------------------------
//切換鏡頭
void *Switch_ch_thread(void *arg)
{
	while(1){
		if (mode == 0)
		{
			printf("\n\n\n[SW_CH1]\n\n\n");
			dm_SetCh(1);
			mode = 1;
			yuv_count=0;
		}
		else if (mode == 1)
		{
			printf("\n\n\n[SW_CH2]\n\n\n");
			dm_SetCh(0);
			mode = 0;
			yuv_count=0;
		}
		system("echo 1 > /dev/watchdog");
		sleep(2);
	}
}
// ----------------------------------------------------------------------------
//YUV解析度改變 
int yuv_change(int Yuv_Resolution)
{
	va_dbg("[Va_Change] Setup4 : %d Setup3 : %d Setup2 : %d Setup : %d\n", cds_setup4, cds_setup3, cds_setup2, cds_setup);
	if(Yuv_Resolution == 1)
	{
		roi_width = 1080;
		roi_height = 1920;
	}
	else if(Yuv_Resolution == 0)
	{
		roi_width = 540;
		roi_height = 960;
	}
	cds_setup4 = 1;cds_setup3 = 1;cds_setup2 = 1;cds_setup = 1;
	return 0;
}
// ----------------------------------------------------------------------------
//ROI_index 
static int parse_usrdata(char *ini)
{
	//char inii[64];
	//char ini = vs1_get_userdata();
	//strcpy(inii,"divided=20;th1=20;th2=30;Src1_ROIcnt=1;Src2_ROIcnt1=0;");
	//sprintf("[VAC] [USRDATA] %s .\n", inii);
	char *ini_section[64];
	char *tmp_section[64];
	char *use_section[64];
	//---------------------------0----------1------2------3-------------4-------------5----------6----------7----------8----------9----------------10----------------11-----------------12---------13---------14---------15---------16---------------17--------------18---------------19---------20---------21---------22---------23---------------24----------------25-----------------26---------27---------28---------29---------30---------------31----------------32-----------------33--------34-----------35---------36------------37-------38--------39------------40----------41
	char  default_section[640]={"divided=20;th1=20;th2=30;Src1_ROIcnt=1;Src2_ROIcnt=0;ROI1_L=404;ROI1_T=251;ROI1_R=927;ROI1_B=576;ROI1_flag_left=0;ROI1_low_gray=255;ROI1_high_gray=255;ROI2_T=251;ROI2_L=404;ROI2_R=927;ROI2_B=527;ROI2_flag_left=0;ROI2_low_gray=0;ROI2_high_gray=0;ROI3_L=404;ROI3_T=251;ROI3_R=927;ROI3_B=576;ROI3_flag_left=0;ROI3_low_gray=255;ROI3_high_gray=255;ROI4_T=251;ROI4_L=404;ROI4_R=927;ROI4_B=576;ROI4_flag_left=0;ROI4_low_gray=255;ROI4_high_gray=255;DMethod=1;Con_events=6;writelog=0;frame_count=0;son_id=0;vs_type=0;socket_type=0;server_ip=0;led_lights=12"};//如有增加需注意順序
	int	cout = 1;
	int cout1 = 1;  
	int len;
	int x,y,a,b;
	char *tmp = strtok(ini, ";");
	ini_section[0] = tmp;
	x = strlen(ini_section[0]);
	printf("[VAC] [SECTION] ini_section[0] = %s (%d). \n", ini_section[0], x);

	tmp = tmp + x + 1;
	printf("[VAC] [SECTION] ini_tmp = %s . \n", tmp);
	while( (ini_section[cout]=strtok(tmp, ";") ) != 0)
	{
		x = strlen(ini_section[cout]);
		tmp = tmp + x + 1;
		printf("[VAC] [SECTION] ini_section[%d] = %s . \n", cout, ini_section[cout]);
		cout ++;
	}
	printf("first char cut finish\n");
	tmp = strtok(default_section,";");
	tmp_section[0] = tmp;
	x = strlen(tmp_section[0]);
	printf("[VAC] [SECTION] tmp_section[0] = %s (%d). \n", tmp_section[0], x);
	tmp = tmp + x + 1;
	printf("[VAC] [SECTION] tmp_tmp = %s . \n", tmp);

	while( (tmp_section[cout1]=strtok(tmp, ";") ) != 0)
	{
		x = strlen(tmp_section[cout1]);
		tmp = tmp + x + 1;
		printf("[VAC] [SECTION] tmp_section[%d] = %s . \n", cout1, tmp_section[cout1]);
		cout1 ++;
	}

	printf("[VAC] [SECTION] cout = %d , cout1 = %d . \n", cout, cout1);

	for (x=0 ; x<cout1 ; x++)
	{
		len = strspn(tmp_section[x],"123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_");
		for (y=0 ; y<cout ; y++)
		{		
			if(memcmp(tmp_section[x],ini_section[y],len) != 0)
			{
				use_section[x] = tmp_section[x];
			}
			else
			{
				use_section[x] = ini_section[y];

				break;
			}
		}
		printf("result [VAC] [SECTION] use_section[%d] = %s\n", x, use_section[x]);
	}
	tmp = strstr( use_section[3], "=");
	src1_num   = atoi(tmp+1);
	tmp = strstr( use_section[4], "=");
	src2_num   = atoi(tmp+1);
	printf("\n\na= %d , b = %d\n\n",src1_num,src2_num);
	total_roi = src1_num+src2_num;

	for(x=0 ; x<4 ; x++)
	{
		tmp = strstr( use_section[(x*7)+5], "=");
		usr_data[x].ROI_L = atoi(tmp+1);
		tmp = strstr( use_section[(x*7)+6], "=");
		usr_data[x].ROI_T = atoi(tmp+1);
		tmp = strstr( use_section[(x*7)+7], "=");
		usr_data[x].ROI_R = atoi(tmp+1);
		tmp = strstr( use_section[(x*7)+8], "=");
		usr_data[x].ROI_B = atoi(tmp+1);

		usr_data[x].center_x = (usr_data[x].ROI_R - usr_data[x].ROI_L) / 2;
		usr_data[x].center_y = (usr_data[x].ROI_B - usr_data[x].ROI_T) / 2;
	}
	//tmp = strstr(use_section[34],"=");
	//confidence = atoi(tmp+1);
	tmp = strstr( use_section[36], "=");
	frame_count = atoi(tmp+1);
	tmp = strstr( use_section[37], "=");
	x = atoi(tmp+1);
	switch(x)
	{
	case 0://0-0
		usr_data[0].enable = usr_data[1].enable = 0;
		mflag = 0;gflag=0;
		break;
	case 1://US-0
		usr_data[0].enable = 1;usr_data[1].enable = 0;
		mflag = 0;gflag=0;
		break;
	case 2://0-US
		usr_data[0].enable = 0;usr_data[1].enable = 1;
		mflag = 0;gflag=0;
		break;
	case 3://US-US
		usr_data[0].enable = 1;usr_data[1].enable = 1;
		mflag = 0;gflag=0;
		break;
	case 4://R-0
		mflag = 1;gflag=0;
		usr_data[0].enable = 0;usr_data[1].enable = 0;
		break;
	case 5://R-US
		mflag = 1;gflag=0;
		usr_data[0].enable = 0;usr_data[1].enable = 1;
		break;
	case 6://0-R
		mflag = 1;gflag=0;
		usr_data[0].enable = 0;usr_data[1].enable = 0;
		break;
	case 7://US-R
		mflag = 1;gflag=0;
		usr_data[0].enable = 1;usr_data[1].enable = 0;
		break;
	case 8://R-R
		mflag = 1;gflag=0;
		usr_data[0].enable = 0;usr_data[1].enable = 0;
		break;
	case 9://G-0
		gflag = 1;mflag = 0;
		usr_data[0].enable = 0;usr_data[1].enable = 0;
		break;
	case 10://G-US
		gflag = 1;mflag = 0;
		usr_data[0].enable = 0;usr_data[1].enable = 1;
		break;
	case 11://0-G
		gflag = 1;mflag = 0;
		usr_data[0].enable = 0;usr_data[1].enable = 0;
		break;
	case 12://US-G
		gflag = 1;mflag = 0;
		usr_data[0].enable = 1;usr_data[1].enable = 0;
		break;
	case 13://G-G
		gflag = 1;mflag = 0;
		usr_data[0].enable = 0;usr_data[1].enable = 0;
		break;
	default:
		mflag = 0;gflag = 0;
		usr_data[0].enable = 0;usr_data[1].enable = 0;
		break;
	}
	tmp = strstr(use_section[38], "=");
	flag=atoi(tmp+1);
	tmp = strstr(use_section[39], "=");
	socket_type=atoi(tmp+1);
	tmp = strstr(use_section[40], "=");
	server_ip=atoi(tmp+1);
	tmp = strstr(use_section[41], "=");
	led_lights=atoi(tmp+1);
	printf("parse flag = %d\n",flag);

	return 1;
}
// ----------------------------------------------------------------------------
//重複確認(超音波) 
static int Double_Check(int index, int detect)
{
	printf("[VAC] [DCHECK] (%d--%d), tmp-%d(%d). \n", index, detect, tmp_state[index], state_count[index]);

	if(tmp_state[index] == detect)
	{
		state_count[index] = state_count[index] + 1;
		if (state_count[index] < DETECT_CONFIDENCE)
		{
			return 0;
		}
		else
			return 1;
	}
	else
	{
		state_count[index] = 0;
		tmp_state[index] = detect;
	}

	return 0;
}
// ----------------------------------------------------------------------------
//IMG Copy (VS1) 
static int phy_copy(u32 src, u32 dest, u16 width, u16 height)
{
	/* declaration */
	HI_S32 s32Ret;
	TDE_HANDLE s32Handle;
	TDE_SURFACE_S stSrc;
	TDE_SURFACE_S stDst;
	TDE_OPT_S stOpt = {0};
	/* create a TDE job */
	s32Handle = HI_TDE2_BeginJob();
	if(HI_ERR_TDE_INVALID_HANDLE == s32Handle)
	{
		return -1;
	}
	/* source bitmap info */
	stSrc.enColorFmt = TDE_COLOR_FMT_RGB8888;
	stSrc.pu8PhyAddr = (u8 *)src;
	stSrc.u16Width = width/4;
	stSrc.u16Height = height;
	stSrc.u16Stride = width;
	/* destination bitmap info */
	stDst.enColorFmt = TDE_COLOR_FMT_RGB8888;
	stDst.pu8PhyAddr = (u8 *)dest;
	stDst.u16Width = width/4;
	stDst.u16Height = height;
	stDst.u16Stride = width;

	/* operation info */
	stOpt.enDataConv = TDE_DATA_CONV_ROP; /* ROP */
	stOpt.enRopCode = TDE_ROP_PS;
	stOpt.stColorSpace.bColorSpace = HI_FALSE; /* no colorspace */
	stOpt.stAlpha.enOutAlphaFrom = TDE_OUTALPHA_FROM_SRC; /* out alpha from source bitmap */
	/* add the bitblt command to the job */
	s32Ret = HI_TDE2_Bitblit(s32Handle, &stSrc, &stDst, &stOpt);
	if(HI_SUCCESS != s32Ret)
	{
		va_err("add bitlit command failed!\n");
	}
	/* submit the job */
	HI_TDE2_EndJob(s32Handle, HI_FALSE, 0);
	HI_TDE2_WaitForDone();

	return 0;
}
// ----------------------------------------------------------------------------
//Buffer_init 
static int buffer_init(void)
{
	int i,j;
#ifdef TEMP_BUF
		img_buffer1_1920 = malloc(2048*2048);
		memset(img_buffer1_1920, 0, 1920*1080);
		img_buffer1_960 = malloc(1024*1024);
		memset(img_buffer1_960, 0, 960*480);
#endif
	//int mem_fd = open("/dev/mem", O_CREAT|O_RDWR|O_SYNC);
	//if(mem_fd < 0) {
	//	va_err("Can't open /dev/mem\n");
	//	return -1;
	//}
	for (i=0 ; i<4 ; i++)
	{
		buffer_info[i].cd_idx = -1;
		buffer_info[i].w_idx = 0;
		buffer_info[i].temp_inuse = 0;
/*		for (j=0;j<4;j++)
		{
			buffer_info[i].buffers[j].virt = malloc(2048*2048);
			memset(buffer_info[i].buffers[j].virt, 0, 1920*1080);
		}*/		
	}
	img_buffer1_1920 = malloc(2048*2048);
	memset(img_buffer1_1920, 0, 2048*2048);
	temp_buffer = malloc(2048*2048);
	memset(temp_buffer, 0, 2048*2048);
	//img_buffer1_960 = malloc(1024*1024);
	//memset(img_buffer1_960, 0, 960*480);

	//img_buffer2_1920 = malloc(2048*2048);
	//memset(img_buffer2_1920, 0, 1920*1080);
	//img_buffer2_960 = malloc(1024*1024);
	//memset(img_buffer2_960, 0, 960*480); 雙鏡頭BUFFER

	detect_roi[0]=0;detect_roi[1]=0;detect_roi[2]=0;detect_roi[3]=0;

	//pool = HI_MPI_VB_CreatePool(BSIZE, CD_GROUP_NR);
	//if(pool == VB_INVALID_POOLID) {
	//	va_err("Create pool failed\n");
	//	close(mem_fd);
	//	return -1;
	//}
	//printf("BIBIBIBIBI\n");
	//for (j=0;j<4;j++)
	//{	
	//	for(i=0;i<CD_GROUP_NR;i++) {
	//	buffer_info[j].buffers[i].inuse = 0;
	//	buffer_info[j].buffers[i].blk = HI_MPI_VB_GetBlock(pool, BSIZE);
	//	buffer_info[j].buffers[i].size = BSIZE;
	//	if(VB_INVALID_HANDLE != buffer_info[j].buffers[i].blk) {	printf("BIBIBIBIBI\n");
	//		buffer_info[j].buffers[i].phy = HI_MPI_VB_Handle2PhysAddr(buffer_info[j].buffers[i].blk);
	//		if(buffer_info[j].buffers[i].phy) {
	//			buffer_info[j].buffers[i].virt = (u8 *)mmap(NULL, BSIZE, PROT_READ|PROT_WRITE, MAP_SHARED, mem_fd, buffer_info[j].buffers[i].phy);
	//			if(buffer_info[j].buffers[i].virt == MAP_FAILED) {
	//				va_err("mmap failed @ %d\n", i);
	//			}
	//			else {
	//				va_dbg("Virt #%d: phy=%p, virt=%p \n", i, (void *)buffer_info[j].buffers[i].phy, buffer_info[j].buffers[i].virt);
	//			}
	//		}
	//	}
	//}
	//}
	//


	//close(mem_fd);

	if(HI_TDE2_Open() < 0) {
		va_err("TDE open falied\n");
	}

	return 0;
}
// ----------------------------------------------------------------------------
//Buffer_exit 
static int buffer_exit(void)
{
	int i,j;
	for (j=0;j<total_roi;j++)
	{
		for(i=0;i<CD_GROUP_NR;i++) {
			if(buffer_info[j].buffers[i].blk != VB_INVALID_HANDLE)
				HI_MPI_VB_ReleaseBlock(buffer_info[j].buffers[i].blk);
		}
	}

	if(pool != VB_INVALID_POOLID)
		(void)HI_MPI_VB_DestroyPool(pool);

	if(HI_TDE2_Close() < 0) {
		va_err("TDE close falied\n");
	}
	return 0;
}
// ----------------------------------------------------------------------------
//Buffer_reset 
static void buffer_reset(int x)
{
	int i;
	printf("CH[%d] Reset\n",x);
		for(i=0;i<CD_GROUP_NR;i++)
		{
			buffer_info[x].buffers[i].inuse = 0;
		}
		buffer_info[x].cd_idx = -1;

#ifdef TEMP_BUF
		buffer_info[x].temp_inuse = 0;
#endif
}
// ----------------------------------------------------------------------------
//車道版Thread(son_id=1) 
static void *lane_thread(void *arg)
{
	unsigned int di_status=0, di_status_prev=0;
	unsigned int do_status=0, do_status_prev=0;
	int		tid = syscall(__NR_gettid);
	char	log_msg[512];
	printf("VA Thread(TID=%d) Start\n", tid);
	//Write_Log(1,log_msg);

	int		nToCapture=0;
	while(1) 
	{
		//if (as_gen_wdt_keep_alive() < 0) {
		//	printf("as_gen_wdt_keep_alive failed\n");
		//	//Write_Log(1,log_msg);
		//}
		// DI process
		di_status = vs1_get_di();	//get DI
		do_status = 0;//vs1_get_do();	//get DO
		if( nToCapture > 0 )		//still in capture process => ignore DI status
		{
			nToCapture--;
			if( nToCapture == 0 )
			{
				vs1_set_motion(0);
				printf("[CAPTURE] STOP (DI=%u/%u,Cnt=%d,DO=%u)\n", di_status, di_status_prev, nToCapture, do_status );
				//Write_Log(1,log_msg);
			}
			else
			{
				printf("[CAPTURE] CONTINUE (DI=%u/%u,Cnt=%d,DO=%u)\n", di_status, di_status_prev, nToCapture, do_status );
				//Write_Log(1,log_msg);
			}
		}
		else if( di_status > 0  && di_status_prev==0 )	//not in capture process && DI on => start capture process
		{
			vs1_set_motion(1);
			nToCapture = 16;
			printf("[CAPTURE] START (DI=%u/%u,Cnt=%d,DO=%u)\n", di_status, di_status_prev, nToCapture, do_status );
			//Write_Log(1,log_msg);
		}
		//		else						//do nothing
		//			printf( "[CAPTURE] OFF (DI=%u/%u,Cnt=%d)\n", di_status, di_status_prev, nToCapture );
		else if( di_status != di_status_prev )
		{
			printf("[CAPTURE] CHANGE (DI=%u/%u,Cnt=%d,DO=%u)\n", di_status, di_status_prev, nToCapture, do_status );
			//Write_Log(1,log_msg);
		}
		di_status_prev = di_status;

		if(vs1_get_motion_client()) {
			if(yuv_enable) {
				if(yuv_inuse) {
					//yuv_proc(yuv_width, yuv_height, yuv_buf, yuv_size);
					yuv_inuse = 0;
					msleep(10);
				}
				else
					msleep(10);
			}
			else {
				yuv_inuse = 0;
				yuv_enable = 1;
			}
		}
		else {
			yuv_enable = 0;
			msleep(10);
		}
	}

	pthread_exit("Lane Thread Exit");
	return 0;
}
// ----------------------------------------------------------------------------
//PC版Thread(不通過3531)(son_id=3)
//static void *pco_thread(void *arg)
//{
//	va_dbg("thread start .....PID:%u \n", getpid());
//	char  *ini ;
//    HANDLE      hDetect = NULL;
//    LPCRDRST    lpResult = NULL;
//    int         width = 0;
//    int         height = 0;
//	int			nn = 0,mm = 0;
//
//	char        path[32];
//	int			save_index = 0;
//	int			save_count = 0;
//	int			x;
//	char		rs485_msg[4];
//	roi_info_t	roi;
//
//	roi.width = roi.height = 0;
//	roi.top = roi.left = roi.bottom = roi.right = 0;
//	roi.status = 0;
//
//	vs1_roi_out(&roi);
//
//    lpResult = (LPCRDRST)malloc(sizeof(CRDRST)*ROI_NR);
//
//	//if (as_gen_wdt_disable() < 0) {
//	//	printf("as_gen_wdt_disable error\n");
//	//	va_dbg("thread exit .....PID:%u \n", getpid());
//	//}
//
//	uart_attr_t uac;
//	uac.baudrate = 4;       // 0=115200, 1=57600, 2=38400, 3=19200, 4=9600, 5=4800, 6=2400, 7=1200
//	uac.data_bits = 8;      // 5,6,7,8
//	uac.stop_bits = 1;      // 1,2
//	uac.parity = 0;         // 0:none, 1:odd, 2:even
//	uac.flow_control = 0;   // 0:no, 1:XON/XOFF, 2:RTS/CTS
//	printf("[VAC] [RS] uac - bau=%d, dat=%d, stop=%d, par=%d, flow=%d. \n", uac.baudrate, uac.data_bits, uac.stop_bits, uac.parity, uac.flow_control);
//
//_roi_init_:
//    sleep(1);
//    if(roi_width == 0 || roi_height == 0) 
//        goto _roi_init_;
//    width = roi_width;
//    height = roi_height;
//
//	ini = vs1_get_userdata();
//
//	va_dbg("parse_usrdata\n");
//	parse_usrdata(ini);
//    if((hDetect = CRDCreate(width, height, ROI_NR, src1_num, 0, 0)) == 0) {
//        va_err("CRDCreate failed\n");
//        goto _roi_init_;
//    }
////printf("[WATCHDOG]as_gen_wdt_enable\n");
////	if (as_gen_wdt_enable() < 0) {
////		printf("as_gen_wdt_enable failed\n");
////		return -1;
////	}
////printf("[WATCHDOG]as_gen_wdt_set_margin\n");
////	if (as_gen_wdt_set_margin(WDT_MARGIN_TIME) < 0) {
////		printf("as_gen_wdt_set_margin failed\n");
////		return -1;
////	}//*/
//
//    va_dbg(" CRDCreate success: %d x %d\n", width, height);
//
//    buffer_reset();
//
//	tmp_state[0] = 0;
//	state_count[0] = 0;
//	tmp_state[1] = 0;
//	state_count[1] = 0;
//	tmp_state[2] = 0;
//	state_count[2] = 0;
//	tmp_state[3] = 0;
//	state_count[3] = 0;
//	va_dbg("x = %d %d(first)\n",tmp_state[0],state_count[0]);	
//	va_dbg("x = %d %d(first)\n",tmp_state[1],state_count[1]);
//	va_dbg("x = %d %d(first)\n",tmp_state[2],state_count[2]);
//	va_dbg("x = %d %d(first)\n",tmp_state[3],state_count[3]);
//
//
//    while(1) {
//		va_dbg("buffer_reset OK\n");
//        if(cds_setup) {
//            va_dbg("Reset\n");
//            if(hDetect) {
//                CRDFree(hDetect);
//                hDetect = 0;
//            }
//            cds_setup = 0;
//            goto _roi_init_;
//        }
//
//#ifdef TEMP_BUF
//		printf("%d\n",temp_inuse);
//        if(temp_inuse) {
//#ifdef WRITE2FILE
//			HIMAGE himage;
//			LPUIMAGE	lpImage;
//			FILE		*fp;
//			int			rc;
//			char		Msg[256];
//			int sample;
//			//temp_buf = malloc(2048*2048);
//			//memset(temp_buf, 0, 1920*1080);
//			/*sprintf(path,"/dev/vi.yuv");
//			himage = OCR_LoadImage(path);
//			lpImage=(LPUIMAGE)himage;
//			memcpy(temp_buf,lpImage->lpBuffer,(1920*1080*3)/2);
//			OCR_FreeImage(himage);*/
//			//sprintf(path,"test-2.obi");
//			//himage = OCR_LoadImage(path);
//			//lpImage=(LPUIMAGE)himage;
//			//OCR_WriteImage("retest.obi",himage,IMG_FMT_OBI,0);
//			//temp_buf = Image2Raw(himage,&width,&height,&sample);
//			//RAW2File("//app//test.obi", temp_buf, width, height, 8, IMG_FMT_OBI, 0);
//			//himage = OCR_LoadImage("//dev//vi.yuv");
//			//if (himage != NULL)
//			//{
//			//	//printf("%d %d\n",himage->width,himage->height);	
//			//	OCR_WriteImage(himage,"VS2_test.obi");
//			//}
//			
//			/*fp = fopen("//dev//vi.yuv", "wb");
//			if(fp) {
//				fread(temp_buf, 1, 1920*1080, fp);
//				fclose(fp);
//			}*/
//			//fp=fopen("VS2_test.obi","wb");
//			//if (fp)
//			//{
//			//	fwrite(lpImage,UIMAGE_HD_SIZE,1,fp);
//			//	fwrite(temp_buf,1920*1080,1,fp);
//			//	fclose( fp );
//			//}
//			
//#endif
//            va_dbg("Start detect\n");  
//			//as_gen_wdt_disable();
//			int nn ;
//			nn = CRDDetect(hDetect, temp_buf, NULL, lpResult);
//			//as_gen_wdt_keep_alive();
//			save_count++;
//			if (save_count == 9)
//			{
//				save_count = 0;
//			}
//        }
//#else
//		//rs485
////		if(usr_data[0].enable)
////		{
////			usr_data[0].detect = rs485_wave(&uac, 1);
////printf("[VAC] [RS] device 1 return %d . \n", usr_data[0].detect);
////		}
////		if(usr_data[1].enable)
////		{
////			usr_data[1].detect = rs485_wave(&uac, 2);
////printf("[VAC] [RS] device 2 return %d . \n", usr_data[0].detect);
////		}
////	
//        if(cd_idx < 0) {
//            int idx = (w_idx > 0) ? (w_idx - 1) : (CD_GROUP_NR-1);
//			printf("[VAC] w_idx=%d, idx=%d, inuse=%d. \n",w_idx, idx, buffers[idx].inuse);
//			//RAW2File(".//inuse.obi", buffers[idx].virt, 1280, 720, 8, IMG_FMT_OBI, 0);
//            if(buffers[idx].inuse) 
//			{
//				//if (as_gen_wdt_keep_alive() < 0) {
//				//	printf("as_gen_wdt_keep_alive failed\n");
//				//}
//                cd_idx = idx;
//                va_dbg("Start detect @ %d\n", idx);
//#ifdef WRITEFILE
//struct timeval tt;
//struct	tm *now;
//char	time_buffer_YDMH[32];
//char	time_buffer_MS[32];
//
//gettimeofday(&tt, NULL);
//now = localtime(&tt.tv_sec);
//strftime (time_buffer_YDMH, 80, "%Y%m%d%H", now);
//strftime (time_buffer_MS, 80, "%M%S", now);
////sprintf(path, "//dbg//%s//test-%s.obi", time_buffer_YDMH, time_buffer_MS);
//sprintf(path, "test-%d.obi", save_count);
//printf("[DBG]\n\n\n %s-----%d. \n\n\n", path,save_count);
//RAW2File(path, buffers[idx].virt, buffers[idx].width, buffers[idx].height, 8, IMG_FMT_OBI, 0);
//save_count++;
//if (save_count == 9)
//{
//	save_count = 0;
//}
//#endif		
////HIMAGE h;
////LPUIMAGE   lp;
////sprintf(path,"test.obi");
////h = OCR_LoadImage(path);
////lp = (LPUIMAGE)h;
////memcpy(lp->lpBuffer,temp_buf,1920*1080);
////OCR_WriteImage((HIMAGE)lp,"test1.obi");
//				if (buffers[idx].samplerate == 0)
//				{
//					//printf("[DBG]\n\n\n buffers[idx].samplerate = %d. \n\n\n", buffers[idx].samplerate);
//					//nn = CRDDetect(hDetect, temp_buf, NULL, lpResult,0);
//				}
//				else if (buffers[idx].samplerate == 1)
//				{
//					//printf("\n\n\n[DBG] buffers[idx].samplerate = %d. \n\n\n", buffers[idx].samplerate);
//					//nn = CRDDetect(hDetect, temp_buf, NULL, lpResult,0);
//				}
//	
//                if((nn) >0) 
//				{
//					//if (as_gen_wdt_keep_alive() < 0) {
//					//	printf("as_gen_wdt_keep_alive failed\n");
//					//}
//                    int i;
//					int roi_decect_count = 0;
//                    for(i=0;i<nn;i++) 
//					{
//                        LPCRDRST lpRst = &lpResult[i];
//                        roi.id = i;
//                        roi.sec = buffers[cd_idx].sec;//獲取偵測時間->移動偵測
//                        roi.usec = buffers[cd_idx].usec;
//                        if(lpRst->nConfidence > 1) 
//						{
//							if( (usr_data[i].enable==1) && (usr_data[i].detect==0) )
//							{
//								printf("[ROI-%d] [RS] goto NO_EVENT . \n", i);
//								goto NO_EVENT;
//							}
//
//							//if( !Double_Check(i , 1) )
//								//if(usr_data[i].enable==0)
//									//continue;
//							printf("\n\nW = %d H = %d\n\n",width,height);
//                            roi.status = 1;
//                            roi.width = 0;
//                            roi.height = 0;
//                            roi.top = 0;
//                            roi.left = 0;
//                            roi.bottom = 0;
//                            roi.right = 0;
//                            int j;
//                            for(j=0;j<CD_GROUP_NR;j++) {
//                                int k = (cd_idx + j) % CD_GROUP_NR;
//                                roi.data[j] = temp_buf;
//                            }
//                            va_dbg("[ROI-%d] conf=%d, boundary= (%d,%d)(%d,%d)\n", i, lpRst->nConfidence,
//                                    lpRst->rtBoundary.left, lpRst->rtBoundary.top, lpRst->rtBoundary.right, lpRst->rtBoundary.bottom);
//							roi_decect_count = roi_decect_count + 1;
//                            //vs1_set_do(2);
//                        }
//                        else 
//						{
//NO_EVENT:
//							//if( !Double_Check(i, 0) )
//								//if (usr_data[i].enable==0)
//									//continue;
//						
//							roi.width = roi.height = 0;
//							roi.top = roi.left = roi.bottom = roi.right = 0;
//							roi.status = 0;
//							va_dbg("[ROI-%d] Clear  sec = %u usec = %u\n", i,roi.sec,roi.usec);
//							
//							//rs485
//							if( (usr_data[i].enable==1) && (usr_data[i].detect==1) )
//							{
//								printf("[ROI-%d] [RS] into center x/y function . \n", i);
//								roi.id = i;
//								roi.sec = buffers[cd_idx].sec;
//								roi.usec = buffers[cd_idx].usec;
//
//								roi.status = 1;
//								roi.width = 0;
//								roi.height = 0;
//								roi.top = 0;
//								roi.left = 0;
//								roi.bottom = 0;
//								roi.right = 0;
//								int j;
//								for(j=0;j<CD_GROUP_NR;j++) {
//									int k = (cd_idx + j) % CD_GROUP_NR;
//									roi.data[j] = temp_buf;
//								}
//								printf("[ROI-%d] [RS] conf=0, boundary= (%d,%d)(%d,%d)\n", i, roi.left, roi.top, roi.right, roi.bottom);
//				
//								roi_decect_count = roi_decect_count + 1;
//							} 
//                            //vs1_set_do(1);
//                        }
//ROI_OUT:
//                        printf("Before ROI OUT \n"); 
//						vs1_roi_out(&roi);
//                    }
//							printf("\n\nroi = %d , n = %d , m = %d\n\n",roi_decect_count,nn,mm);
//					if( nn == roi_decect_count && gflag == 0)
//						{
//							vs1_set_do(2);
//						}
//					else //empty give green light
//					{
//						/*sprintf(path, ".//test.obi");
//						printf("[DBG] %s. \n", path);
//						RAW2File(path, buffers[idx].virt, buffers[idx].width, buffers[idx].height, 8, IMG_FMT_OBI, 0);*/
//						if (mflag == 1)//Monthly mode
//						{
//							vs1_set_do(2);
//						}
//						else //enforce green light mode
//						{
//							vs1_set_do(1);
//						}
//					}
//                }
//                else {
//                    va_err("CRDetect Error\n");
//                }
//                buffer_reset();
//            }
//        }
//#endif
//        sleep(2);
//    }
//
//    if(hDetect) {
//        CRDFree(hDetect);
//        hDetect = 0;
//    }
//    if(lpResult) {
//        free(lpResult);
//        lpResult = 0;
//    }
// //	if (as_gen_wdt_disable() < 0) {
// //		printf("as_gen_wdt_disable error\n");
// //	va_dbg("thread exit .....PID:%u \n", getpid());
//	//}
//    pthread_exit("VA Thread Exit");
//    return 0;
//}
// ----------------------------------------------------------------------------
//雙鏡頭va_thread2-1 
static void *va_thread2_1(void *arg)//img_buffer2已改為img_buffer1_960，後續要改雙鏡頭要再增加img_buffer2_1920，img_buffer2_960
{
	va_dbg("[VA3] thread start .....PID:%u \n", getpid());
	char  *ini ;
    HANDLE      hDetect = NULL;
    LPCRDRST    lpResult = NULL;
    int         width = 0;
    int         height = 0;
	int			nn = 0,mm = 0;
	int			i;

	char        path[32];
	int			save_index = 0;
	int			save_count = 0;
	int			x=0;
	char		rs485_msg[4];
	roi_info_t	roi;

	roi.width = roi.height = 0;
	roi.top = roi.left = roi.bottom = roi.right = 0;
	roi.status = 0;

	//vs1_roi_out(&roi);

    lpResult = (LPCRDRST)malloc(sizeof(CRDRST)*ROI_NR);


	uart_attr_t uac;
	uac.baudrate = 4;       // 0=115200, 1=57600, 2=38400, 3=19200, 4=9600, 5=4800, 6=2400, 7=1200
	uac.data_bits = 8;      // 5,6,7,8
	uac.stop_bits = 1;      // 1,2
	uac.parity = 0;         // 0:none, 1:odd, 2:even
	uac.flow_control = 0;   // 0:no, 1:XON/XOFF, 2:RTS/CTS
	va_dbg("[VA3] [RS] uac - bau=%d, dat=%d, stop=%d, par=%d, flow=%d. \n", uac.baudrate, uac.data_bits, uac.stop_bits, uac.parity, uac.flow_control);

/*	memset(rs485_msg , 0 ,4);
	rs485_msg[0] = 250;
	rs485_msg[1] = 1;
	rs485_msg[2] = 251;
	printf("[RS][VAC] rs485_msg = %X, %X, %X. \n", rs485_msg[0], rs485_msg[1], rs485_msg[2]);

	rs485_alloc(&uac, rs485_msg);*/

_roi_init3_:
    sleep(1);
    if(roi_width == 0 || roi_height == 0) 
        goto _roi_init3_;
    width = roi_width/2;
    height = roi_height/2;

#ifdef TEMP_BUF
	if((hDetect = CRDCreate(1920, 1080, ROI_NR, -3, 0, 0)) == 0) { 
		va_err("[VA3] CRDCreate failed\n");
		goto _roi_init3_;
	}
#else
	if((hDetect = CRDCreate(width, height, ROI_NR, -3, 0, 0)) == 0) { 
		va_err("[VA3] CRDCreate failed\n");
		goto _roi_init3_;
	}
#endif

    va_dbg("[VA3] CRDCreate success: %d x %d\n", width, height);

    buffer_reset(2);

	
	//x=0;
	/*va_dbg("x = %d (first)\n",save_index);
	save_index = 0;
	va_dbg("x = %d (first)\n",save_index);	
	for( save_index=0 ; save_index<ROI_MAX ; save_index++ )
	{
		va_dbg("%d\n",save_index);
		tmp_state[save_index] = 0;
		state_count[save_index] = 0;
	}*/
		va_dbg("[VA3] tmp_state init\n");
	for (i=0;i<4;i++)
	{
		tmp_state[i]=0;
		state_count[0]=0;
	}
		va_dbg("[VA3] tmp_state OK\n");	


    while(1) {
		//va_dbg("[VA3] buffer_reset OK\n");
        if(cds_setup3) {
            va_dbg("[VA3] Reset\n");
            if(hDetect) {
                CRDFree(hDetect);
                hDetect = 0;
            }
            cds_setup3 = 0;
            goto _roi_init3_;
        }

		//watchdog keep live
		/*if (as_gen_wdt_keep_alive() < 0) {
			printf("as_gen_wdt_keep_alive failed\n");
		}*/

        //va_dbg("===== %p = %08x =======\n", hDetect, ((u32 *)hDetect)[0]);
#ifdef TEMP_BUF
		//va_dbg("[VA3] %d\n",buffer_info[2].temp_inuse);
        if(buffer_info[2].temp_inuse==0) {
#ifdef WRITE2FILE
			HIMAGE h;
			LPUIMAGE   lp;

			sprintf(path,"test.obi"); 
			h = OCR_LoadImage(path);
			lp = (LPUIMAGE)h;
			memcpy(img_buffer1_960,lp->lpBuffer,1920*1080);
			//sprintf(path, "/dev/test-%d.obi", save_count);
			//OCR_WriteImage((HIMAGE)lp,path);
			//save_count++;
			//if (save_count == 9)
			//{
			//	save_count = 0;
			//}寫檔
			//va_dbg("\n\n\n[VA3] [DBG] buffers[idx].samplerate = %d. \n\n\n", buffer_info[2].buffers[idx].samplerate);
			OCR_FreeImage((HIMAGE)lp);
			
#endif
            va_dbg("[VA3] Start detect\n");  
			int nn ;
			nn = CRDDetect(hDetect, img_buffer1_960, img_buffer1_960, lpResult,2);
			if (nn>0)
			{
				int roi_decect_count = 0;
				for (i=0;i<nn;i++)
				{
					LPCRDRST lpRst = &lpResult[i];
					if(lpRst->nConfidence > 1) 
					{
						roi_decect_count = roi_decect_count + 1;
					}
				}
				if( nn == roi_decect_count && gflag == 0)
				{
					int dto=0,q;
					detect_roi[2] = roi_decect_count;
					for (q=0;q<4;q++)
					{
						dto=dto+detect_roi[q];
					}
					if (dto==total_roi){vs1_set_do(led_lights%10,socket_type);led_status = 1;}
				}
				else //empty give green light
				{
					detect_roi[2] = 0;
					printf("[VA2] empty. \n");
				}

			}
        }
#else
		//rs485
//		if(usr_data[0].enable)
//		{
//			usr_data[0].detect = rs485_wave(&uac, 1);
//printf("[VAC] [RS] device 1 return %d . \n", usr_data[0].detect);
//		}
//		if(usr_data[1].enable)
//		{
//			usr_data[1].detect = rs485_wave(&uac, 2);
//printf("[VAC] [RS] device 2 return %d . \n", usr_data[0].detect);
//		}
//	
        if(buffer_info[2].cd_idx < 0) {
            int idx = (buffer_info[2].w_idx > 0) ? (buffer_info[2].w_idx - 1) : (CD_GROUP_NR-1);
			//va_dbg("[VA3] w_idx=%d, idx=%d, inuse=%d. \n",buffer_info[2].w_idx, idx, buffer_info[2].buffers[idx].inuse);
            if(buffer_info[2].buffers[idx].inuse) 
			{
                buffer_info[2].cd_idx = idx;
                va_dbg("[VA3] Start detect @ %d\n", idx);
				FILE *fp;
				sprintf(path,"/dev/t960-%d-3.yuv",mm);
				//sprintf(path,"/dev/t960-3.yuv");
				if (mm==5){mm=0;}
				else{mm++;}
				//fp = fopen(path,"rb");
				fp = fopen(path,"wb");
				if (fp)
				{
					//fwrite(data,sizeof(dm_data_t),1,fp);
					printf("\n\nYES %d\n\n",mm);
					fwrite(img_buffer2_960,960*540,1,fp);
					//fread(img_buffer2_960,960*540,1,fp);
					fclose(fp);
				}
				nn = CRDDetect(hDetect, img_buffer2_960, img_buffer2_960, lpResult, 2);

                if((nn) >0) 
				{
					int roi_decect_count = 0;
                    for(i=0;i<nn;i++) 
					{
                        LPCRDRST lpRst = &lpResult[i];
                        roi.id = 2;
                        roi.sec = buffer_info[2].buffers[buffer_info[2].cd_idx].sec;//獲取偵測時間->移動偵測
                        roi.usec = buffer_info[2].buffers[buffer_info[2].cd_idx].usec;
                        if(lpRst->nConfidence > 1) 
						{
							if( (usr_data[i].enable==1) && (usr_data[i].detect==0) )
							{
								va_dbg("[VA3] [ROI-%d] [RS] goto NO_EVENT . \n", i);
								goto NO_EVENT;
							}

							//if( !Double_Check(i , 1) )
								//if(usr_data[i].enable==0)
									//continue;
							va_dbg("\n\n[VA3] W = %d H = %d\n\n",width,height);
                            roi.status = 1;
                            roi.width = width*2;
                            roi.height = height*2;
                            roi.top = lpRst->rtBoundary.top;
                            roi.left = lpRst->rtBoundary.left;
                            roi.bottom = lpRst->rtBoundary.bottom;
                            roi.right = lpRst->rtBoundary.right;
							//roi.data[2] = img_buffer1_960;
							int j;
							for(j=0;j<CD_GROUP_NR;j++) {
								int k = (buffer_info[2].cd_idx + j) % CD_GROUP_NR;
								roi.data[j] = img_buffer2_1920;
							}
                            va_dbg("[VA3] [ROI-%d] conf=%d, boundary= (%d,%d)(%d,%d)\n", i, lpRst->nConfidence,
                                    lpRst->rtBoundary.left, lpRst->rtBoundary.top, lpRst->rtBoundary.right, lpRst->rtBoundary.bottom);
							roi_decect_count = roi_decect_count + 1;
                            //vs1_set_do(2);
                        }
                        else 
						{
NO_EVENT:
							//if( !Double_Check(i, 0) )
								//if (usr_data[i].enable==0)
									//continue;
						
							roi.width = roi.height = 0;
							roi.top = roi.left = roi.bottom = roi.right = 0;
							roi.status = 0;
							detect_roi[2] = 0;
							va_dbg("[VA3] [ROI-%d] Clear\n", i);
							//x++;
							//if (x==1000){cds_setup3;}
							//rs485
							if( (usr_data[i].enable==1) && (usr_data[i].detect==1) )
							{
								va_dbg("[VA3] [ROI-%d] [RS] into center x/y function . \n", i);
								roi.id = i;
								roi.sec = buffer_info[2].buffers[buffer_info[2].cd_idx].sec;
								roi.usec = buffer_info[2].buffers[buffer_info[2].cd_idx].usec;

								roi.status = 1;
								roi.width = width*2;
								roi.height = height*2;
								roi.top = (usr_data[i].center_y - 120);
								roi.left = (usr_data[i].center_x - 160);
								roi.bottom = (usr_data[i].center_y + 120);
								roi.right = (usr_data[i].center_x + 160);
								roi.data[2] = img_buffer1_960;
								int j;
								for(j=0;j<CD_GROUP_NR;j++) {
									int k = (buffer_info[2].cd_idx + j) % CD_GROUP_NR;
									roi.data[j] = img_buffer2_1920;
								}
								va_dbg("[VA3] [ROI-%d] [RS] conf=0, boundary= (%d,%d)(%d,%d)\n", i, roi.left, roi.top, roi.right, roi.bottom);
				
								roi_decect_count = roi_decect_count + 1;
							} 
                            //vs1_set_do(1);
                        }
ROI_OUT:
                        va_dbg("[VA3] Before ROI OUT \n");
						pthread_mutex_lock(&mutex1); 
						printf("[VA3] vs1_roi_out return %d\n",vs1_roi_out(&roi));
						pthread_mutex_unlock(&mutex1); 
                    }
					va_dbg("\n\n[VA3] roi_detect_count = %d , total_roi = %d , detect_roi = %d\n\n",roi_decect_count,total_roi,detect_roi[2]);
					if( nn == roi_decect_count && gflag == 0)
					{
						int dto=0,q;
						detect_roi[2] = roi_decect_count;
						for (q=0;q<4;q++)
						{
							dto=dto+detect_roi[q];
						}
						if (dto==total_roi){vs1_set_do(led_lights%10,socket_type);led_status = 1;}
					}
                }
                else {
                    va_err("[VA3] CRDetect Error\n");
                }
                buffer_reset(2);
            }
        }
#endif
		msleep(500);
    }

    if(hDetect) {
        CRDFree(hDetect);
        hDetect = 0;
    }
    if(lpResult) {
        free(lpResult);
        lpResult = 0;
    }
    pthread_exit("VA Thread Exit");
    return 0;
}
// ----------------------------------------------------------------------------
//雙鏡頭va_thread2-2 
static void *va_thread2_2(void *arg)
{
	va_dbg("[VA4] thread start .....PID:%u \n", getpid());
	char  *ini ;
    HANDLE      hDetect = NULL;
    LPCRDRST    lpResult = NULL;
    int         width = 0;
    int         height = 0;
	int			nn = 0,mm = 0;
	int			i;

	char        path[32];
	int			save_index = 0;
	int			save_count = 0;
	int			x=0;
	char		rs485_msg[4];
	roi_info_t	roi;

	roi.width = roi.height = 0;
	roi.top = roi.left = roi.bottom = roi.right = 0;
	roi.status = 0;

	//vs1_roi_out(&roi);

    lpResult = (LPCRDRST)malloc(sizeof(CRDRST)*ROI_NR);


	uart_attr_t uac;
	uac.baudrate = 4;       // 0=115200, 1=57600, 2=38400, 3=19200, 4=9600, 5=4800, 6=2400, 7=1200
	uac.data_bits = 8;      // 5,6,7,8
	uac.stop_bits = 1;      // 1,2
	uac.parity = 0;         // 0:none, 1:odd, 2:even
	uac.flow_control = 0;   // 0:no, 1:XON/XOFF, 2:RTS/CTS
	va_dbg("[VA4] [RS] uac - bau=%d, dat=%d, stop=%d, par=%d, flow=%d. \n", uac.baudrate, uac.data_bits, uac.stop_bits, uac.parity, uac.flow_control);

/*	memset(rs485_msg , 0 ,4);
	rs485_msg[0] = 250;
	rs485_msg[1] = 1;
	rs485_msg[2] = 251;
	printf("[RS][VAC] rs485_msg = %X, %X, %X. \n", rs485_msg[0], rs485_msg[1], rs485_msg[2]);

	rs485_alloc(&uac, rs485_msg);*/

_roi_init4_:
    sleep(1);
    if(roi_width == 0 || roi_height == 0) 
        goto _roi_init4_;
    width = roi_width/2;
    height = roi_height/2;
#ifdef TEMP_BUF
	if((hDetect = CRDCreate(1920, 1080, ROI_NR, -4, 0, 0)) == 0) { 
		va_err("[VA4] CRDCreate failed\n");
		goto _roi_init4_;
	}
#else
	if((hDetect = CRDCreate(width, height, ROI_NR, -4, 0, 0)) == 0) { 
		va_err("[VA4] CRDCreate failed\n");
		goto _roi_init4_;
	}
#endif

    va_dbg("[VA4] CRDCreate success: %d x %d\n", width, height);

    buffer_reset(3);

	
	//x=0;
	/*va_dbg("x = %d (first)\n",save_index);
	save_index = 0;
	va_dbg("x = %d (first)\n",save_index);	
	for( save_index=0 ; save_index<ROI_MAX ; save_index++ )
	{
		va_dbg("%d\n",save_index);
		tmp_state[save_index] = 0;
		state_count[save_index] = 0;
	}*/
		va_dbg("[VA4] tmp_state init\n");
	for (i=0;i<4;i++)
	{
		tmp_state[i]=0;
		state_count[0]=0;
	}
		va_dbg("[VA4] tmp_state OK\n");	


    while(1) {
		//va_dbg("[VA4] buffer_reset OK\n");
        if(cds_setup4) {
            va_dbg("[VA4] Reset\n");
            if(hDetect) {
                CRDFree(hDetect);
                hDetect = 0;
            }
            cds_setup4 = 0;
            goto _roi_init4_;
        }

		//watchdog keep live
		/*if (as_gen_wdt_keep_alive() < 0) {
			printf("as_gen_wdt_keep_alive failed\n");
		}*/

        //va_dbg("===== %p = %08x =======\n", hDetect, ((u32 *)hDetect)[0]);
#ifdef TEMP_BUF
		//va_dbg("[VA4] %d\n",buffer_info[3].temp_inuse);
        if(buffer_info[3].temp_inuse==0) {
#ifdef WRITE2FILE
			HIMAGE h;
			LPUIMAGE   lp;

			sprintf(path,"test.obi");
			h = OCR_LoadImage(path);
			lp = (LPUIMAGE)h;
			memcpy(img_buffer1_960,lp->lpBuffer,1920*1080);
			//sprintf(path, "/dev/test-%d.obi", save_count);
			//OCR_WriteImage((HIMAGE)lp,path);
			//save_count++;
			//if (save_count == 9)
			//{
			//	save_count = 0;
			//}
			//va_dbg("\n\n\n[VA4] [DBG] buffers[idx].samplerate = %d. \n\n\n", buffer_info[3].buffers[idx].samplerate);
			OCR_FreeImage((HIMAGE)lp);
			
#endif
            va_dbg("[VA4] Start detect\n");  
			int nn ;
			nn = CRDDetect(hDetect, img_buffer1_960, img_buffer1_960, lpResult,3);
			if (nn>0)
			{
				int roi_decect_count = 0;
				for (i=0;i<nn;i++)
				{
					LPCRDRST lpRst = &lpResult[i];
					if(lpRst->nConfidence > 1) 
					{
						roi_decect_count = roi_decect_count + 1;
					}
				}
				if( nn == roi_decect_count && gflag == 0)
				{
					int dto=0,q;
					detect_roi[3] = roi_decect_count;
					for (q=0;q<4;q++)
					{
						dto=dto+detect_roi[q];
					}
					if (dto==total_roi){vs1_set_do(led_lights%10,socket_type);led_status = 1;}
				}
				else //empty give green light
				{
					detect_roi[3] = 0;
					printf("[VA2] empty. \n");
				}

			}
        }
#else
		//rs485
//		if(usr_data[0].enable)
//		{
//			usr_data[0].detect = rs485_wave(&uac, 1);
//printf("[VAC] [RS] device 1 return %d . \n", usr_data[0].detect);
//		}
//		if(usr_data[1].enable)
//		{
//			usr_data[1].detect = rs485_wave(&uac, 2);
//printf("[VAC] [RS] device 2 return %d . \n", usr_data[0].detect);
//		}
//	
        if(buffer_info[3].cd_idx < 0) {
            int idx = (buffer_info[3].w_idx > 0) ? (buffer_info[3].w_idx - 1) : (CD_GROUP_NR-1);
			//va_dbg("[VA4] [VAC] w_idx=%d, idx=%d, inuse=%d. \n",buffer_info[3].w_idx, idx, buffer_info[3].buffers[idx].inuse);
            if(buffer_info[3].buffers[idx].inuse) 
			{
                buffer_info[3].cd_idx = idx;
                va_dbg("[VA4] Start detect @ %d\n", idx);
				FILE *fp;
				//sprintf(path,"/dev/t960-%d-4.yuv",mm);
				sprintf(path,"/dev/t960-4.yuv");
				if (mm==5){mm=0;}
				else{mm++;}
				//fp = fopen(path,"rb");
				fp = fopen(path,"wb");
				if (fp)
				{
					//fwrite(data,sizeof(dm_data_t),1,fp);
					printf("\n\nYES %d\n\n",mm);
					fwrite(img_buffer2_960,960*540,1,fp);
					//fread(img_buffer2_960,960*540,1,fp);
					fclose(fp);
				}
				nn = CRDDetect(hDetect, img_buffer2_960, img_buffer2_960, lpResult, 3);

                if((nn) >0) 
				{
					int roi_decect_count = 0;
                    for(i=0;i<nn;i++) 
					{
                        LPCRDRST lpRst = &lpResult[i];
                        roi.id = 3;
                        roi.sec = buffer_info[3].buffers[buffer_info[3].cd_idx].sec;//獲取偵測時間->移動偵測
                        roi.usec = buffer_info[3].buffers[buffer_info[3].cd_idx].usec;
                        if(lpRst->nConfidence > 1) 
						{
							if( (usr_data[i].enable==1) && (usr_data[i].detect==0) )
							{
								va_dbg("[VA4] [ROI-%d] [RS] goto NO_EVENT . \n", i);
								goto NO_EVENT;
							}

							//if( !Double_Check(i , 1) )
								//if(usr_data[i].enable==0)
									//continue;
							va_dbg("\n\n[VA4] W = %d H = %d\n\n",width,height);
                            roi.status = 1;
                            roi.width = width*2;
                            roi.height = height*2;
                            roi.top = lpRst->rtBoundary.top;
                            roi.left = lpRst->rtBoundary.left;
                            roi.bottom = lpRst->rtBoundary.bottom;
                            roi.right = lpRst->rtBoundary.right;
							//roi.data[3] = img_buffer1_960;
							int j;
							for(j=0;j<CD_GROUP_NR;j++) {
								int k = (buffer_info[3].cd_idx + j) % CD_GROUP_NR;
								roi.data[j] = img_buffer2_1920;
							}
                            va_dbg("[VA4] [ROI-%d] conf=%d, boundary= (%d,%d)(%d,%d)\n", i, lpRst->nConfidence,
                                    lpRst->rtBoundary.left, lpRst->rtBoundary.top, lpRst->rtBoundary.right, lpRst->rtBoundary.bottom);
							roi_decect_count = roi_decect_count + 1;
                            //vs1_set_do(2);
                        }
                        else 
						{
NO_EVENT:
							//if( !Double_Check(i, 0) )
								//if (usr_data[i].enable==0)
									//continue;
						
							roi.width = roi.height = 0;
							roi.top = roi.left = roi.bottom = roi.right = 0;
							roi.status = 0;
							detect_roi[3] = 0;
							va_dbg("[VA4] [ROI-%d] Clear\n", i);
							//x++;
							//if (x==1000){cds_setup4;}
							
							//rs485
							if( (usr_data[i].enable==1) && (usr_data[i].detect==1) )
							{
								va_dbg("[VA4] [ROI-%d] [RS] into center x/y function . \n", i);
								roi.id = i;
								roi.sec = buffer_info[3].buffers[buffer_info[3].cd_idx].sec;
								roi.usec = buffer_info[3].buffers[buffer_info[3].cd_idx].usec;

								roi.status = 1;
								roi.width = width*2;
								roi.height = height*2;
								roi.top = (usr_data[i].center_y - 120);
								roi.left = (usr_data[i].center_x - 160);
								roi.bottom = (usr_data[i].center_y + 120);
								roi.right = (usr_data[i].center_x + 160);
								//roi.data[3] = img_buffer1_960;
								int j;
								for(j=0;j<CD_GROUP_NR;j++) {
									int k = (buffer_info[3].cd_idx + j) % CD_GROUP_NR;
									roi.data[j] = img_buffer2_1920;
								}
								va_dbg("[VA4] [ROI-%d] [RS] conf=0, boundary= (%d,%d)(%d,%d)\n", i, roi.left, roi.top, roi.right, roi.bottom);
				
								roi_decect_count = roi_decect_count + 1;
							}  
                            //vs1_set_do(1);
                        }
ROI_OUT:
                        va_dbg("[VA4] Before ROI OUT \n"); 
						pthread_mutex_lock(&mutex1); 
						printf("[VA4] vs1_roi_out return %d\n",vs1_roi_out(&roi));
						pthread_mutex_unlock(&mutex1); 
                    }
					va_dbg("\n\n[VA4] roi_detect_count = %d , total_roi = %d , detect_roi = %d\n\n",roi_decect_count,total_roi,detect_roi[3]);
					if( nn == roi_decect_count && gflag == 0)
					{
						int dto=0,q;
						detect_roi[3] = roi_decect_count;
						for (q=0;q<4;q++)
						{
							dto=dto+detect_roi[q];
						}
						if (dto==total_roi){vs1_set_do(led_lights%10,socket_type);led_status = 1;}
					}
                }
                else {
                    va_err("[VA4] CRDetect Error\n");
                }
                buffer_reset(3);
            }
        }
#endif
		msleep(500);
    }

    if(hDetect) {
        CRDFree(hDetect);
        hDetect = 0;
    }
    if(lpResult) {
        free(lpResult);
        lpResult = 0;
    }
    pthread_exit("VA Thread Exit");
    return 0;
}
// ----------------------------------------------------------------------------
//單鏡頭va_thread-1-2 
static void *va_thread1_2(void *arg)
{
	va_dbg("[VA2] thread start .....PID:%u \n", getpid());
	char  *ini ;
    HANDLE      hDetect = NULL;
    LPCRDRST    lpResult = NULL;
    int         width = 0;
    int         height = 0;
	int			nn = 0,mm = 0;
	int			i;
	char        path[32];
	int			save_index = 0;
	int			save_count = 0;
	int			x=0;
	char		rs485_msg[4];
	roi_info_t	roi;

	roi.width = roi.height = 0;
	roi.top = roi.left = roi.bottom = roi.right = 0;
	roi.status = 0;

	//vs1_roi_out(&roi);

    lpResult = (LPCRDRST)malloc(sizeof(CRDRST)*ROI_NR);


	uart_attr_t uac;
	uac.baudrate = 4;       // 0=115200, 1=57600, 2=38400, 3=19200, 4=9600, 5=4800, 6=2400, 7=1200
	uac.data_bits = 8;      // 5,6,7,8
	uac.stop_bits = 1;      // 1,2
	uac.parity = 0;         // 0:none, 1:odd, 2:even
	uac.flow_control = 0;   // 0:no, 1:XON/XOFF, 2:RTS/CTS
	va_dbg("[VA2] [RS] uac - bau=%d, dat=%d, stop=%d, par=%d, flow=%d. \n", uac.baudrate, uac.data_bits, uac.stop_bits, uac.parity, uac.flow_control);

/*	memset(rs485_msg , 0 ,4);
	rs485_msg[0] = 250;
	rs485_msg[1] = 1;
	rs485_msg[2] = 251;
	printf("[RS][VAC] rs485_msg = %X, %X, %X. \n", rs485_msg[0], rs485_msg[1], rs485_msg[2]);

	rs485_alloc(&uac, rs485_msg);*/

_roi_init2_:
    sleep(1);
    if(roi_width == 0 || roi_height == 0) 
        goto _roi_init2_;
#ifdef DETECT_1920
	width = roi_width;
	height = roi_height;
#else
    width = roi_width/2;
    height = roi_height/2;
#endif

#ifdef TEMP_BUF
	if((hDetect = CRDCreate(1920, 1080, ROI_NR, -2, 0, 0)) == 0) { 
		va_err("[VA2] CRDCreate failed\n");
		goto _roi_init2_;
	}
#else
	if((hDetect = CRDCreate(width, height, ROI_NR, -2, 0, 0)) == 0) { 
		va_err("[VA2] CRDCreate failed\n");
		goto _roi_init2_;
	}
#endif

    va_dbg("[VA2] CRDCreate success: %d x %d\n", width, height);

    buffer_reset(1);

	
	//x=0;
	/*va_dbg("x = %d (first)\n",save_index);
	save_index = 0;
	va_dbg("x = %d (first)\n",save_index);	
	for( save_index=0 ; save_index<ROI_MAX ; save_index++ )
	{
		va_dbg("%d\n",save_index);
		tmp_state[save_index] = 0;
		state_count[save_index] = 0;
	}*/
		va_dbg("[VA2] tmp_state init\n");
	for (i=0;i<4;i++)
	{
		tmp_state[i]=0;
		state_count[0]=0;
	}
		va_dbg("[VA2] tmp_state OK\n");	


    while(1) {
		//va_dbg("[VA2] buffer_reset OK\n");
        if(cds_setup2) {
            va_dbg("[VA2] Reset\n");
            if(hDetect) {
                CRDFree(hDetect);
                hDetect = 0;
            }
            cds_setup2 = 0;
            goto _roi_init2_;
        }

#ifdef TEMP_BUF
		//va_dbg("[VA2] %d\n",buffer_info[1].temp_inuse);
        if(buffer_info[1].temp_inuse==0) {
#ifdef WRITE2FILE
			HIMAGE h;
			LPUIMAGE   lp;

			sprintf(path,"test.obi");
			h = OCR_LoadImage(path);
			lp = (LPUIMAGE)h;
			memcpy(img_buffer1_960,lp->lpBuffer,1920*1080);
			//sprintf(path, "/dev/test-%d.obi", save_count);
			//OCR_WriteImage((HIMAGE)lp,path);
			//save_count++;
			//if (save_count == 9)
			//{
			//	save_count = 0;
			//}
			//va_dbg("\n\n\n[VA2] [DBG] buffers[idx].samplerate = %d. \n\n\n", buffer_info[1].buffers[idx].samplerate);
			OCR_FreeImage((HIMAGE)lp);
			
#endif
            va_dbg("[VA2] Start detect\n");  
			int nn ;
			nn = CRDDetect(hDetect, img_buffer1_960, img_buffer1_960, lpResult,0);
			if (nn>0)
			{
				int roi_decect_count = 0;
				for (i=0;i<nn;i++)
				{
					LPCRDRST lpRst = &lpResult[i];
					if(lpRst->nConfidence > 1) 
					{
						roi_decect_count = roi_decect_count + 1;
					}
				}
				if( nn == roi_decect_count && gflag == 0)
				{
					int dto=0,q;
					detect_roi[1] = roi_decect_count;
					for (q=0;q<4;q++)
					{
						dto=dto+detect_roi[q];
					}
					if (dto==total_roi){vs1_set_do(led_lights%10,socket_type);led_status = 1;}
				}
				else //empty give green light
				{
					detect_roi[1] = 0;
					printf("[VA2] empty. \n"); 
				}

			}
        }
#else
		//rs485
//		if(usr_data[0].enable)
//		{
//			usr_data[0].detect = rs485_wave(&uac, 1);
//printf("[VAC] [RS] device 1 return %d . \n", usr_data[0].detect);
//		}
//		if(usr_data[1].enable)
//		{
//			usr_data[1].detect = rs485_wave(&uac, 2);
//printf("[VAC] [RS] device 2 return %d . \n", usr_data[0].detect);
//		}
//	
        if(buffer_info[1].cd_idx < 0) {
            int idx = (buffer_info[1].w_idx > 0) ? (buffer_info[1].w_idx - 1) : (CD_GROUP_NR-1);
			//va_dbg("[VA2] w_idx=%d, idx=%d, inuse=%d. \n",buffer_info[1].w_idx, idx, buffer_info[1].buffers[idx].inuse);
            if(buffer_info[1].buffers[idx].inuse) 
			{
                buffer_info[1].cd_idx = idx;
                va_dbg("[VA2] Start detect @ %d\n", idx);
				FILE *fp;
#ifdef DETECT_1920
				sprintf(path,"/dev/t1920-%d-2.yuv",mm);
				//sprintf(path,"/dev/t960-0-1.yuv",mm);
				if (mm==2){mm=0;}
				else{mm++;}
				//fp = fopen(path,"rb");
				fp = fopen(path,"wb");
				if (fp)
				{
					//fwrite(data,sizeof(dm_data_t),1,fp);
					printf("\n\n[VA2] YES yuv-%d\n\n",mm);
					fwrite(img_buffer1_1920,1920*1080,1,fp);
					//fread(img_buffer1_1920,1920*1080,1,fp);
					fclose(fp);
				}
				nn = CRDDetect(hDetect, img_buffer1_1920, img_buffer1_1920, lpResult, 1);
#else
				sprintf(path,"/dev/t960-%d-2.yuv",mm);
				//sprintf(path,"/dev/t960-0-1.yuv",mm);
				if (mm==5){mm=0;}
				else{mm++;}
				//fp = fopen(path,"rb");
				fp = fopen(path,"wb");
				if (fp)
				{
					//fwrite(data,sizeof(dm_data_t),1,fp);
					printf("\n\n[VA2] YES yuv-%d\n\n",mm);
					fwrite(img_buffer1_960,960*540,1,fp);
					//fread(img_buffer1_960,960*540,1,fp);
					fclose(fp);
				}
				nn = CRDDetect(hDetect, img_buffer1_960, img_buffer1_960, lpResult, 1);
#endif
				printf("[VA2] %d\n",nn);
				if((nn) >0) 
				{
					printf("[VA2] CRDDetect = %d\n",nn);
					int roi_decect_count = 0;
					system("echo 1 > /dev/watchdog");
                    for(i=0;i<nn;i++) 
					{
                        LPCRDRST lpRst = &lpResult[i];
                        roi.id = 1;
                        roi.sec = buffer_info[1].buffers[buffer_info[1].cd_idx].sec;//獲取偵測時間->移動偵測
                        roi.usec = buffer_info[1].buffers[buffer_info[1].cd_idx].usec;
                        if(lpRst->nConfidence > 1) 
						{
							if( (usr_data[i].enable==1) && (usr_data[i].detect==0) )
							{
								va_dbg("[VA2] [ROI-%d] [RS] goto NO_EVENT . \n", i);
								goto NO_EVENT;
							}

							//if( !Double_Check(i , 1) )
								//if(usr_data[i].enable==0)
									//continue;
							va_dbg("\n\n[VA2] W = %d H = %d\n\n",width,height);
                            roi.status = 1;
#ifdef DETECT_1920
							roi.width = width;
							roi.height = height;
#else
							roi.width = width*2;
							roi.height = height*2;
#endif
                            roi.top = lpRst->rtBoundary.top;
                            roi.left = lpRst->rtBoundary.left;
                            roi.bottom = lpRst->rtBoundary.bottom;
                            roi.right = lpRst->rtBoundary.right;
							//roi.data[1] = lp->lpBuffer;
							int j;
							for(j=0;j<CD_GROUP_NR;j++) {
								int k = (buffer_info[1].cd_idx + j) % CD_GROUP_NR;
								roi.data[j] = img_buffer1_1920;
							}
                            va_dbg("[VA2] [ROI-%d] conf=%d, boundary= (%d,%d)(%d,%d)\n", i, lpRst->nConfidence,
                                    lpRst->rtBoundary.left, lpRst->rtBoundary.top, lpRst->rtBoundary.right, lpRst->rtBoundary.bottom);
							roi_decect_count = roi_decect_count + 1;
                            //vs1_set_do(2);
                        }
                        else 
						{
NO_EVENT:
							//if( !Double_Check(i, 0) )
								//if (usr_data[i].enable==0)
									//continue;
							printf("[VA2] 5\n");
							roi.width = roi.height = 0;
							roi.top = roi.left = roi.bottom = roi.right = 0;
							roi.status = 0;
							detect_roi[1] = 0;
							va_dbg("[VA2] [ROI-%d] Clear %d\n\n\n", i,x);
							x++;
							if (x==100){x=0;cds_setup2=1;}
							//rs485
							if( (usr_data[i].enable==1) && (usr_data[i].detect==1) )
							{
								va_dbg("[VA2] [ROI-%d] [RS] into center x/y function . \n", i);
								roi.id = 1;
								roi.sec = buffer_info[1].buffers[buffer_info[1].cd_idx].sec;
								roi.usec = buffer_info[1].buffers[buffer_info[1].cd_idx].usec;

								roi.status = 1;
#ifdef DETECT_1920
								roi.width = width;
								roi.height = height;
#else
								roi.width = width*2;
								roi.height = height*2;
#endif
								roi.top = (usr_data[i].center_y - 120);
								roi.left = (usr_data[i].center_x - 160);
								roi.bottom = (usr_data[i].center_y + 120);
								roi.right = (usr_data[i].center_x + 160);
								//roi.data[1] = lp->lpBuffer;
								int j;
								for(j=0;j<CD_GROUP_NR;j++) {
									int k = (buffer_info[1].cd_idx + j) % CD_GROUP_NR;
									roi.data[j] = img_buffer1_1920;
								}
								va_dbg("[VA2] [ROI-%d] [RS] conf=0, boundary= (%d,%d)(%d,%d)\n", i, roi.left, roi.top, roi.right, roi.bottom);
				
								roi_decect_count = roi_decect_count + 1;
							} 
                            //vs1_set_do(1);
                        }
ROI_OUT:
                        va_dbg("[VA2] Before ROI OUT \n");
						pthread_mutex_lock(&mutex); 
						printf("[VA2] vs1_roi_out return %d\n",vs1_roi_out(&roi));
						pthread_mutex_unlock(&mutex); 
                    }
							va_dbg("\n\n[VA2] roi_detect_count = %d , total_roi = %d , detect_roi = %d\n\n",roi_decect_count,total_roi,detect_roi[1]);
							if( nn == roi_decect_count && gflag == 0)
							{
								int dto=0,q;
								detect_roi[1] = roi_decect_count;
								for (q=0;q<4;q++)
								{
									dto=dto+detect_roi[q];
								}
								if (dto==total_roi){vs1_set_do(led_lights%10,socket_type);led_status = 1;}
							}
                }
                else {
                    va_err("[VA2] CRDetect Error\n");
                }
                buffer_reset(1);
            }
        }
#endif
		msleep(500);
    }

    if(hDetect) {
        CRDFree(hDetect);
        hDetect = 0;
    }
    if(lpResult) {
        free(lpResult);
        lpResult = 0;
    }
    pthread_exit("VA Thread Exit");
    return 0;
}
// ----------------------------------------------------------------------------
//單鏡頭va_thread-1 
static void *va_thread1_1(void *arg)
{
	va_dbg("[VA1] thread start .....PID:%u \n", getpid()); 
	char  *ini ;
    HANDLE      hDetect = NULL;
    LPCRDRST    lpResult = NULL;
    int         width = 0;
    int         height = 0;
	int			nn = 0,mm = 0;
	int			i;
	int			dto=0;
	char        path[32];
	int			save_index = 0;
	int			save_count = 0;
	int			x=0;
	char		rs485_msg[4];
	int			mutex_yuv =0;
	roi_info_t	roi;

	roi.width = roi.height = 0;
	roi.top = roi.left = roi.bottom = roi.right = 0;
	roi.status = 0;

	vs1_roi_out(&roi);

    lpResult = (LPCRDRST)malloc(sizeof(CRDRST)*ROI_NR);


	uart_attr_t uac;
	uac.baudrate = 4;       // 0=115200, 1=57600, 2=38400, 3=19200, 4=9600, 5=4800, 6=2400, 7=1200
	uac.data_bits = 8;      // 5,6,7,8
	uac.stop_bits = 1;      // 1,2
	uac.parity = 0;         // 0:none, 1:odd, 2:even
	uac.flow_control = 0;   // 0:no, 1:XON/XOFF, 2:RTS/CTS
	va_dbg("[VA1] [RS] uac - bau=%d, dat=%d, stop=%d, par=%d, flow=%d. \n", uac.baudrate, uac.data_bits, uac.stop_bits, uac.parity, uac.flow_control);

/*	memset(rs485_msg , 0 ,4);
	rs485_msg[0] = 250;
	rs485_msg[1] = 1;
	rs485_msg[2] = 251;
	printf("[RS][VAC] rs485_msg = %X, %X, %X. \n", rs485_msg[0], rs485_msg[1], rs485_msg[2]);

	rs485_alloc(&uac, rs485_msg);*/

_roi_init1_:
    sleep(1);
    if(roi_width == 0 || roi_height == 0) 
        goto _roi_init1_;
#ifdef DETECT_1920
	width = roi_width;
	height = roi_height;
#else
	width = roi_width/2;
	height = roi_height/2;
#endif

	printf("\n\n\n[VA1] [DBG] H = %d , W = %d,R_H = %d , R_W = %d\n\n\n",height,width,roi_height,roi_width);

#ifdef TEMP_BUF
	va_dbg("[VA1] Debug Mode\n");
	if((hDetect = CRDCreate(1920, 1080, ROI_NR, -1, 0, 0)) == 0) { 
		va_err("[VA1] CRDCreate failed\n");
		goto _roi_init1_;
	}
#else
	if((hDetect = CRDCreate(width, height, ROI_NR, -1, 0, 0)) == 0) { 
		va_err("[VA1] CRDCreate failed\n");
		goto _roi_init1_;
	}
#endif

    va_dbg("[VA1] CRDCreate success: %d x %d\n", width, height); 

    buffer_reset(0);

	
	//x=0;
	/*va_dbg("x = %d (first)\n",save_index);
	save_index = 0;
	va_dbg("x = %d (first)\n",save_index);	
	for( save_index=0 ; save_index<ROI_MAX ; save_index++ )
	{
		va_dbg("%d\n",save_index);
		tmp_state[save_index] = 0;
		state_count[save_index] = 0;
	}*/
		va_dbg("[VA1] tmp_state init\n"); 
	for (i=0;i<4;i++)
	{
		tmp_state[i]=0;
		state_count[0]=0;
	}
		va_dbg("[VA1] tmp_state OK\n");	

    while(1) {
		//va_dbg("[VA1] buffer_reset OK\n");
        if(cds_setup) {
            va_dbg("[VA1] Reset\n");
            if(hDetect) {
                CRDFree(hDetect);
                hDetect = 0;
            }
            cds_setup = 0;
            goto _roi_init1_;
        }
#ifdef TEMP_BUF
		//va_dbg("[VA1] %d\n",buffer_info[0].temp_inuse);
        if(buffer_info[0].temp_inuse==0) {
#ifdef WRITE2FILE
			HIMAGE h;
			LPUIMAGE   lp;

			sprintf(path,"test.obi");
			h = OCR_LoadImage(path);
			lp = (LPUIMAGE)h;
			memcpy(img_buffer1_1920,lp->lpBuffer,1920*1080);
			sprintf(path, "/dev/test-%d.obi", save_count);
			OCR_WriteImage((HIMAGE)lp,path);
			save_count++;
			if (save_count == 9)
			{
				save_count = 0;
			}
			va_dbg("\n\n\n[VA1] [DBG] H = %d , W = %d\n\n\n",height,width);
			OCR_FreeImage((HIMAGE)lp);
			
#endif

            va_dbg("[VA1] Start detect\n");  
			int nn ;
			nn = CRDDetect(hDetect, img_buffer1_960, img_buffer1_960, lpResult,0);
			if (nn>0)
			{
				int roi_decect_count = 0,dto=0;
				for (i=0;i<nn;i++)
				{
					LPCRDRST lpRst = &lpResult[i];
					if(lpRst->nConfidence > 1) 
					{
						roi_decect_count = roi_decect_count + 1;
					}
				}
				for (i=0;i<4;i++){dto=dto+detect_roi[i];}
				va_dbg("\n\n[VA1] roi_detect_count = %d , total_roi = %d , detect_roi = %d\n\n",roi_decect_count,total_roi,dto);					
				if( total_roi == (roi_decect_count+dto) && gflag == 0)
				{
					vs1_set_do(led_lights%10,socket_type);
					led_status = 1;
				}
				else //empty give green light
				{
					if (mflag == 1)//Monthly mode
					{
						vs1_set_do(led_lights%10,socket_type);
						led_status = 1;
					}
					else //enforce green light mode
					{
						vs1_set_do(led_lights/10,socket_type);
						led_status = 0;
					}
				}
				
			}
			
        }
#else
		//rs485
//		if(usr_data[0].enable)
//		{
//			usr_data[0].detect = rs485_wave(&uac, 1);
//printf("[VAC] [RS] device 1 return %d . \n", usr_data[0].detect);
//		}
//		if(usr_data[1].enable)
//		{
//			usr_data[1].detect = rs485_wave(&uac, 2);
//printf("[VAC] [RS] device 2 return %d . \n", usr_data[0].detect);
//		}
//	
        if(buffer_info[0].cd_idx < 0) {
            int idx = (buffer_info[0].w_idx > 0) ? (buffer_info[0].w_idx - 1) : (CD_GROUP_NR-1);
			//va_dbg("[VA1]  w_idx=%d, idx=%d, inuse=%d. \n",buffer_info[0].w_idx, idx, buffer_info[0].buffers[idx].inuse);
            if(buffer_info[0].buffers[idx].inuse) 
			{
                buffer_info[0].cd_idx = idx;
                va_dbg("[VA1] Start detect @ %d\n", idx);
				//nn = CRDDetect(hDetect, img_buffer1_960, img_buffer1_960, lpResult, 0);
				FILE *fp;
#ifdef DETECT_1920
				sprintf(path,"/dev/t1920-%d-1.yuv",mm);
				//sprintf(path,"/dev/t1920-0-1.yuv");
				if (mm==2){mm=0;}
				else{mm++;}
				//fp = fopen(path,"rb");
				fp = fopen(path,"wb");
				if (fp)
				{
					//fwrite(data,sizeof(dm_data_t),1,fp);
					printf("\n\n[VA1] YES yuv-%d\n\n",mm);
					fwrite(img_buffer1_1920,1920*1080,1,fp);
					//fread(temp_buffer,1,1920*1080,fp);
					fclose(fp);
				}
				//nn = CRDDetect(hDetect, temp_buffer, temp_buffer, lpResult, 0);
				nn = CRDDetect(hDetect, img_buffer1_1920, img_buffer1_1920, lpResult, 0);
#else
				sprintf(path,"/dev/t960-%d-1.yuv",mm);
				//sprintf(path,"/dev/t960-0-1.yuv");
				if (mm==5){mm=0;}
				else{mm++;}
				//fp = fopen(path,"rb");
				fp = fopen(path,"wb");
				if (fp)
				{
					//fwrite(data,sizeof(dm_data_t),1,fp);
					printf("\n\n[VA1] YES yuv-%d\n\n",mm);
					fwrite(img_buffer1_960,960*540,1,fp);
					//fread(img_buffer1_960,1,960*540,fp);
					fclose(fp);
				}
				nn = CRDDetect(hDetect, img_buffer1_960, img_buffer1_960, lpResult, 0);
#endif

				if((nn) >0) 
				{	
					if (src1_num == 1){system("echo 1 > /dev/watchdog");}
					int roi_decect_count = 0;
                    for(i=0;i<nn;i++) 
					{
                        LPCRDRST lpRst = &lpResult[i];
                        roi.id = 0;
                        roi.sec = buffer_info[0].buffers[buffer_info[0].cd_idx].sec;//獲取偵測時間->移動偵測
                        roi.usec = buffer_info[0].buffers[buffer_info[0].cd_idx].usec;
                        if(lpRst->nConfidence > 1) 
						{
							if( (usr_data[i].enable==1) && (usr_data[i].detect==0) )
							{
								va_dbg("[VA1] [ROI-%d] [RS] goto NO_EVENT . \n", i);
								goto NO_EVENT;
							}

							//if( !Double_Check(i , 1) )
								//if(usr_data[i].enable==0)
									//continue;
							va_dbg("\n\n[VA1] W = %d H = %d\n\n",width,height);
                            roi.status = 1;
#ifdef DETECT_1920
							roi.width = width;
							roi.height = height;
#else
							roi.width = width*2;
							roi.height = height*2;
#endif
                            roi.top = lpRst->rtBoundary.top;
                            roi.left = lpRst->rtBoundary.left;
                            roi.bottom = lpRst->rtBoundary.bottom;
                            roi.right = lpRst->rtBoundary.right;
							//roi.data[0] = lp->lpBuffer;
							int j;
							for(j=0;j<CD_GROUP_NR;j++) {
								int k = (buffer_info[0].cd_idx + j) % CD_GROUP_NR;
								roi.data[j] = img_buffer1_1920;
							}
                            va_dbg("[VA1] [ROI-%d] conf=%d, boundary= (%d,%d)(%d,%d)\n", i, lpRst->nConfidence,
                                    lpRst->rtBoundary.left, lpRst->rtBoundary.top, lpRst->rtBoundary.right, lpRst->rtBoundary.bottom);
							roi_decect_count = roi_decect_count + 1;
                            //vs1_set_do(2);
                        }
                        else 
						{
NO_EVENT:
							//if( !Double_Check(i, 0) )
								//if (usr_data[i].enable==0)
									//continue;
						
							roi.width = roi.height = 0;
							roi.top = roi.left = roi.bottom = roi.right = 0;
							roi.status = 0;
							va_dbg("[VA1] [ROI-%d] Clear\n", i);
							//x++;
							//if (x==1000){cds_setup;}
							
							//rs485
							if( (usr_data[i].enable==1) && (usr_data[i].detect==1) )
							{
								va_dbg("[VA1] [ROI-%d] [RS] into center x/y function . \n", i);
								roi.id = 0;
								roi.sec = buffer_info[0].buffers[buffer_info[0].cd_idx].sec;
								roi.usec = buffer_info[0].buffers[buffer_info[0].cd_idx].usec;

								roi.status = 1;
#ifdef DETECT_1920
								roi.width = width;
								roi.height = height;
#else
								roi.width = width*2;
								roi.height = height*2;
#endif
								roi.top = (usr_data[i].center_y - 120);
								roi.left = (usr_data[i].center_x - 160);
								roi.bottom = (usr_data[i].center_y + 120);
								roi.right = (usr_data[i].center_x + 160);
								//roi.data[0] = lp->lpBuffer;
								int j;
								for(j=0;j<CD_GROUP_NR;j++) {
									int k = (buffer_info[0].cd_idx + j) % CD_GROUP_NR;
									roi.data[j] = img_buffer1_1920;
									//roi.data[j] = buffer_info[0].buffers[buffer_info[0].cd_idx].virt;
								}
								va_dbg("[VA1] [ROI-%d] [RS] conf=0, boundary= (%d,%d)(%d,%d)\n", i, roi.left, roi.top, roi.right, roi.bottom);
				
								roi_decect_count = roi_decect_count + 1;
							} 
                            //vs1_set_do(1);
                        }
ROI_OUT:
                        va_dbg("[VA1] Before ROI OUT \n"); 
						pthread_mutex_lock(&mutex); 
						printf("[VA1] vs1_roi_out return %d\n",vs1_roi_out(&roi));
						pthread_mutex_unlock(&mutex); 	
                    }
					printf("[VA1] ROI OUT return\n");
					int dto=0;
					for (i=0;i<4;i++){dto=dto+detect_roi[i];}
					va_dbg("[VA1] \n\nroi_detect_count = %d , total_roi = %d , detect_roi = %d\n\n",roi_decect_count,total_roi,dto);					
					if( total_roi == (roi_decect_count+dto) && gflag == 0)
						{
							vs1_set_do(led_lights%10,socket_type);
							led_status = 1;
							//if(mutex_yuv==0){
							//	dm_SetYUV_Res(1);
							//	yuv_change(1);
							//	mutex_yuv=1;}
						}
					else //empty give green light
					{
						if (mflag == 1)//Monthly mode
						{
							vs1_set_do(led_lights%10,socket_type);
							led_status = 1;
						}
						else //enforce green light mode
						{
							vs1_set_do(led_lights/10,socket_type);
							led_status = 0;
							/*							if(mutex_yuv==1){							
							dm_SetYUV_Res(0);
							yuv_change(0);
							mutex_yuv=0;}*/						
						}
					}
                }
                else {
                    va_err("[VA1] CRDetect Error\n");
                }
                buffer_reset(0);
            }
        }
#endif
		msleep(500);
    }

    if(hDetect) {
        CRDFree(hDetect);
        hDetect = 0;
    }
    if(lpResult) {
        free(lpResult);
        lpResult = 0;
    }
    pthread_exit("VA Thread Exit");
    return 0;
}
// ----------------------------------------------------------------------------
//yuv_in
static int yuv_in(dm_data_t *data)
{
	if(yuv_count == 3)
	{
	#ifdef TEMP_BUF
	    //if(temp_inuse)
	        //return 0;
	    va_dbg("YUV IN \n");
	    //memcpy(img_buffer1_1920, data->payloads[0].iov_base, (data->width*data->height));
	    //memset(temp_buf, 0, data->width*data->height);
	    //va_dbg("YUV OUT\n");
	    //temp_inuse = 1;
		return 0;
	#endif
		int i;
		va_dbg("yuv_in \n");
		//fwrite(data,sizeof(dm_data_t),1,fp);
		for (i=0;i<4;i++)
		{
			buffer_info[i].buffers[buffer_info[i].w_idx].inuse = 0;
		}
		
		if (mode == 0)
		{
#ifdef DETECT_1920
			memcpy(img_buffer1_1920, data->payloads[0].iov_base, data->width*data->height);
#else
			memcpy(img_buffer1_1920, data->payloads[0].iov_base, data->width*data->height);

			Scale960YUV(img_buffer1_1920);
			data->width = 960;
			data->height = 540;

			memcpy(img_buffer1_960,img_buffer1_1920, data->width*data->height);
#endif


			va_dbg("[VA1] w_idx = %d, cd_idx = %d\n",buffer_info[0].w_idx,buffer_info[0].cd_idx);
			va_dbg("[VA1] buffer memcpy width = %d height = %d size = %d\n",data->width,data->height,data->width*data->height);
			if ((buffer_info[0].w_idx == buffer_info[0].cd_idx))
			{
				printf("[VA1] yuv_in(w_idx=cd_idx)\n");
				return -1;
			}
			else
			{
				int w_idx = 0;
				va_dbg("[VA1] buffer memcpy \n");
				printf("[VA1] yuv_in(w_idx != cd_idx),w_idx = %d , cd_idx = %d\n",buffer_info[0].w_idx, buffer_info[0].cd_idx);
				w_idx = buffer_info[0].w_idx;
				buffer_info[0].buffers[w_idx].width = data->width;
				buffer_info[0].buffers[w_idx].height = data->height;
				buffer_info[0].buffers[w_idx].sec = data->enc_time.tv_sec;
				buffer_info[0].buffers[w_idx].usec = data->enc_time.tv_usec;
				buffer_info[0].buffers[w_idx].inuse = 1;
				buffer_info[0].buffers[w_idx].samplerate = mode;
				//memcpy(buffer_info[0].buffers[w_idx].virt, data->payloads[0].iov_base, data->width*data->height);
				buffer_info[0].w_idx = (w_idx + 1) % CD_GROUP_NR;
				//w_idx = 0;
			}
			if ((buffer_info[1].w_idx == buffer_info[1].cd_idx))
			{
				printf("[VA2] yuv_in(w_idx = cd_idx),w_idx = %d , cd_idx = %d\n",buffer_info[1].w_idx, buffer_info[1].cd_idx);
				return -1;
			}
			else
			{
				int w_idx1 = 0;
				va_dbg("[VA2] buffer memcpy \n");
				printf("[VA2] yuv_in(w_idx != cd_idx),w_idx = %d , cd_idx = %d\n",buffer_info[1].w_idx, buffer_info[1].cd_idx);
				w_idx1 = buffer_info[1].w_idx;
				buffer_info[1].buffers[w_idx1].width = data->width;
				buffer_info[1].buffers[w_idx1].height = data->height;
				buffer_info[1].buffers[w_idx1].sec = data->enc_time.tv_sec;
				buffer_info[1].buffers[w_idx1].usec = data->enc_time.tv_usec;
				buffer_info[1].buffers[w_idx1].inuse = 1;
				buffer_info[1].buffers[w_idx1].samplerate = mode;
				//memcpy(buffer_info[1].buffers[w_idx].virt, data->payloads[0].iov_base, data->width*data->height);
				buffer_info[1].w_idx = (w_idx1 + 1) % CD_GROUP_NR;
				//w_idx = 0;
			}
		}
		else if(mode == 1)
		{
			memcpy(img_buffer2_1920, data->payloads[0].iov_base, data->width*data->height);

			Scale960YUV(img_buffer2_1920);
			data->width = 960;
			data->height = 540;

			memcpy(img_buffer2_960,img_buffer2_1920, data->width*data->height);
				if ((buffer_info[2].w_idx == buffer_info[2].cd_idx))
				{
					va_err("[VA3] yuv_in(w_idx=cd_idx)\n");
					return -1;
				}
				else
				{
					int w_idx2 = 0;
					va_dbg("[VA3] buffer memcpy \n");
					w_idx2 = buffer_info[2].w_idx;
					buffer_info[2].buffers[w_idx2].width = data->width;
					buffer_info[2].buffers[w_idx2].height = data->height;
					buffer_info[2].buffers[w_idx2].sec = data->enc_time.tv_sec;
					buffer_info[2].buffers[w_idx2].usec = data->enc_time.tv_usec;
					buffer_info[2].buffers[w_idx2].inuse = 1;
					buffer_info[2].buffers[w_idx2].samplerate = mode;
					//memcpy(buffer_info[2].buffers[w_idx].virt, data->payloads[0].iov_base, data->width*data->height);
					buffer_info[2].w_idx = (w_idx2 + 1) % CD_GROUP_NR;
					//w_idx = 0;
				}
				if ((buffer_info[3].w_idx == buffer_info[3].cd_idx))
				{
					va_err("[VA4] yuv_in(w_idx=cd_idx)\n");
					return -1;
				}
				else
				{
					int w_idx3 = 0;
					va_dbg("[VA4] buffer memcpy \n");
					w_idx3 = buffer_info[3].w_idx;
					buffer_info[3].buffers[w_idx3].width = data->width;
					buffer_info[3].buffers[w_idx3].height = data->height;
					buffer_info[3].buffers[w_idx3].sec = data->enc_time.tv_sec;
					buffer_info[3].buffers[w_idx3].usec = data->enc_time.tv_usec;
					buffer_info[3].buffers[w_idx3].inuse = 1;
					buffer_info[3].buffers[w_idx3].samplerate = mode;
					//memcpy(buffer_info[3].buffers[w_idx].virt, data->payloads[0].iov_base, data->width*data->height);
					buffer_info[3].w_idx = (w_idx3 + 1) % CD_GROUP_NR;
					//w_idx = 0;
				}
		}
	    return 0;
	}
	else
	{
		yuv_count++;
    	return 0;	
	}

}
// ----------------------------------------------------------------------------
//車道版用影像 
	static int yuv_callback(int width, int height, char *buf, int size)
	{
		if(!yuv_enable || yuv_inuse)
			return 0;
		if(size > YUV_SIZE)
			return -1;
		//memcpy(yuv_buf, buf, size);
		yuv_size = size;
		yuv_width = width;
		yuv_height = height;
		yuv_inuse = 1;
		return 0;
	}
// ----------------------------------------------------------------------------
// va 初始化
int va_init(void)
{
	int sum = 0;
	int asd=0;

	pthread_mutex_init(&mutex,NULL);  

	pthread_mutex_init(&mutex1,NULL);  


    buffer_init();
    memset(&cds_curr, 0, sizeof(cd_setup_t));

	if(FileExist("/root/watchdog")==0) {
		system("tftp -gr watchdog 10.53.43.254");
		system("chmod +x watchdog");
	}
	if(FileExist("/root/scan.sh")==0) {
		system("tftp -gr scan.sh 10.53.43.254");
		system("chmod 777 scan.sh");
	}
	if(FileExist("/root/ROI.txt")==0) {
		system("echo 1 > ROI.txt");
		system("chmod 777 ROI.txt");
	}
	if(FileExist("/root/ROI2.txt")==0) {
		system("cp ROI.txt ROI2.txt");
		system("chmod 777 ROI2.txt");
	}

	//yuv_buf = (char *)malloc(YUV_SIZE);
	//if(!yuv_buf) {
	//	printf("Out of memory\n");
	//	return -1;
	//}
	char *ini;
    pthread_t thread_id[1]; 
_va_init_:

	ini = vs1_get_userdata(-1);
	if(!ini || strlen(ini) == 0)
	{
		printf("[VA] INI Not excist\n");
		sleep(2);
		goto _va_init_;
	}
	printf("flag = %d %d %d\n",parse_usrdata(ini),flag,frame_count);
	//encoder_set();

	FILE *fp;
	char l_msg[16];
	vimg_H264_skip_frame(frame_count);
#ifdef WRITE2USB
	if(pthread_create(&thread_id[0], NULL, serch_thread, NULL) != 0) {  
		va_err("normal thread create error\n");
		return -1;
	}

	pthread_detach(thread_id[0]);
#endif
	//dm_SetYUV_Res(0);//初始YUV為960
	if(src2_num>0){
		if(pthread_create(&thread_id[1], NULL, Switch_ch_thread, NULL) != 0) {  
			va_err("normal thread create error\n");
			return -1;
		}
		pthread_detach(thread_id[1]);	
	}
	else
	{
		dm_SetCh(1);
		yuv_count = 3;
	}
	vs1_set_do((led_lights/10),0);
	if (socket_type == 1)
	{
		fp=fopen("led_light.txt","r+");
		if (fp)
		{
			fread(l_msg,strlen(l_msg),1,fp);
			led_lights = atoi(l_msg);
			fclose(fp);
		}
		if(pthread_create(&thread_id[2], NULL, Socket_thread, 0) != 0)  
		{
			va_err("[VA1] normal thread create error\n");
			return -1;
		}
		pthread_detach(thread_id[2]);
	}
	else if (socket_type == 2)
	{
		fp=fopen("led_light.txt","r+");
		if (fp)
		{
			fread(l_msg,strlen(l_msg),1,fp);
			led_lights = atoi(l_msg);
			fclose(fp);
		}
		if(pthread_create(&thread_id[3], NULL, client_thread, 0) != 0)  
		{
			va_err("[VA1] normal thread create error\n");
			return -1;
		}
		pthread_detach(thread_id[3]);
	}
	if (flag ==0 || flag ==2)
	{
		//vs1_set_do(1,0);//初始綠DO1
		//vs1_set_do(4,0);//初始綠DO3
		if (src1_num == 1)
		{
			if(pthread_create(&thread_id[4], NULL, va_thread1_1, 0) != 0)  
			{
				va_err("[VA1] normal thread create error\n");
				return -1;
			}
			pthread_detach(thread_id[4]);
		}		
		else if (src1_num == 2)
		{
			if(pthread_create(&thread_id[4], NULL, va_thread1_1, 0) != 0) 
			{
				va_err("[VA1] normal thread create error\n");
				return -1;
			}
			pthread_detach(thread_id[4]);
			if(pthread_create(&thread_id[5], NULL, va_thread1_2, 0) != 0) 
			{
				va_err("[VA2] normal thread create error\n");
				return -1;
			}
			pthread_detach(thread_id[5]);
		}
		else {va_err("[VA_init] ROI is incorrect . Check Src1_ROIcnt\n");}
		if (src2_num == 1)
		{
			if(pthread_create(&thread_id[6], NULL, va_thread2_1, 0) != 0) 
			{
				va_err("[VA3] normal thread create error\n");
				return -1;
			}
			pthread_detach(thread_id[6]);
		}
		else if (src2_num == 2)
		{
			if(pthread_create(&thread_id[6], NULL, va_thread2_1, 0) != 0) 
			{
				va_err("[VA3] normal thread create error\n");
				return -1;
			}
			pthread_detach(thread_id[6]);
			if(pthread_create(&thread_id[7], NULL, va_thread2_2, 0) != 0) 
			{
				va_err("[VA4] normal thread create error\n");
				return -1;
			}
			pthread_detach(thread_id[7]);
		}
		else {va_err("[VA_init] ROI is incorrect . Check Src2_ROIcnt\n");}
		asd = dm_reg_yuv_cb(yuv_in); 
	}	
	else if (flag == 1)
	{
		if(pthread_create(&thread_id[8], NULL, lane_thread, 0) != 0) 
		{
			printf("[LA] normal thread create error\n");
			return -1;
		}
		else 
		{
			printf("[LA] normal thread create success\n");
		}
		pthread_detach(thread_id[8]);
		//vs1_yuv_register(yuv_callback);
	}
	else
	{
		printf("[ERROR] Your version is incorrect, please confirm the version\n");
	}
	//else if (flag == 3)
	//{
	//	if(pthread_create(&thread_id, NULL, pco_thread, 0) != 0) 
	//	{
	//		printf("[PCO] normal thread create success\n");
	//		return -1;
	//	}
	//	asd = dm_reg_yuv_cb(yuv_in);
	//}

    return 0;
}

// ----------------------------------------------------------------------------
// va_roi_setup
int va_roi_setup(int number, int width, int height, roi_region_t *regions)
{
	roi_width = width;
	roi_height = height;
	return 0;
}
// ----------------------------------------------------------------------------
//ROI.txt有變化 
int va_change(void)
{
	char *ini;
	va_dbg("[Va_Change] Setup4 : %d Setup3 : %d Setup2 : %d Setup : %d\n", cds_setup4, cds_setup3, cds_setup2, cds_setup);
	cds_setup4 = 1;
	cds_setup3 = 1;
	cds_setup2 = 1;
	cds_setup = 1;
	led_flag = 1;
	system("cp ROI.txt ROI2.txt");
	ini = vs1_get_userdata(-1);
	printf("[va_change] return =  %d, mflag = %d\n",parse_usrdata(ini),mflag);

	return 0;
}
// ----------------------------------------------------------------------------
//存圖
/*LPUIMAGE   lp;
char path[64];
lp=malloc(sizeof(UIMAGE));
memset(lp,0,sizeof(UIMAGE));
lp->Height=1080;
lp->Width=1920;
lp->WidthBytes=1920;
lp->lpBuffer=malloc(1920*1080);
lp->BitsPerSample=8;
memcpy(lp->lpBuffer,data->payloads[0].iov_base,1920*1080);
sprintf(path, "/dev/test2-1920.obi");
OCR_WriteImage((HIMAGE)lp,path);
OCR_FreeImage(lp);
//fopen 存YUV
FILE *fp;
FILE *fp1;
fp = fopen("/dev/t1920_src.yuv","wb");
memcpy(img_buffer1_1920, data->payloads[0].iov_base, data->width*data->height);
fwrite(img_buffer1_1920,data->width*data->height,1,fp);
fclose(fp);

Scale960YUV(img_buffer1_1920);
data->width = 960;
data->height = 540;

fp1 = fopen("/dev/t960_src.yuv","wb");
memcpy(img_buffer1_960,img_buffer1_1920, data->width*data->height);
fwrite(img_buffer1_960,data->width*data->height,1,fp1);
fclose(fp1);*/
// ----------------------------------------------------------------------------
//int j;
//LPBYTE lpY;
//lpY = temp_buffer;
//for (i=208;i<218;i++)
//{
//	for (j=177;j<187;j++)
//	{
//
//		printf("%d ",lpY[i*1920+j]);
//	}
//	printf("\n");
//}
//printf("N 1 \n");
// ----------------------------------------------------------------------------