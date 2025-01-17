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
 * 	2、DEV_MAX_CNT 		宏 			设备数			NULL					1
 * 	3、struct class		结构 		设备类型		NULL					1
 * 	4、dev_name			char		设备名称	    struct char_base		若干
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

#include <linux/sched.h>
#include <linux/pid.h>

#define MODULE_NAME		"platform_led" 	/* 模块名称 */
#define DEV_MAX_CNT 	 1			/* 驱动最高复用设备数，即挂载/dev/的节点数 */

/* 定义按键状态 */
enum led_status {
    LED_ON = 0,    
    LED_OFF,      
};

/* 驱动全局属性 */
struct class *led_class; 			/* 表示当前驱动代表的类型，可复用只创建一个 */
dev_t module_devid;					/* 驱动模块的设备号，即第一个设备的设备号 */
int devid_major;					
int dev_num = 0;					/* 复用当前驱动的设备数，互斥访问 */


/* 字符型设备属性 */
struct chr_base{ 
	dev_t devid;					
	struct cdev cdev;
	struct device *device; 
	int devid_minor;				
	struct device_node *nd; 		
	char dev_name[20]; 				/* /dev/下挂载的设备名称 */
};

/* 按键属性 */
struct led_base{
	struct chr_base parent;			/* 继承字符型设备属性 */
	int led_pin;  				
	
};


static struct led_base led_dev;	//led使用最简单的字符型即可
static struct chr_base *pBase_chr_led;		/* 指针目标_设备父类_所属设备 */

/*
 * @description		: LED打开/关闭
 * @param - sta 	: LEDON(0) 打开LED，LEDOFF(1) 关闭LED
 * @return 			: 无
 */
void led_switch(u8 sta)
{
if (sta == LED_ON )
		gpio_set_value(led_dev.led_pin, 0);
	else if (sta == LED_OFF)
		gpio_set_value(led_dev.led_pin, 1);	
}



static int led_open(struct inode *inode, struct file *filp)
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
static ssize_t led_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
	int retvalue;
	unsigned char databuf[1];
	unsigned char ledstat = 0;
	
	retvalue = copy_from_user(databuf, buf, cnt);
	if(retvalue < 0) {
		printk("kernel write failed!\r\n");
		return -EFAULT;
	}

	ledstat = databuf[0];		/* 获取状态值 */
	if(ledstat == LED_ON) {
		led_switch(LED_ON);		/* 打开LED灯 */
	}else if(ledstat == LED_OFF) {
		led_switch(LED_OFF);	/* 关闭LED灯 */
	}

	return 0;
}

static struct file_operations led_fops = {
	.owner = THIS_MODULE,
	.write = led_write,
	.open = led_open,
};


/** 
 * @brief			: 匹配设备树信息 
 * @description		:
 * 		在不使用platform的情况下，所有的设备树节点nd需要手动读取匹配，
 * 		使用of函数读取。而在使用platform机制的时候，设备树信息读取则
 * 		由Platform机制完成，这里只需要根据节点读取相关信息即可。	
 * @return 			: 0 成功;其他 失败
 */
static int parse_devtree(struct device_node *nd)
{
	/* 将父类指针指向设备父类属性方便访问 */
	pBase_chr_led = &led_dev.parent;	

	/* 4、 get pin */
	led_dev.led_pin = of_get_named_gpio(nd, "led-gpio", 0);
	if(!gpio_is_valid(led_dev.led_pin)) {
        printk(KERN_ERR "leddev: Failed to get led-gpio\n");
        return -EINVAL;
    }

	printk("led-pin num = %d\r\n", led_dev.led_pin);

	return 0;
}

static int led_pinctrl_init(void)
{
	int ret;

	/* 5.向gpio子系统申请使用GPIO */
	ret = gpio_request(led_dev.led_pin, "led0");
    if (ret) {
        printk(KERN_ERR "led: Failed to request led-pin\n");
        return ret;
	}

	gpio_direction_output(led_dev.led_pin,1);	

	return 0;
}

/* 设备属性初始化 */
static int led_dev_init(void)
{
	int ret;

	ret = led_pinctrl_init();
	if(ret){
		gpio_free(led_dev.led_pin);
		return ret;
	}

	return 0;
}

/*
 * @description		: platform驱动的probe函数，当驱动与设备匹配以后此函数就会执行
 * @param - dev 	: platform设备
 * @return 			: 0，成功;其他负值,失败
 */
static int led_probe(struct platform_device *pdev)
{	
	int ret = 0;
	
	printk("led driver and device was matched!\r\n");

	if (devid_major) {		/*  定义了设备号 */
		module_devid = MKDEV(devid_major, 0); /* 从0开始 */
		ret = register_chrdev_region(module_devid,DEV_MAX_CNT, MODULE_NAME);
		if(ret < 0) {
			pr_err("cannot register %s char driver [ret=%d]\n",MODULE_NAME, 1);
			
			/* 该设备号已经使用，返回设备忙*/
			return -EBUSY;
		}
	} else {						/* 没有定义设备号 */
		ret = alloc_chrdev_region(&module_devid, 0,DEV_MAX_CNT, MODULE_NAME);	
		if(ret < 0) {
			pr_err("%s Couldn't alloc_chrdev_region, ret=%d\r\n", MODULE_NAME, ret);
			
			/* 动态分配设备号失败，超出内存范围*/
			return -ENOMEM;
		}
		devid_major = MAJOR(module_devid);	
	}
	printk("devid_major=%d\r\n",devid_major);	

	led_class = class_create(THIS_MODULE, MODULE_NAME);
	if (IS_ERR(led_class)) {
		goto del_unregister;
	}

	/* 绑定设备信息，仅在绑定成功后初始化设备 */
	ret = parse_devtree(pdev->dev.of_node);
	if(ret < 0){
		pr_err("%s Couldn't parse dev-tree, ret=%d\r\n", MODULE_NAME, ret);
		goto destroy_class;
	}

	/* 绑定并初始化特定设备属性 */
	/* 初始化 LED */
	//ret = led_gpio_init(pdev->dev.of_node);
	ret = led_dev_init();
	if(ret < 0){
		pr_err("%s Couldn't init property, ret=%d\r\n", MODULE_NAME, ret);
		goto destroy_class;
	}

	//pBase_chr_led.dev_prop = &led_dev; 	

	/* 初始化第一个设备信息 */
	pBase_chr_led->devid_minor = 0;
	pBase_chr_led->devid = MKDEV(devid_major, pBase_chr_led->devid_minor);

	/* 设置最后一个参数作为设备挂载名称 */
	snprintf(pBase_chr_led->dev_name, sizeof(pBase_chr_led->dev_name), "led");
	pBase_chr_led->cdev.owner = THIS_MODULE;
	cdev_init(&pBase_chr_led->cdev, &led_fops);
	ret = cdev_add(&pBase_chr_led->cdev, pBase_chr_led->devid, DEV_MAX_CNT);
	if(ret < 0)
		goto free_prop;
		
	pBase_chr_led->device = device_create(led_class, NULL, pBase_chr_led->devid, NULL, pBase_chr_led->dev_name);
	if (IS_ERR(pBase_chr_led->device)) {
		goto del_cdev;
	}
	
	return 0;

del_cdev:
	cdev_del(&pBase_chr_led->cdev);
free_prop:
	gpio_free(led_dev.led_pin);
destroy_class:
	class_destroy(led_class);
del_unregister:
	unregister_chrdev_region(pBase_chr_led->devid, DEV_MAX_CNT);

	return -EIO;
}

/*
 * @description		: platform驱动的remove函数，移除platform驱动的时候此函数会执行
 * @param - dev 	: platform设备
 * @return 			: 0，成功;其他负值,失败
 */
static int led_remove(struct platform_device *dev)
{
	
	cdev_del(&pBase_chr_led->cdev);/*  删除cdev */
	gpio_set_value(led_dev.led_pin, 1); 	/* 卸载驱动的时候关闭LED */
	gpio_free(led_dev.led_pin);
	unregister_chrdev_region(pBase_chr_led->devid, DEV_MAX_CNT); /* 注销设备号 */
	device_destroy(led_class, pBase_chr_led->devid);
	class_destroy(led_class);
	return 0;
}

/* 匹配列表 */
static const struct of_device_id led_of_match[] = {
	{ .compatible = "robert,led" },
	{ /* Sentinel */ }
};

MODULE_DEVICE_TABLE(of, led_of_match);

/* platform驱动结构体 */
static struct platform_driver led_driver = {
	.driver		= {
		.name	= MODULE_NAME,			/* 驱动名字，用于和设备匹配 */
		.of_match_table	= led_of_match, 	/* 设备树匹配表 		 */
	},
	.probe		= led_probe,
	.remove		= led_remove,
};


static int __init chr_dev_init(void)
{
	return platform_driver_register(&led_driver);
}

static void __exit chr_dev_exit(void)
{
	platform_driver_unregister(&led_driver);
}

module_init(chr_dev_init);
module_exit(chr_dev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Robert");
MODULE_INFO(intree, "Y");