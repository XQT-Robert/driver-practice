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


#define LED_NAME		"gpioled" 	/* 设备名字 */
#define LEDOFF 	0				/* 关灯 */
#define LEDON 	1				/* 开灯 */


struct led_dev{
	dev_t devid;
	struct cdev cdev;
	struct class *class;
	struct device *device; 
	int major;
	int minor;
	struct device_node *nd;
	int led_gpio; /*linux会把某个设备树的gpio表示为一个数值，比如128就是PI0*/
};

struct led_dev gpioled;


/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int led_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &gpioled; /* 设置私有数据 */
	return 0;
}

/*
 * @description		: 从设备读取数据 
 * @param - filp 	: 要打开的设备文件(文件描述符)
 * @param - buf 	: 返回给用户空间的数据缓冲区
 * @param - cnt 	: 要读取的数据长度
 * @param - offt 	: 相对于文件首地址的偏移
 * @return 			: 读取的字节数，如果为负值，表示读取失败
 */
static ssize_t led_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
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
	int ret;
	unsigned char databuf[1];
	unsigned char ledstat;
	struct led_dev *dev = filp->private_data;

	ret = copy_from_user(databuf, buf, cnt);
	if(ret < 0) {
		printk("kernel write failed!\r\n");
		return -EFAULT;
	}

	ledstat = databuf[0];		/* 获取状态值 */

	if(ledstat == LEDON) {	
		gpio_set_value(dev->led_gpio, 0);	/* 打开LED灯 */
	} else if(ledstat == LEDOFF) {
		gpio_set_value(dev->led_gpio, 1);	/* 打开LED灯 */
	}
	return 0;
}

/*
 * @description		: 关闭/释放设备
 * @param - filp 	: 要关闭的设备文件(文件描述符)
 * @return 			: 0 成功;其他 失败
 */
static int led_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static struct file_operations led_fops = {
	.owner = THIS_MODULE,
	.open = led_open,
	.release = led_release,
	.write = led_write,
	.read = led_read,

};

static int __init led_init(void)
{
	int ret = 0;
	const char *str;

	gpioled.nd = of_find_node_by_path("/gpioled");
	if(gpioled.nd == NULL ){
		printk("stm32mp1_led node nost find!\r\n");
		return -EINVAL;
	}

	ret = of_property_read_string(gpioled.nd,"status",&str);
	if(ret < 0){
		printk("gpioled: Failed to get status property\n");
		return -EINVAL;
	} 

	if (strcmp(str, "okay"))
        return -EINVAL;

	ret = of_property_read_string(gpioled.nd,"compatible",&str);
	if(ret < 0){
		printk("gpioled: Failed to get compatible property\n");
		return -EINVAL;
	} 

	if (strcmp(str, "robert,led"))
        return -EINVAL;

		/* 4、 获取设备树中的gpio属性，得到LED所使用的LED编号 */
	gpioled.led_gpio = of_get_named_gpio(gpioled.nd, "led-gpio", 0);
	if(gpioled.led_gpio < 0) {
		printk("can't get led-gpio");
		return -EINVAL;
	}
	printk("led-gpio num = %d\r\n", gpioled.led_gpio);

		/* 5.向gpio子系统申请使用GPIO */
	ret = gpio_request(gpioled.led_gpio, "LED-GPIO");
    if (ret) {
        printk(KERN_ERR "gpioled: Failed to request led-gpio\n");
        return ret;
	}

	/* 6、设置PI0为输出，并且输出高电平，默认关闭LED灯 */
	ret = gpio_direction_output(gpioled.led_gpio, 1);
	if(ret < 0) {
		printk("can't set gpio!\r\n");
	}

	/* 注册字符设备驱动 */
	/* 1、创建设备号 */
	if (gpioled.major) {		/*  定义了设备号 */
		gpioled.devid = MKDEV(gpioled.major, 0);
		ret = register_chrdev_region(gpioled.devid, 1, LED_NAME);
		if(ret < 0) {
			pr_err("cannot register %s char driver [ret=%d]\n",LED_NAME, 1);
			goto free_gpio;
		}
	} else {						/* 没有定义设备号 */
		ret = alloc_chrdev_region(&gpioled.devid, 0, 1, LED_NAME);	/* 申请设备号 */
		if(ret < 0) {
			pr_err("%s Couldn't alloc_chrdev_region, ret=%d\r\n", LED_NAME, ret);
			goto free_gpio;
		}
		gpioled.major = MAJOR(gpioled.devid);	/* 获取分配号的主设备号 */
		gpioled.minor = MINOR(gpioled.devid);	/* 获取分配号的次设备号 */
	}
	printk("newcheled major=%d,minor=%d\r\n",gpioled.major, gpioled.minor);	
	
	/* 2、初始化cdev */
	gpioled.cdev.owner = THIS_MODULE;
	cdev_init(&gpioled.cdev, &led_fops);
	
	/* 3、添加一个cdev */
	ret = cdev_add(&gpioled.cdev, gpioled.devid, 1);
	if(ret < 0)
		goto del_unregister;
		
	/* 4、创建类 */
	gpioled.class = class_create(THIS_MODULE, LED_NAME);
	if (IS_ERR(gpioled.class)) {
		goto del_cdev;
	}

	/* 5、创建设备 */
	gpioled.device = device_create(gpioled.class, NULL, gpioled.devid, NULL, LED_NAME);
	if (IS_ERR(gpioled.device)) {
		goto destroy_class;
	}
	
	return 0;

destroy_class:
	class_destroy(gpioled.class);
del_cdev:
	cdev_del(&gpioled.cdev);
del_unregister:
	unregister_chrdev_region(gpioled.devid, 1);
free_gpio:
	gpio_free(gpioled.led_gpio);
	return -EIO;
}

static void __exit led_exit(void)
{
   
	/* 注销字符设备驱动 */
	cdev_del(&gpioled.cdev);/*  删除cdev */
	unregister_chrdev_region(gpioled.devid, 1); /* 注销设备号 */

	device_destroy(gpioled.class, gpioled.devid);
	class_destroy(gpioled.class);

	gpio_free(gpioled.led_gpio); /* 释放GPIO */
}

module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Robert");
MODULE_INFO(intree, "Y");