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


#define MODULE_NAME		"beep" 	/* 模块名称 */
#define DEV_MAX_CNT 	 1			/* 驱动最高复用设备数，即挂载/dev/的节点数 */

#define BEEPOFF 		 0				/* 关灯 */
#define BEEPON 			 1				/* 开灯 */


struct class *beep_class; 			/* 表示当前驱动代表的类型，可复用只创建一个 */
dev_t module_devid;					/* 驱动模块的设备号，即第一个设备的设备号 */
int devid_major;					/* 一个驱动仅需要一个主设备号 */
int dev_num = 0;					/* 复用当前驱动的设备数，互斥访问 */


/* 该数据结构中仅包含一个设备独有的属性 */
struct chr_dev{ 
	dev_t devid;					/* 设备号唯一对应一个设备 */
	struct cdev cdev;
	struct device *device; 
	int devid_minor;				/* 次设备号为设备独有，主设备号同驱动共用 */
	struct device_node *nd; 		/* 用于保存设备树中的节点信息 */
	char dev_name[20]; 				/* /dev/下挂载的设备名称 */

	/* 
	 * linux会把某个设备树的gpio引脚表示为一个数值，比如128就是PI0 
	 * 设备中需要使用多少引脚，就需要多少个int变量（或者使用数组）
	 * 因此这个变量也是根据实际使用情况设置
	 */
	int beep_pin; 
};

struct chr_dev beep;


/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int beep_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &beep; /* 设置私有数据 */
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
static ssize_t beep_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
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
	int ret;
	unsigned char databuf[1];
	unsigned char beepstat;
	struct chr_dev *dev = filp->private_data;

	ret = copy_from_user(databuf, buf, cnt);
	if(ret < 0) {
		printk("kernel write failed!\r\n");
		return -EFAULT;
	}

	beepstat = databuf[0];		/* 获取状态值 */

	if(beepstat == BEEPON) {	
		gpio_set_value(dev->beep_pin, 0);	
	} else if(beepstat == BEEPOFF) {
		gpio_set_value(dev->beep_pin, 1);	
	}
	return 0;
}

/*
 * @description		: 关闭/释放设备
 * @param - filp 	: 要关闭的设备文件(文件描述符)
 * @return 			: 0 成功;其他 失败
 */
static int beep_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static struct file_operations beep_fops = {
	.owner = THIS_MODULE,
	.open = beep_open,
	.release = beep_release,
	.write = beep_write,
	.read = beep_read,

};

static int __init chr_dev_init(void)
{
	int ret = 0;
	const char *str;

	/* 注册字符设备驱动的主设备号 */
	/* 1、创建设备号 */
	if (devid_major) {		/*  定义了设备号 */
		module_devid = MKDEV(devid_major, 0); /* 从0开始 */
		ret = register_chrdev_region(module_devid,DEV_MAX_CNT, MODULE_NAME);
		if(ret < 0) {
			pr_err("cannot register %s char driver [ret=%d]\n",MODULE_NAME, 1);
			goto free_gpio;
		}
	} else {						/* 没有定义设备号 */
		ret = alloc_chrdev_region(&module_devid, 0,DEV_MAX_CNT, MODULE_NAME);	/* 申请设备号 */
		if(ret < 0) {
			pr_err("%s Couldn't alloc_chrdev_region, ret=%d\r\n", MODULE_NAME, ret);
			goto free_gpio;
		}
		devid_major = MAJOR(module_devid);	/* 获取分配号的主设备号 */
	}
	printk("devid_major=%d\r\n",devid_major);	

	/* 4、创建类 */
	beep_class = class_create(THIS_MODULE, MODULE_NAME);
	if (IS_ERR(beep_class)) {
		goto del_cdev;
	}

	/*	创建设备号、设备名称 */
	beep.devid_minor = 0;
	beep.devid = MKDEV(devid_major, beep.devid_minor);
	snprintf(beep.dev_name, sizeof(beep.dev_name), "beep");

	beep.nd = of_find_node_by_path("/beep");
	if(beep.nd == NULL ){
		printk("beep node nost find!\r\n");
		return -EINVAL;
	}

	ret = of_property_read_string(beep.nd,"status",&str);
	if(ret < 0){
		printk("beep: Failed to get status property\n");
		return -EINVAL;
	} 

	if (strcmp(str, "okay"))
        return -EINVAL;

	ret = of_property_read_string(beep.nd,"compatible",&str);
	if(ret < 0){
		printk("beep: Failed to get compatible property\n");
		return -EINVAL;
	} 

	if (strcmp(str, "robert,beep"))
        return -EINVAL;

	/* 4、 获取设备树中的gpio属性，得到LED所使用的LED编号 */
	beep.beep_pin = of_get_named_gpio(beep.nd, "beep-pin", 0);
	if(beep.beep_pin < 0) {
		printk("can't get beep-pin");
		return -EINVAL;
	}
	printk("beep-pin num = %d\r\n", beep.beep_pin);

	/* 5.向gpio子系统申请使用GPIO */
	ret = gpio_request(beep.beep_pin, "BEEP-PIN-NAME");
    if (ret) {
        printk(KERN_ERR "beep: Failed to request beep-pin\n");
        return ret;
	}

	/* 6、设置PI0为输出，并且输出高电平，默认关闭LED灯 */
	ret = gpio_direction_output(beep.beep_pin, 1);
	if(ret < 0) {
		printk("can't set gpio!\r\n");
	}
	
	/* 2、初始化cdev */
	beep.cdev.owner = THIS_MODULE;
	cdev_init(&beep.cdev, &beep_fops);
	
	/* 3、添加一个cdev */
	ret = cdev_add(&beep.cdev, beep.devid, 1);
	if(ret < 0)
		goto del_unregister;
		

	/* 5、创建设备 */
	beep.device = device_create(beep_class, NULL, beep.devid, NULL, beep.dev_name);
	if (IS_ERR(beep.device)) {
		goto destroy_class;
	}
	
	return 0;

destroy_class:
	class_destroy(beep_class);
del_cdev:
	cdev_del(&beep.cdev);
del_unregister:
	unregister_chrdev_region(beep.devid, 1);
free_gpio:
	gpio_free(beep.beep_pin);
	return -EIO;
}

static void __exit chr_dev_exit(void)
{
   
	/* 注销字符设备驱动 */
	cdev_del(&beep.cdev);/*  删除cdev */
	unregister_chrdev_region(beep.devid, 1); /* 注销设备号 */

	device_destroy(beep_class, beep.devid);
	class_destroy(beep_class);

	gpio_free(beep.beep_pin); /* 释放GPIO */
}

module_init(chr_dev_init);
module_exit(chr_dev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Robert");
MODULE_INFO(intree, "Y");