#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#define MATE_DESKTOP_USE_UNSTABLE_API
#include <libmate-desktop/mate-rr.h>
#include <libmate-desktop/mate-rr-config.h>
#include <libmate-desktop/mate-rr-labeler.h>
#include <libmate-desktop/mate-desktop-utils.h>
#include <math.h>

int main (){
	MateRRScreen          *mmScreen;
	MateRRConfig *config;
	gdk_init(NULL, NULL);
	mmScreen = mate_rr_screen_new (gdk_screen_get_default (),NULL);
	config = mate_rr_config_new_current (mmScreen, NULL);
	MateRROutputInfo **outputs = mate_rr_config_get_outputs (config);
	system("su kylin -c 'xhost + '");

	for (int i = 0; outputs[i] != NULL; i++) {
		MateRROutputInfo *output = outputs[i];
		printf("显示器名字:%s, ", mate_rr_output_info_get_name (output));
		printf("是否输出:%s, ", mate_rr_output_info_is_active(output)?"是":"否");
		printf("是否连接:%s \n", mate_rr_output_info_is_connected(output)?"是":"否");
		if(mate_rr_output_info_is_connected(output)){
			int x,y,width,height=0;
			mate_rr_output_info_get_geometry(output, &x, &y, &width, &height);
			printf("输出位置为:%d %d %d %d \n", x, y, width, height);
			printf("是否为主屏:%s \n", mate_rr_output_info_get_primary (output)?"是":"否");
		}
	}

	bool current_config = mate_rr_config_get_clone(config);
	printf("当前克隆配置:%d \n", current_config);

	mate_rr_config_set_clone(config,false);
	printf("设置克隆配置:%d \n", mate_rr_config_get_clone(config));
}
