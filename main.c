/*!
 *  @file       main.c
 *  @version    0.01
 *  @author     James Chien <james@phototracq.com>
 *  @作用 : VS2 主程式
 */

extern int app_init(void);
extern int app_main(void);
extern int va_init(void);

int main(int argc, char *argv[])
{
    printf("VS2 Version 0.01(2015/08/12) \n");
    system("/sbin/ifconfig eth0 10.54.43.252 netmask 255.255.255.0");
	//system("/root/watchdog &");
    app_init();
    va_init();
    app_main();
    return 0;
}

