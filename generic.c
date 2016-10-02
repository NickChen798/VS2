/*!
 *  @file       generic.c
 *  @version    0.01
 *  @author     James Chien <james@phototracq.com>
 */

#include "as_types.h"
 
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <termios.h>
#include <string.h>
#include <stdio.h>
// ----------------------------------------------------------------------------
// APIs
// ----------------------------------------------------------------------------
/*!
 * @brief   Initialization
 * @return  0 on success, otherwise failed
 */

/*!
 * @brief       generic library initialize
 *
 * @return      0 on success, otherwise < 0
 */
int memfd=0;
unsigned long *sys_base;    //0x200F0000
unsigned long *gpio0_base;  //0x20140000
unsigned long *gpio1_base;  //0x20150000
unsigned long *gpio2_base;  //0x20160000
unsigned long *gpio5_base;  //0x20190000
unsigned long *gpio6_base;  //0x201A0000
unsigned long *gpio7_base;  //0x201B0000
unsigned long *gpio8_base;  //0x201C0000

int as_gen_init(void) {
    memfd=open("/dev/mem",O_RDWR|O_SYNC);
    if(memfd==-1) {
        printf("Open /dev/mem fail , need su\n");
        return (-1);	
    }
    sys_base = (unsigned long *) mmap(NULL,512,PROT_READ|PROT_WRITE,MAP_SHARED,memfd,0x200F0000);
    if(sys_base==0) {
        printf("sys_base NULL pointer!\n");	 
        return (-1);
    }
    sys_base[0x11C/4]=0; // SPI1_CSN(GPIO5_7) DOUT1 //Next Version
    sys_base[0x010/4]=0; // SPI0_MOSI(GPIO1_5) DOUT2
    sys_base[0x014/4]=0; // SPI0_MISO(GPIO1_6) DOUT3
    sys_base[0x00C/4]=0; // SPI0_SCLK(GPIO1_4) DOUT4
    sys_base[0x0F0/4]=1; // NF_REN(GPIO8_2) RIN1  (I)
    sys_base[0x0FC/4]=1; // NF_CLE(GPIO8_5) RIN2  (I)
    sys_base[0x100/4]=1; // NF_ALE(GPIO8_6) RIN3  (I) 
    sys_base[0x104/4]=1; // NF_WEN(GPIO6_7) RIN4  (I)

    sys_base[0x114/4]=0; // SPI1_MOSI(GPIO5_4) J26.RESET
    sys_base[0x110/4]=0; // SPI1_SCLK(GPIO5_4) J24.RESET

    sys_base[0x0BC/4]=0; // PWM_OUT0(GPIO5_2) LED0
    sys_base[0x0C0/4]=0; // PWM_OUT1(GPIO5_3) LED1

    sys_base[0x120/4]=0; // JTAG_TRSTN    (GPIO0_0) DIPSW1
    sys_base[0x124/4]=0; // JTAG_TCK      (GPIO0_1) DIPSW2
    sys_base[0x128/4]=0; // JTAG TMS      (GPIO0_2) DIPSW3
    sys_base[0x12C/4]=0; // JTAG_TDO      (GPIO0_3) DIPSW4
    sys_base[0x130/4]=0; // JTAG_TDI      (GPIO0_4) DIPSW5
    sys_base[0x0C4/4]=1; // IRIN          (GPIO7_5) DIPSW6
    sys_base[0x07C/4]=0; // FLASH_TRIG    (GPIO1_7) DIPSW7
    sys_base[0x000/4]=0; // SHUTTER_TRIG  (GPIO1_0) DIPSW8

    sys_base[0x020/4]=0; // UART1_RSTN (GPIO2_2) RS485A_CTL
    //sys_base[0x028/4]=0; // UART1_CTSN (GPIO2_4) RS485B_CTL //Next Version
    gpio0_base = (unsigned long *) mmap(NULL,0x800,PROT_READ|PROT_WRITE,MAP_SHARED,memfd,0x20140000);
    if(gpio0_base==0) {
        printf("gpio0_base NULL pointer!\n");	 
        return (-1);
    }
    gpio0_base[0x400/4]&=0xE0;  //b0-b4 Input

    gpio1_base = (unsigned long *) mmap(NULL,0x800,PROT_READ|PROT_WRITE,MAP_SHARED,memfd,0x20150000);
    if(gpio1_base==0) {
        printf("gpio1_base NULL pointer!\n");	 
        return (-1);
    }
    gpio1_base[0x400/4]&=0x7E;  //b7,b0 Input
    gpio1_base[0x400/4]|=0x70;  //b4/5/6 Output
    gpio1_base[0x1C0/4]=0x00;  //b4/5/6 Output Low


    gpio2_base = (unsigned long *) mmap(NULL,0x800,PROT_READ|PROT_WRITE,MAP_SHARED,memfd,0x20160000);
    if(gpio2_base==0) {
        printf("gpio2_base NULL pointer!\n");	 
        return (-1);
    }

    gpio2_base[0x400/4]|=0x14;  //b4/2 Output
    gpio2_base[0x010/4]|=0x04;  //GPIO2_2 Output
    gpio5_base = (unsigned long *) mmap(NULL,0x800,PROT_READ|PROT_WRITE,MAP_SHARED,memfd,0x20190000);
    if(gpio5_base==0) {
        printf("gpio5_base NULL pointer!\n");	 
        return (-1);
    }

    gpio5_base[0x400/4]|=0xFC;  //b2/3/4/5/6/7 Output
    gpio5_base[0x3F0/4]=0x00;  //b2/3/4/5/6/7 Output Low

    gpio6_base = (unsigned long *) mmap(NULL,0x800,PROT_READ|PROT_WRITE,MAP_SHARED,memfd,0x201A0000);
    if(gpio6_base==0) {
        printf("gpio6_base NULL pointer!\n");	 
        return (-1);
    }
    gpio6_base[0x400/4]&=0x7F;  //b7 Input
    gpio7_base = (unsigned long *) mmap(NULL,0x800,PROT_READ|PROT_WRITE,MAP_SHARED,memfd,0x201B0000);
    if(gpio7_base==0) {
        printf("gpio7_base NULL pointer!\n");	 
        return (-1);
    }
    gpio7_base[0x400/4]&=0xDF;  //b5 Input
    gpio8_base = (unsigned long *) mmap(NULL,0x800,PROT_READ|PROT_WRITE,MAP_SHARED,memfd,0x201C0000);
    if(gpio8_base==0) {
        printf("gpio8_base NULL pointer!\n");	 
        return (-1);
    }
    gpio8_base[0x400/4]&=0x9B;  //b2/5/6 Input
  //  as_gen_set_video_channel(0);
}
int as_gen_read_nt99140(u32 addr , u8 *data) {
    return 0;
}
int as_gen_write_nt99140(u32 addr, u8 *data) {
    return 0;
} 
int as_gen_rs485_test(void) {
    return 0;
} 
int as_gen_rs485_status(int *st) {
    return 0;
}
int as_gen_usb_status(int *st) {
}
int as_gen_set_video_channel(u32 ch) {
    if(ch!=0) {
        printf("Switch Sensor J26\n"); 
        gpio5_base[0x400/4]|=0x10;
        gpio5_base[0x400/4]&=0xDF;  
        //gpio5_base[0x040/4]=0x10;  //b4 Output High J26
        gpio5_base[0x040/4]=0x00;  //b5 Output Low J24

    } else {
        printf("Switch Sensor J24\n");
        gpio5_base[0x400/4]|=0x20;
        gpio5_base[0x400/4]&=0xEF;  
        //gpio5_base[0x080/4]=0x20;  //b4 Output High J24
        gpio5_base[0x080/4]=0x00;  //b5 Output Low J26
    }
}

int as_gen_read_dip_switch(u32 *data) {
    unsigned char ch;
    *data=0x0000;
    gpio0_base[0x3FF/4]=0xFF;
    ch=gpio0_base[0x3FF/4]; 
    if((ch & 0x01)==0x00) *data|=0x0001;
    if((ch & 0x02)==0x00) *data|=0x0002;
    if((ch & 0x04)==0x00) *data|=0x0004;
    if((ch & 0x08)==0x00) *data|=0x0008;
    if((ch & 0x10)==0x00) *data|=0x0010;
    ch=gpio7_base[0x3FF/4]; 
    if((ch & 0x20)==0x00) *data|=0x0020;
    ch=gpio1_base[0x3FF/4]; 
    if((ch & 0x80)==0x00) *data|=0x0040;
    if((ch & 0x01)==0x00) *data|=0x0080;

}

int as_gen_read_alarm_in(u32 *alarm) {
}

int as_gen_set_status_led(u32 flag) {
}

int as_gen_set_do(u32 flag) {

    if(flag & 0x0002) gpio5_base[0x200/4]|=0x80; 
    else gpio5_base[0x200/4]&=0x7F;   
    if(flag & 0x0001) gpio1_base[0x1C0/4]|=0x20; 
    else gpio1_base[0x1C0/4]&=0xDF;  
    if(flag & 0x0008) gpio1_base[0x1C0/4]|=0x40; 
    else gpio1_base[0x1C0/4]&=0xBF;
    if(flag & 0x0004) gpio1_base[0x1C0/4]|=0x10;
    else gpio1_base[0x1C0/4]&=0xEF;

}

int as_gen_set_red_led(u32 flag) {
//    if(flag==0) gpio5_base[0x010/4]=0xFF; // LED0 On
//    else gpio5_base[0x010/4]=0x00; // LED0 On
	//printf("[as_gen_set_red_led] %u\n",flag);
    if(flag ==0) gpio5_base[0x200/4]|=0x80; 
    else gpio5_base[0x200/4]&=0x7F;   
}

int as_gen_set_green_led(u32 flag) {
	//printf("[as_gen_set_green_led] %u\n",flag);
//    if(flag==0) gpio5_base[0x020/4]=0xFF; // LED1 Off
//    else gpio5_base[0x020/4]=0x00; // LED1 Off
    if(flag ==0) gpio1_base[0x1C0/4]|=0x20; 
    else gpio1_base[0x1C0/4]&=0xDF;  
}

int as_gen_set_red_led1(u32 flag) {
	//printf("[as_gen_set_red_led1] %u\n",flag);
    if(flag ==0) gpio1_base[0x1C0/4]|=0x10;
    else gpio1_base[0x1C0/4]&=0xEF;	
}

int as_gen_set_green_led1(u32 flag) {
	//printf("[as_gen_set_green_led1] %u\n",flag);
	if(flag ==0) gpio1_base[0x1C0/4]|=0x40; 
    else gpio1_base[0x1C0/4]&=0xBF;
}

int as_gen_wdt_reboot(void) {
}

int as_gen_wdt_set_margin(u32 margin) {
}

int as_gen_wdt_keep_alive(void) {
}

int as_gen_wdt_disable(void) {
}
int as_gen_wdt_enable(void) {
}
/*!
 * @brief       send pipe command
 *
 * @param[in]  	cmd      		pipe command
 *
 * @return      0 on success, otherwise < 0
 */
int as_gen_net_pipe_cmd(const char *cmd) {
    system(cmd);
}
/*!
 * @brief       set serial number
 *
 * @param[in]	sn		serial number string buffer, max:32 byte
 *
 * @return      0 on success, otherwise < 0
 */
int as_gen_set_serial_num(char *sn) {
/*
    unsigned char snStr[30]="vs2sn=VS2000000";
    FILE *fsn=fopen("/etc/vs2sn.user","rb");
    if(fsn > 0 ) {
        fread(snStr,1,30,fsn);
    }
    fclose(fsn);
    snStr[6+9]=0x00;
    sprintf(sn,"%s",&snStr[6]);
*/
    return 0;
}

/*!
 * @brief       get serial number
 *
 * @param[out]	sn		serial number string buffer
 * @param[in]	size	string size
 *
 * @return      0 on success, otherwise < 0
 */
int as_gen_get_serial_num(char *sn, u32 size) {
    int i;
    unsigned char snStr[30]="vs2sn=VS2000000";
    FILE *fsn=fopen("/etc/vs2sn.user","rb");
    if(fsn > 0 ) {
        fread(snStr,1,30,fsn);
    }
    fclose(fsn);
    snStr[6+9]=0x00;
    
    sprintf(sn,"%s",&snStr[6]);
    for(i=0;i<9;i++) sn[i]=snStr[6+i];
    return 1;
}

/*!
 * @brief       set mac address
 *
 * @param[in]	mac		mac address1, 6 byte
 *
 * @return      0 on success, otherwise < 0
 */
int as_gen_set_mac(char *mac) {

}

/*!
 * @brief       get mac address
 *
 * @param[out]	mac		mac address
 *
 * @return      0 on success, otherwise < 0
 */
int as_gen_get_mac(u8 *mac) {
}

/*!
 * @brief       get firmware version
 *
 * @param[out]	version	version string buffer
 * @param[in]	size	string size
 *
 * @return      0 on success, otherwise < 0
 */
int as_gen_fw_get_version(char *version, u32 size) {
}

/*!
 * @brief       get firmware version number
 *
 * @return      firmware version number in 32bits
 */
u32 as_gen_fw_get_version_num(void) {
   return 0x0000002A;
}

/*!
 * @brief       get firmware upgrade status
 *
 * @param[out]	percentage	upgrade status
 *
 * @return      0 on success
 * @return      1 on idle
 * @return      -1 on fail
 */
int as_gen_fw_status(u32 *percentage) {
}

/*!
 * @brief       erase config
 *
 * @return      0 on success, otherwise < 0
 */
int as_gen_cfg_erase(void) {
//   return 0;   
}

/*!
 * @brief       write config to flash
 *
 * @param[in]	data	config data
 * @param[in]	size	config size
 * @param[in]	offset	config offset on flash
 *
 * @return      0 on success, otherwise < 0
 */
int as_gen_cfg_write(u8 *data, u32 size, u32 offset) {
/*
    FILE *fp;
    printf("VS2.APP cfg_write offset:0x%08X / Size:%ld\r\n",offset,size);
    fp=fopen("/root/ascfg.dat","rb+");
    if(fp > 0) {
       fseek(fp, offset, SEEK_SET);
       fwrite(data,size,1,fp);
       fclose(fp);
       return 0;
    }
    return -1;
*/
}

/*!
 * @brief       read config from flash
 *
 * @param[out]	data	config data
 * @param[in]	size	config size
 * @param[in]	offset	config offset on flash
 *
 * @return      0 on success, otherwise < 0
 */
int as_gen_cfg_read(u8 *data, u32 size, u32 offset) {
/*
    FILE *fp;
    printf("VS2.APP cfg_read offset:0x%08X / Size:%ld\r\n",offset,size);
    fp=fopen("/root/ascfg.dat","rb");
    if(fp > 0) {
       fseek(fp, offset, SEEK_SET);
       fread(data,size,1,fp);
       fclose(fp);
       return 0;
    }
    return -1;
*/
}
int as_gen_cfg_encoder1_read(u8 *data, u32 size, u32 offset) {
    FILE *fp;
    printf("VS2.APP cfg_read offset:0x%08X / Size:%ld\r\n",offset,size);
    fp=fopen("/root/asenc1.dat","rb");
    if(fp > 0) {
       fread(data,size,1,fp);
       fclose(fp);
       return 0;
    }
    return -1;
}
int as_gen_cfg_encoder1_write(u8 *data, u32 size, u32 offset) {

    FILE *fp;
    fp=fopen("/root/asenc1.dat","rb+");
    if(fp > 0) {
       fwrite(data,size,1,fp);
       fclose(fp);
       return 0;
    } else {
       fp=fopen("/root/asenc1.dat","wb");
       fwrite(data,size,1,fp);
       fclose(fp);
       return 0;
    }
    return -1;

}
/*!
 * @brief       upgrade firmware
 *
 * @param[out]	data	firmware data
 * @param[in]	size	firmware size
 *
 * @return      0 on success, otherwise < 0
 */
int as_gen_fw_upgrade(const u8 *data, u32 size) {
}

/*!
 * @brief       generic library initialize
 *
 * @return      0 on success, otherwise < 0
 */
