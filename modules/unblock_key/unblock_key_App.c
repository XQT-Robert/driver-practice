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

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>


#define KEYOFF 	0
#define KEYON 	1

/*
 * @description		: main主程序
 * @param - argc 	: argv数组元素个数
 * @param - argv 	: 具体参数
 * @return 			: 0 成功;其他 失败
 */
int main(int argc, char *argv[])
{
	int fd;
	int ret;
	char *filename;
	int keyvalue;
	
	fd_set readfds;

	if(argc != 2){
		printf("Error Usage!\r\n");
		return -1;
	}

	filename = argv[1];

	/* 打开key驱动 */
	fd = open(filename, O_RDWR | O_NONBLOCK);
	if(fd < 0){
		printf("file %s open failed!\r\n", argv[1]);
		return -1;
	}

	FD_ZERO(&readfds);
    FD_SET(fd, &readfds);

	/* 循环读取按键值数据！ */
	while(1) {
		ret = select(fd + 1, &readfds, NULL, NULL, NULL);

		 switch (ret) {

        case 0:     // 超时
            /* 用户自定义超时处理 */
            break;

        case -1:        // 错误
            /* 用户自定义错误处理 */
            break;

        default:
            if(FD_ISSET(fd, &readfds)) {
                read(fd, &keyvalue, sizeof(int));
                if (0 == keyvalue)
                    printf("Key Press\n");
                else if (1 == keyvalue)
                    printf("Key Release\n");
            }

            break;
        }
	}

	ret= close(fd); /* 关闭文件 */
	if(ret < 0){
		printf("file %s close failed!\r\n", argv[1]);
		return -1;
	}
	return 0;
}