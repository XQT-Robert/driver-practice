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
#include <linux/of_gpio.h>
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <linux/i2c.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include "ap3216creg.h"

//-------------------------------------------------------
//properties

#define MODULE_NAME		"ap3216c" 		/* 模块名称 */
#define DEV_MAX_CNT 	 1				/* 驱动最高复用设备数，即挂载/dev/的节点数 */

/* 驱动静态全局属性 */
static struct class *ap3216c_class; 	/* 表示当前驱动代表的类型，可复用只创建一个 */
static dev_t module_devid;				/* 驱动模块的设备号，即第一个设备的设备号 */
static int devid_major;					
//static int dev_num = 0;					/* 复用当前驱动的设备数，互斥访问 */

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
struct ap3216c_base{
	struct chr_base parent;  		/* 按键对应的input_dev指针 */
	struct i2c_client *client;		/* i2c 设备 */

	unsigned short ir, als, ps;		/* 三个光传感器数据 */
};

//不适合，静态分配贯穿整个驱动模块周期，使用动态分devm_kzalloc，设备卸载时释放
//static struct ap3216c_base ap3216c_dev;

static struct chr_base *pBase_chr_ap3216c ;

//end of properties
//-------------------------------------------------------
//operations

/*
 * @description	: 从ap3216c读取多个寄存器数据
 * @param - dev:  ap3216c设备
 * @param - reg:  要读取的寄存器首地址
 * @param - val:  读取到的数据
 * @param - len:  要读取的数据长度
 * @return 		: 操作结果
 */
static int ap3216c_read_regs(struct ap3216c_base *dev, u8 reg, void *val, int len)
{
	int ret;
	struct i2c_msg msg[2];
	struct i2c_client *client = (struct i2c_client *)dev->client;

	/* msg[0]为发送要读取的首地址 */
	msg[0].addr = client->addr;			/* ap3216c地址 */
	msg[0].flags = 0;					/* 标记为发送数据 */
	msg[0].buf = &reg;					/* 读取的首地址 */
	msg[0].len = 1;						/* reg长度*/

	/* msg[1]读取数据 */
	msg[1].addr = client->addr;			/* ap3216c地址 */
	msg[1].flags = I2C_M_RD;			/* 标记为读取数据*/
	msg[1].buf = val;					/* 读取数据缓冲区 */
	msg[1].len = len;					/* 要读取的数据长度*/

	ret = i2c_transfer(client->adapter, msg, 2);
	if(ret == 2) {
		ret = 0;
	} else {
		printk("i2c rd faiap3216c=%d reg=%06x len=%d\n",ret, reg, len);
		ret = -EREMOTEIO;
	}
	return ret;
}

/*
 * @description	: 向ap3216c多个寄存器写入数据
 * @param - dev:  ap3216c设备
 * @param - reg:  要写入的寄存器首地址
 * @param - val:  要写入的数据缓冲区
 * @param - len:  要写入的数据长度
 * @return 	  :   操作结果
 */
static s32 ap3216c_write_regs(struct ap3216c_base *dev, u8 reg, u8 *buf, u8 len)
{
	u8 b[256];
	struct i2c_msg msg;
	struct i2c_client *client = (struct i2c_client *)dev->client;
	
	b[0] = reg;					/* 寄存器首地址 */
	memcpy(&b[1],buf,len);		/* 将要写入的数据拷贝到数组b里面 */
		
	msg.addr = client->addr;	/* ap3216c地址 */
	msg.flags = 0;				/* 标记为写数据 */

	msg.buf = b;				/* 要写入的数据缓冲区 */
	msg.len = len + 1;			/* 要写入的数据长度 */

	return i2c_transfer(client->adapter, &msg, 1);
}

/*
 * @description	: 读取ap3216c指定寄存器值，读取一个寄存器
 * @param - dev:  ap3216c设备
 * @param - reg:  要读取的寄存器
 * @return 	  :   读取到的寄存器值
 */
static unsigned char ap3216c_read_reg(struct ap3216c_base *dev, u8 reg)
{
	u8 data = 0;

	ap3216c_read_regs(dev, reg, &data, 1);
	return data;
}

/*
 * @description	: 向ap3216c指定寄存器写入指定的值，写一个寄存器
 * @param - dev:  ap3216c设备
 * @param - reg:  要写的寄存器
 * @param - data: 要写入的值
 * @return   :    无
 */
static void ap3216c_write_reg(struct ap3216c_base *dev, u8 reg, u8 data)
{
	u8 buf = 0;
	buf = data;
	ap3216c_write_regs(dev, reg, &buf, 1);
}

/*
 * @description	: 读取AP3216C的数据，读取原始数据，包括ALS,PS和IR, 注意！
 *				: 如果同时打开ALS,IR+PS的话两次数据读取的时间间隔要大于112.5ms
 * @param - ir	: ir数据
 * @param - ps 	: ps数据
 * @param - ps 	: als数据 
 * @return 		: 无。
 */
void ap3216c_readdata(struct ap3216c_base *dev)
{
	unsigned char i =0;
    unsigned char buf[6];
	
	/* 循环读取所有传感器数据 */
    for(i = 0; i < 6; i++) {
        buf[i] = ap3216c_read_reg(dev, AP3216C_IRDATALOW + i);	
    }

    if(buf[0] & 0X80) 	/* IR_OF位为1,则数据无效 */
		dev->ir = 0;					
	else 				/* 读取IR传感器的数据   		*/
		dev->ir = ((unsigned short)buf[1] << 2) | (buf[0] & 0X03); 			
	
	dev->als = ((unsigned short)buf[3] << 8) | buf[2];	/* 读取ALS传感器的数据 			 */  
	
    if(buf[4] & 0x40)	/* IR_OF位为1,则数据无效 			*/
		dev->ps = 0;    													
	else 				/* 读取PS传感器的数据    */
		dev->ps = ((unsigned short)(buf[5] & 0X3F) << 4) | (buf[4] & 0X0F); 
}

/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int ap3216c_open(struct inode *inode, struct file *filp)
{
	/* 从file结构体获取cdev的指针，在根据cdev获取ap3216c_dev结构体的首地址 */
	/* 获取 cdev 并通过 container_of 获取对应结构体 */
	struct cdev *cdev;
	struct chr_base *parent;
	struct ap3216c_base *ap3216cdev;

	cdev = filp->f_path.dentry->d_inode->i_cdev;   // 获取 cdev 指针
	if (!cdev) {
    	pr_err("Failed to get cdev pointer\n");
    	return -ENODEV;
	}

	parent = container_of(cdev, struct chr_base, cdev);  // 获取 chr_base
	if (!parent) {
    	pr_err("Failed to get chr_base pointer\n");
    	return -ENODEV;
	}

	ap3216cdev = container_of(parent, struct ap3216c_base, parent); // 获取 ap3216c_base
	if (!ap3216cdev) {
    	pr_err("Failed to get ap3216c_base pointer\n");
    	return -ENODEV;
	}

	/* 你可以在这里使用 ap3216cdev 来访问相关数据 */
	/* 初始化AP3216C */
	ap3216c_write_reg(ap3216cdev, AP3216C_SYSTEMCONG, 0x04);		/* 复位AP3216C 			*/
	mdelay(50);														/* AP3216C复位最少10ms 	*/
	ap3216c_write_reg(ap3216cdev, AP3216C_SYSTEMCONG, 0X03);		/* 开启ALS、PS+IR 		*/
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
static ssize_t ap3216c_read(struct file *filp, char __user *buf, size_t cnt, loff_t *off)
{
	short data[3];
	long err = 0;

	/* 从file结构体获取cdev的指针，在根据cdev获取ap3216c_dev结构体的首地址 */
	/* 获取 cdev 并通过 container_of 获取对应结构体 */
	struct cdev *cdev;
	struct chr_base *parent;
	struct ap3216c_base *dev;

	cdev = filp->f_path.dentry->d_inode->i_cdev;   // 获取 cdev 指针
	if (!cdev) {
    	pr_err("Failed to get cdev pointer\n");
    	return -ENODEV;
	}

	parent = container_of(cdev, struct chr_base, cdev);  // 获取 chr_base
	if (!parent) {
    	pr_err("Failed to get chr_base pointer\n");
    	return -ENODEV;
	}

	dev = container_of(parent, struct ap3216c_base, parent); // 获取 ap3216c_base
	if (!dev) {
    	pr_err("Failed to get ap3216c_base pointer\n");
    	return -ENODEV;
	}

	ap3216c_readdata(dev);

	data[0] = dev->ir;
	data[1] = dev->als;
	data[2] = dev->ps;
	err = copy_to_user(buf, data, sizeof(data));
	return 0;
}

/*
 * @description		: 关闭/释放设备
 * @param - filp 	: 要关闭的设备文件(文件描述符)
 * @return 			: 0 成功;其他 失败
 */
static int ap3216c_release(struct inode *inode, struct file *filp)
{
	return 0;
}

/* AP3216C操作函数 */
static const struct file_operations ap3216c_fops = {
	.owner = THIS_MODULE,
	.open = ap3216c_open,
	.read = ap3216c_read,
	.release = ap3216c_release,
};

//end of operations
//-------------------------------------------------------
//probe

/*
 * @description		: platform驱动的probe函数，当驱动与设备匹配以后此函数就会执行
 * @param - dev 	: platform设备
 * @return 			: 0，成功;其他负值,失败
 */
static int ap3216c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{	
	int ret = 0;
	struct ap3216c_base *ap3216cdev;
	
	/* 动态申请结构体 */
	ap3216cdev = devm_kzalloc(&client->dev, sizeof(*ap3216cdev), GFP_KERNEL);
	if(!ap3216cdev)
		return -ENOMEM;

	ap3216cdev->client = client;
	/* 保存ap3216cdev结构体 */
	i2c_set_clientdata(client,ap3216cdev);
	
	/* 初始化第一个设备信息 */
	pBase_chr_ap3216c = &ap3216cdev->parent;
	pBase_chr_ap3216c->devid_minor = 0; 
	pBase_chr_ap3216c->devid = MKDEV(devid_major, pBase_chr_ap3216c->devid_minor);

	/* 设置最后一个参数作为设备挂载名称 */
	snprintf(pBase_chr_ap3216c->dev_name, sizeof(pBase_chr_ap3216c->dev_name), "ap3216c");
	pBase_chr_ap3216c->cdev.owner = THIS_MODULE;
	cdev_init(&pBase_chr_ap3216c->cdev, &ap3216c_fops);
	ret = cdev_add(&pBase_chr_ap3216c->cdev, pBase_chr_ap3216c->devid, DEV_MAX_CNT);
	if(ret < 0)
		return -EIO;
		
	pBase_chr_ap3216c->device = device_create(ap3216c_class, NULL, pBase_chr_ap3216c->devid, NULL, pBase_chr_ap3216c->dev_name);
	if (IS_ERR(pBase_chr_ap3216c->device)) {
		goto del_cdev;
	}
	
	return 0;

del_cdev:
	cdev_del(&pBase_chr_ap3216c->cdev);

	return -EIO;
}

/*
 * @description		: platform驱动的remove函数，移除platform驱动的时候此函数会执行
 * @param - dev 	: platform设备
 * @return 			: 0，成功;其他负值,失败
 */
static int ap3216c_remove(struct i2c_client *client)
{	
	cdev_del(&pBase_chr_ap3216c->cdev);/*  删除cdev */
	device_destroy(ap3216c_class, pBase_chr_ap3216c->devid);
	return 0;
}

/* 传统匹配方式ID列表 */
static const struct i2c_device_id ap3216c_id[] = {
	{"robert,ap3216c", 0},  
	{}
};

/* 设备树匹配列表 */
static const struct of_device_id ap3216c_of_match[] = {
	{ .compatible = "robert,ap3216c"},
	{ /* Sentinel */ }
};

//MODULE_DEVICE_TABLE(of, ap3216c_of_match);

/* platform驱动结构体 */
static struct i2c_driver ap3216c_driver = {
	.driver		= {
		.owner = THIS_MODULE,
		.name	= MODULE_NAME,			/* 驱动名字，用于和设备匹配 */
		.of_match_table	= ap3216c_of_match, 	/* 设备树匹配表 		 */
	},
	.probe		= ap3216c_probe,
	.remove		= ap3216c_remove,
};

//end of probe
//-------------------------------------------------------
//module

/*
 * @description	: 驱动入口函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init ap3216c_init(void)
{
	int ret = 0;

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

	ap3216c_class = class_create(THIS_MODULE, MODULE_NAME);
	if (IS_ERR(ap3216c_class)) {
		goto del_unregister;
	}

	ret = i2c_add_driver(&ap3216c_driver);
	return 0;

	//注册失败就无需单独注销class了
	//destroy_class:
		//class_destroy(ap3216c_class);
	del_unregister:
		unregister_chrdev_region(pBase_chr_ap3216c->devid, DEV_MAX_CNT);

	return ret;
}

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit ap3216c_exit(void)
{
	i2c_del_driver(&ap3216c_driver);
	unregister_chrdev_region(pBase_chr_ap3216c->devid, DEV_MAX_CNT);
	class_destroy(ap3216c_class);
	
}

/* module_i2c_driver(ap3216c_driver) */

module_init(ap3216c_init);
module_exit(ap3216c_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Robert");
MODULE_INFO(intree, "Y");