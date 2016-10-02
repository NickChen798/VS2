/*!
 *  @file       app_init.c
 *  @version    0.01
 *  @author     James Chien <james@phototracq.com>
 */
#include <signal.h>
#include "as_headers.h"
#include "as_dm.h"
#include "as_cmd.h"
#include "as_media.h"
#include "as_generic.h"
#include "as_config.h"

static void quit_signal(int sig)
{
	static int disable_flag = 0;
	if (!disable_flag) {
		if (as_gen_wdt_disable() < 0) {
			printf("as_gen_wdt_disable error\n");
		}

		disable_flag = 1;
	}
	usleep(100000);
	exit(1);
}

int app_init(void)
{
	signal(SIGINT, quit_signal);
	signal(SIGTERM, quit_signal);


/*
    printf("PID:%d, Model:%d\n", MKF_PRODUCT, MKF_MODEL);
    printf("FW version: 0x%x\n", as_gen_fw_get_version_num());
    printf("Build Date: 0x%x\n", FW_BUILD);
*/
    	struct timeval tt;
    	gettimeofday(&tt, 0);
    	srandom((u32)tt.tv_usec);
	if (as_gen_init() < 0) {
		printf( "as_gen_init() fail..\n" );
		return -1;
	}

    	u32 flag = 0;
    	as_gen_read_dip_switch(&flag);
    	printf("Device ID: %u\n", flag);

    	if(dm_init(1, 2) < 0) {
        	printf("dm_init failed\n");
        	return -1;
    	}

    	as_media_init_attr_t init_attr;
    	init_attr.vsystem = AS_MEDIA_VIDEO_TYPE_NTSC;
/*
    	if(as_media_init( &init_attr ) < 0) {
        	printf("as_media_init error\n");
        	return -1;
    	}
*/
    	if(as_media_enc_setup() < 0) {
        	printf("failed to as_media_enc_setup\n");
        	return -1;
    	}

    	if(as_cfg_init() < 0) {
        	printf("as_cfg_init failed\n");
    	}

    	if(as_data_init(1, 2, 8) < 0) {
        	printf("as_data_init failed\n");
        	return -1;
    	}

    	if(as_cmd_init() < 0) {
        	printf("as_cmd_init failed\n");
        	return -1;
    	}

    	dm_reg_aslive_cb(as_data_in);

    	return 0;
}

int app_main(void)
{
	printf("=========== Delay Encoder ================\n");
	sleep(3);
/*
    	if(as_media_enc_start() < 0) {
        	printf("failed to as_media_enc_start\n");
        	return -1;
    	}
*/
	while (1) {
		sleep(1);
	}
	return 0;
}
