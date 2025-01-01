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
	
	int count = 0; 

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

	printf("the fking fd is %d\r\n", fd);

	FD_ZERO(&readfds);
    FD_SET(fd, &readfds);

	//按键按下松开均触发select回调，一按两个hello，触发两次读取
	//但是需要运行select
	ret = select(fd + 1, &readfds, NULL, NULL, NULL);
	read(fd, &keyvalue, sizeof(int));
	printf("hello 1r\n");
	ret = select(fd + 1, &readfds, NULL, NULL, NULL);
	read(fd, &keyvalue, sizeof(int));
	printf("hello 2 \r\n");
	ret = select(fd + 1, &readfds, NULL, NULL, NULL);
	read(fd, &keyvalue, sizeof(int));
	printf("hello 3\r\n");
	ret = select(fd + 1, &readfds, NULL, NULL, NULL);
	read(fd, &keyvalue, sizeof(int));
	printf("hello 4 \r\n");


	printf("special prior----------------------- \r\n");
	ret = select(fd + 1, &readfds, NULL, NULL, NULL);
	read(fd, &keyvalue, sizeof(int));
	read(fd, &keyvalue, sizeof(int));
	printf("two read after,block comming... \r\n");
	ret = select(fd + 1, &readfds, NULL, NULL, NULL);
	printf("select again ,without press button is no blcok.\r\n");
	printf("before the second select read \r\n");
	read(fd, &keyvalue, sizeof(int));
	printf("after the second select read \r\n");
	ret = select(fd + 1, &readfds, NULL, NULL, NULL);
	printf("the message will be block, because is no more ready \r\n");
	//事实证明，一次按键会产生松开按下两个状态的变更，让select获取到两次可读取信息
	//每执行一次select获取一次可读取的缓冲区信息，比如执行第一次select获取的是按下
	//第二次select获取的是松开，不执行select读取信息会停留在底层缓冲区
	//只有当两次信息均被读取后，调用select会被阻塞（在驱动的poll_wait中）
	//如果在这里使用2次非阻塞读，
	printf("special prior 2----------------------- \r\n");
	printf("well，two select now got press and release \r\n");
	ret = select(fd + 1, &readfds, NULL, NULL, NULL);
	ret = select(fd + 1, &readfds, NULL, NULL, NULL);
	printf("is three,be ware the last prior select,this will be block?  \r\n");
	read(fd, &keyvalue, sizeof(int));
	read(fd, &keyvalue, sizeof(int));
	read(fd, &keyvalue, sizeof(int));
	printf("read three,because both third select and read are useless \r\n");
	ret = select(fd + 1, &readfds, NULL, NULL, NULL);
	printf("one select one read! \r\n");
	read(fd, &keyvalue, sizeof(int));
	printf("we will stop before here! \r\n");
	ret = select(fd + 1, &readfds, NULL, NULL, NULL);

	printf("crazy stop by r1! \r\n");
	read(fd, &keyvalue, sizeof(int));
	printf("crazy stop by s1! \r\n");
	ret = select(fd + 1, &readfds, NULL, NULL, NULL);
	printf("crazy stop by r2! \r\n");
	read(fd, &keyvalue, sizeof(int));
	printf("crazy stop by s2! \r\n");
	ret = select(fd + 1, &readfds, NULL, NULL, NULL);



	/* 循环读取按键值数据！ */
	while(1) {
		printf("stupid while\r\n");
		ret = select(fd + 1, &readfds, NULL, NULL, NULL);
/*	
		switch (ret) {
        case 0:    
           	 break;
        case -1:        
            break;
        default:
            if(FD_ISSET(fd, &readfds)) {
                //read(fd, &keyvalue, sizeof(int));
                if (0 == keyvalue)
                    printf("Key Press\n");
                else if (1 == keyvalue)
                    printf("Key Release\n");
            }
            break;
        }
*/

		count++;
		if(1000 == count){//检测是否阻塞这里的循环（轮询）
			printf("fking stop\r\n");
			break;
		}
		else if(count % 100 == 0){
			printf("take it in 100\r\n");
			read(fd, &keyvalue, sizeof(int));
		}
		else{
			//printf("polling: %d\r\n", count);
		}

	}

	ret= close(fd); /* 关闭文件 */
	if(ret < 0){
		printf("file %s close failed!\r\n", argv[1]);
		return -1;
	}
	return 0;
}