// SPDX-License-Identifier: GPL-2.0
/*
 * Driver pratice
 *
 * Descirption: NULL
 * 
 * Notice: NULL
 * 
 * Author: Xu <xqt_robert@126.com>
 * 
 * Configuration:
 * 		全局修改参数（其他代码一般和逻辑相关容易察觉）
 * 		1、修改名称包括固定的命名或者特定的struct，全局搜索即可
 * 		2、类型表示该变量的类型，结构也就是类，一般是特定的结构体
 * 		3、作用说明该变量或类型的作用
 * 		4、所属表示该类型属于哪个结构
 * 
 * 	|  修改名称  	  | 类型 	|    作用		|	所属   			|		个数  		|
 * 	1、MODULE_NAME 		宏 			模块名称		NULL					1
 * 	2、DEVICE_NAME 		宏 			设备名			NULL					1
 * 	3、MISCBEEP_MINOR	宏 			子设备号		NULL					1
 * ------------		
 * 
 */
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/semaphore.h>
#include <linux/of_irq.h>
#include <linux/irq.h>
#include <linux/poll.h>
#include <linux/fcntl.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>

#include <linux/sched.h>
#include <linux/pid.h>

#define MODULE_NAME			"misc_beep" /* 模块名称 */
#define DEVICE_NAME			"misc_beep_dev" /* 设备名称 */
#define MISCBEEP_MINOR		144			/* 子设备号 */

/* 定义按键状态 */
enum beep_status {
    BEEP_ON = 0,    
    BEEP_OFF,      
};

/* 按键属性 */
struct beep_base{
	struct miscdevice misc ;
	int beep_pin;  				
	
};

static struct beep_base beep_dev;	//beep使用最简单的字符型即可

static int beep_open(struct inode *inode, struct file *filp)
{
	return 0;
}


/*
 * @description		: 向设备写数据 
 * @param - filp 	: 设备文件，表示打开的文件描述符
 * @param - buf 	: 要写给设备写入的数据
 * @param - cnt 	: 要写入的数据长度
 * @param - offt 	: 相对于文件首地址的偏移
 * @return 			: 写入的字节数，如果为负值，表示写入失败
 */
static ssize_t beep_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
	int retvalue;
	unsigned char databuf[1];
	unsigned char beepstat = 0;
	
	retvalue = copy_from_user(databuf, buf, cnt);
	if(retvalue < 0) {
		printk("kernel write faibeep!\r\n");
		return -EFAULT;
	}

	beepstat = databuf[0];		/* 获取状态值 */
	if(beepstat == BEEP_ON) {
		gpio_set_value(beep_dev.beep_pin, 0);	/* 打开蜂鸣器 */
	}else if(beepstat == BEEP_OFF) {
		gpio_set_value(beep_dev.beep_pin, 1);	/* 关闭蜂鸣器 */
	}

	return 0;
}

static struct file_operations beep_fops = {
	.owner = THIS_MODULE,
	.write = beep_write,
	.open = beep_open,
};

/* MISC设备结构体 */
static struct miscdevice beep_miscdev = {
	.minor = MISCBEEP_MINOR,
	.name = DEVICE_NAME,
	.fops = &beep_fops,
};

/** 
 * @brief			: 读取设备树信息 
 * @description		:
 * 		仅读取，和修改分离。不使用platform时，设备树节点匹配需要手动调用of函数，
 * 		查找具体的节点，在Platform中，该部分内容由机制完成，避免代码重复，
 * 		设备树节点匹配只要在match列表中写出即可，这里的nd参数则是获取到的
 * 		设备节点信息，在该函数下，利用函数仅获取匹配后设备树节点中的信息即可。
 * @return 			: 0 成功;其他 失败
 */
static int parse_devtree(struct device_node *nd)
{
	/* get pin */
	beep_dev.beep_pin = of_get_named_gpio(nd, "beep-pin", 0);
	if(!gpio_is_valid(beep_dev.beep_pin)) {
        printk(KERN_ERR "beepdev: Failed to get beep-pin\n");
        return -EINVAL;
    }

	printk("beep-pin num = %d\r\n", beep_dev.beep_pin);

	return 0;
}

/** 
 * @brief			: 初始化pinctrl中设定信息 
 * @description		:
 * 		仅修改，和读取分离。利用gpio子系统初始化已经pinctrl中设置好的节点信息。
 * 		确保pinctrl信息已经被读入程序。
 * @return 			: 0 成功;其他 失败
 */
static int beep_pinctrl_init(void)
{
	int ret;

	/* 向gpio子系统申请使用GPIO */
	ret = gpio_request(beep_dev.beep_pin, "beep0");
    if (ret) {
        printk(KERN_ERR "beep: Failed to request beep-pin\n");
        return ret;
	}

	gpio_direction_output(beep_dev.beep_pin,1);	

	return 0;
}

/** 
 * @brief			: 初始化设备属性 
 * @description		:
 * 		初始化设备所有需要的属性，包括电气属性（调用另外的函数）、定时器、
 * 		中断、自旋锁或者信号量等。
 * @return 			: 0 成功;其他 失败
 */
static int beep_dev_init(struct platform_device *pdev)
{
	int ret;

	/* 读取设备树信息 */
	ret = parse_devtree(pdev->dev.of_node);
	if(ret < 0){
		pr_err("%s Couldn't parse dev-tree, ret=%d\r\n", MODULE_NAME, ret);
	}

	/* 设置pinctrl信息 */
	ret = beep_pinctrl_init();
	if(ret){
		gpio_free(beep_dev.beep_pin);
		return ret;
	}

	return 0;
}

/*
 * @description		: platform驱动的probe函数，当驱动与设备匹配以后此函数就会执行
 * @param - dev 	: platform设备
 * @return 			: 0，成功;其他负值,失败
 */
static int beep_probe(struct platform_device *pdev)
{	
	int ret = 0;
	
	printk("beep driver and device was matched!\r\n");

	ret = beep_dev_init(pdev);
	if(ret < 0){
		pr_err("%s Couldn't init property, ret=%d\r\n", MODULE_NAME, ret);
	}

	/* 一般情况下会注册对应的字符设备，但是这里我们使用MISC设备
  	 * 所以我们不需要自己注册字符设备驱动，只需要注册misc设备驱动即可
	 */
	ret = misc_register(&beep_miscdev);
	if(ret < 0){
		printk("misc device register failed!\r\n");
		goto free_prop;
	}
	
	return 0;

/* 在这里清除所有设备的属性，比如定时器或者释放引脚等 */
free_prop:
	gpio_free(beep_dev.beep_pin);
	return -EIO;
}

/*
 * @description		: platform驱动的remove函数，移除platform驱动的时候此函数会执行
 * @param - dev 	: platform设备
 * @return 			: 0，成功;其他负值,失败
 */
static int beep_remove(struct platform_device *dev)
{	
	/* 注销设备的时候关闭LED灯 */
	gpio_set_value(beep_dev.beep_pin, 1);
	
	/* 释放BEEP */
	gpio_free(beep_dev.beep_pin);

	/* 注销misc设备 */
	misc_deregister(&beep_miscdev);
	return 0;
}

/* 匹配列表 */
static const struct of_device_id beep_of_match[] = {
	{ .compatible = "robert,beep" },
	{ /* Sentinel */ }
};

MODULE_DEVICE_TABLE(of, beep_of_match);

/* platform驱动结构体 */
static struct platform_driver beep_driver = {
	.driver		= {
		.name	= MODULE_NAME,			/* 驱动名字，用于和设备匹配 */
		.of_match_table	= beep_of_match, 	/* 设备树匹配表 		 */
	},
	.probe		= beep_probe,
	.remove		= beep_remove,
};


static int __init chr_dev_init(void)
{
	return platform_driver_register(&beep_driver);
}

static void __exit chr_dev_exit(void)
{
	platform_driver_unregister(&beep_driver);
}

module_init(chr_dev_init);
module_exit(chr_dev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Robert");
MODULE_INFO(intree, "Y");