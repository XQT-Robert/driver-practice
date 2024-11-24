#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
#include <sys/ioctl.h>
/***************************************************************
Copyright © ALIENTEK Co., Ltd. 1998-2029. All rights reserved.
文件名		: ledApp.c
作者	  	: 正点原子
版本	   	: V1.0
描述	   	: chrdevbase驱测试APP。
其他	   	: 无
使用方法	 ：./ledtest /dev/led  0 关闭LED
		     ./ledtest /dev/led  1 打开LED		
论坛 	   	: www.openedv.com
日志	   	: 初版V1.0 2020/11/23 正点原子团队创建
***************************************************************/

#define KEYOFF 	0
#define KEYON 	1

#define TIMER_IOCTL_MARK  		'T'							/* 标记只允许单字节char */
#define TIMER_CLOSE_CMD 		(_IO(TIMER_IOCTL_MARK, 0x1))	/* 关闭定时器 */
#define TIMER_OPEN_CMD			(_IO(TIMER_IOCTL_MARK, 0x2))	/* 打开定时器 */
#define TIMER_SETPERIOD_CMD		(_IO(TIMER_IOCTL_MARK, 0x3))	/* 设置定时器周期命令 */

/*
 * @description		: main主程序
 * @param - argc 	: argv数组元素个数
 * @param - argv 	: 具体参数
 * @return 			: 0 成功;其他 失败
 */
int main(int argc, char *argv[])
{
	int fd, ret;
	char *filename;
	unsigned int cmd;
	unsigned int arg;
	unsigned char str[100];

	if (argc != 2) {
		printf("Error Usage!\r\n");
		return -1;
	}

	filename = argv[1];

	fd = open(filename, O_RDWR);
	if (fd < 0) {
		printf("Can't open file %s\r\n", filename);
		return -1;
	}

	while (1) {
		printf("Input CMD:");
		ret = scanf("%d", &cmd);
		if (ret != 1) {				/* 参数输入错误 */
			fgets(str, sizeof(str), stdin);				/* 防止卡死 */
		}
		if(4 == cmd)				/* 退出APP	 */
			goto out;
		if(cmd == 1)				/* 关闭LED灯 */
			cmd = TIMER_CLOSE_CMD;
		else if(cmd == 2)			/* 打开LED灯 */
			cmd = TIMER_OPEN_CMD;
		else if(cmd == 3) {
			cmd = TIMER_SETPERIOD_CMD;	/* 设置周期值 */
			printf("Input Timer Period:");
			ret = scanf("%d", &arg);
			if (ret != 1) {			/* 参数输入错误 */
				fgets(str, sizeof(str), stdin);			/* 防止卡死 */
			}
		}
		ioctl(fd, cmd, arg);		/* 控制定时器的打开和关闭 */	
	}

out:
	close(fd);
}