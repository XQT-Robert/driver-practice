#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
/***************************************************************
Copyright © ALIENTEK Co., Ltd. 1998-2029. All rights reserved.
文件名		: ledApp.c
作者	  	: 正点原子Linux团队
版本	   	: V1.0
描述	   	: platform驱动驱测试APP。
其他	   	: 无
使用方法	 ：./ledApp /dev/platled  0 关闭LED
		     ./ledApp /dev/platled  1 打开LED		
论坛 	   	: www.openedv.com
日志	   	: 初版V1.0 2019/8/16 正点原子Linux团队创建
***************************************************************/
#define LEDOFF 	0
#define LEDON 	1

/*
 * @description		: main主程序
 * @param - argc 	: argv数组元素个数
 * @param - argv 	: 具体参数
 * @return 			: 0 成功;其他 失败
 */
int main(int argc, char *argv[])
{
	int fd, retvalue;
	char *filename;
	unsigned char databuf[1];
	
	int i;

	// 参数检查
	if(argc != 3){
		printf("Error Usage!\r\n");
		return -1;
	}

	filename = argv[1];

	// 打开led驱动
	fd = open(filename, O_RDWR);
	if(fd < 0){
		printf("file %s open failed!\r\n", argv[1]);
		return -1;
	}

	// 从命令行参数中读取数据并存储到 databuf
	databuf[0] = (unsigned char)atoi(argv[2]);	/* 要执行的操作：打开或关闭 */
	// 写入数据到设备
	retvalue = write(fd, databuf, sizeof(databuf));

	// 错误检查
	if(retvalue < 0){
		printf("LED Control Failed!\r\n");
		close(fd);
		return -1;
	}

	// 关闭文件
	retvalue = close(fd); 
	if(retvalue < 0){
		printf("file %s close failed!\r\n", argv[1]);
		return -1;
	}

	return 0;
}
