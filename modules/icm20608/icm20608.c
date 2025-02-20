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

 #include <linux/spi/spi.h>
 #include <linux/kernel.h>
 #include <linux/module.h>
 #include <linux/init.h>
 #include <linux/delay.h>
 #include <linux/ide.h>
 #include <linux/errno.h>
 #include <linux/platform_device.h>
 #include <linux/gpio.h>
 #include <linux/device.h>
 #include <asm/uaccess.h>
 #include <linux/cdev.h>

#include "icm20608reg.h"

//-------------------------------------------------------
//properties

#define MODULE_NAME		"icm20608" 		/* 模块名称 */
#define DEV_MAX_CNT 	 1				/* 驱动最高复用设备数，即挂载/dev/的节点数 */

/* 驱动静态全局属性 */
static struct class *icm20608_class; 	/* 表示当前驱动代表的类型，可复用只创建一个 */
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


/* 设备属性 */
struct icm20608_base{
	struct chr_base parent;  		/* 按键对应的input_dev指针 */
	struct spi_device *spi;		/* spi设备 */

	signed int gyro_x_adc;		/* 陀螺仪X轴原始值 	 */
	signed int gyro_y_adc;		/* 陀螺仪Y轴原始值		*/
	signed int gyro_z_adc;		/* 陀螺仪Z轴原始值 		*/
	signed int accel_x_adc;		/* 加速度计X轴原始值 	*/
	signed int accel_y_adc;		/* 加速度计Y轴原始值	*/
	signed int accel_z_adc;		/* 加速度计Z轴原始值 	*/
	signed int temp_adc;		/* 温度原始值 			*/
};

//不适合，静态分配贯穿整个驱动模块周期，使用动态分devm_kzalloc，设备卸载时释放
//static struct icm20608_base icm20608_base;

static struct chr_base *pBase_chr_icm20608 ;

//end of properties
//-------------------------------------------------------
//operations
/*
 * @description	: 从icm20608读取多个寄存器数据
 * @param - dev:  icm20608设备
 * @param - reg:  要读取的寄存器首地址
 * @param - val:  读取到的数据
 * @param - len:  要读取的数据长度
 * @return 		: 操作结果
 */
static int icm20608_read_regs(struct icm20608_base *dev, u8 reg, void *buf, int len)
{

	int ret = -1;
	unsigned char txdata[1];
	unsigned char * rxdata;
	struct spi_message m;
	struct spi_transfer *t;
	struct spi_device *spi = (struct spi_device *)dev->spi;
    
	t = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);	/* 申请内存 */
	if(!t) {
		return -ENOMEM;
	}
	rxdata = kzalloc(sizeof(char) * len, GFP_KERNEL);	/* 申请内存 */
	if(!rxdata) {
		goto out1;
	}
	/* 一共发送len+1个字节的数据，第一个字节为
	寄存器首地址，一共要读取len个字节长度的数据，*/
	txdata[0] = reg | 0x80;		/* 写数据的时候首寄存器地址bit8要置1 */			
	t->tx_buf = txdata;			/* 要发送的数据 */
    t->rx_buf = rxdata;			/* 要读取的数据 */
	t->len = len+1;				/* t->len=发送的长度+读取的长度 */
	spi_message_init(&m);		/* 初始化spi_message */
	spi_message_add_tail(t, &m);/* 将spi_transfer添加到spi_message队列 */
	ret = spi_sync(spi, &m);	/* 同步发送 */
	if(ret) {
		goto out2;
	}
	
    memcpy(buf , rxdata+1, len);  /* 只需要读取的数据 */

out2:
	kfree(rxdata);					/* 释放内存 */
out1:	
	kfree(t);						/* 释放内存 */
	
	return ret;
}

/*
 * @description	: 向icm20608多个寄存器写入数据
 * @param - dev:  icm20608设备
 * @param - reg:  要写入的寄存器首地址
 * @param - val:  要写入的数据缓冲区
 * @param - len:  要写入的数据长度
 * @return 	  :   操作结果
 */
static s32 icm20608_write_regs(struct icm20608_base *dev, u8 reg, u8 *buf, u8 len)
{
	int ret = -1;
	unsigned char *txdata;
	struct spi_message m;
	struct spi_transfer *t;
	struct spi_device *spi = (struct spi_device *)dev->spi;
	
	t = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);	/* 申请内存 */
	if(!t) {
		return -ENOMEM;
	}
	
	txdata = kzalloc(sizeof(char)+len, GFP_KERNEL);
	if(!txdata) {
		goto out1;
	}
	
	/* 一共发送len+1个字节的数据，第一个字节为
	寄存器首地址，len为要写入的寄存器的集合，*/
	*txdata = reg & ~0x80;	/* 写数据的时候首寄存器地址bit8要清零 */
    memcpy(txdata+1, buf, len);	/* 把len个寄存器拷贝到txdata里，等待发送 */
	t->tx_buf = txdata;			/* 要发送的数据 */
	t->len = len+1;				/* t->len=发送的长度+读取的长度 */
	spi_message_init(&m);		/* 初始化spi_message */
	spi_message_add_tail(t, &m);/* 将spi_transfer添加到spi_message队列 */
	ret = spi_sync(spi, &m);	/* 同步发送 */
    if(ret) {
        goto out2;
    }
	
out2:
	kfree(txdata);				/* 释放内存 */
out1:
	kfree(t);					/* 释放内存 */
	return ret;
}

/*
 * @description	: 读取icm20608指定寄存器值，读取一个寄存器
 * @param - dev:  icm20608设备
 * @param - reg:  要读取的寄存器
 * @return 	  :   读取到的寄存器值
 */
static unsigned char icm20608_read_onereg(struct icm20608_base *dev, u8 reg)
{
	u8 data = 0;
	icm20608_read_regs(dev, reg, &data, 1);
	return data;
}

/*
 * @description	: 向icm20608指定寄存器写入指定的值，写一个寄存器
 * @param - dev:  icm20608设备
 * @param - reg:  要写的寄存器
 * @param - data: 要写入的值
 * @return   :    无
 */	

static void icm20608_write_onereg(struct icm20608_base *dev, u8 reg, u8 value)
{
	u8 buf = value;
	icm20608_write_regs(dev, reg, &buf, 1);
}

/*
 * @description	: 读取ICM20608的数据，读取原始数据，包括三轴陀螺仪、
 * 				: 三轴加速度计和内部温度。
 * @param - dev	: ICM20608设备
 * @return 		: 无。
 */
void icm20608_readdata(struct icm20608_base *dev)
{
	unsigned char data[14];
	icm20608_read_regs(dev, ICM20_ACCEL_XOUT_H, data, 14);

	dev->accel_x_adc = (signed short)((data[0] << 8) | data[1]); 
	dev->accel_y_adc = (signed short)((data[2] << 8) | data[3]); 
	dev->accel_z_adc = (signed short)((data[4] << 8) | data[5]); 
	dev->temp_adc    = (signed short)((data[6] << 8) | data[7]); 
	dev->gyro_x_adc  = (signed short)((data[8] << 8) | data[9]); 
	dev->gyro_y_adc  = (signed short)((data[10] << 8) | data[11]);
	dev->gyro_z_adc  = (signed short)((data[12] << 8) | data[13]);
}

/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做pr似有ate_data的成员变量
 * 					  一般在open的时候将private_data似有向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int icm20608_open(struct inode *inode, struct file *filp)
{
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
static ssize_t icm20608_read(struct file *filp, char __user *buf, size_t cnt, loff_t *off)
{
	signed int data[7];
	long err = 0;

	/* 从file结构体获取cdev的指针，在根据cdev获取icm20680_dev结构体的首地址 */
	/* 获取 cdev 并通过 container_of 获取对应结构体 */
	struct cdev *cdev;
	struct chr_base *parent;
	struct icm20608_base *dev;

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

	dev = container_of(parent, struct icm20608_base, parent); // 获取 icm20608_base
	if (!dev) {
    	pr_err("Failed to get icm20608_base pointer\n");
    	return -ENODEV;
	}
            
	icm20608_readdata(dev);
	data[0] = dev->gyro_x_adc;
	data[1] = dev->gyro_y_adc;
	data[2] = dev->gyro_z_adc;
	data[3] = dev->accel_x_adc;
	data[4] = dev->accel_y_adc;
	data[5] = dev->accel_z_adc;
	data[6] = dev->temp_adc;
	err = copy_to_user(buf, data, sizeof(data));
	return 0;
}

/*
 * @description		: 关闭/释放设备
 * @param - filp 	: 要关闭的设备文件(文件描述符)
 * @return 			: 0 成功;其他 失败
 */
static int icm20608_release(struct inode *inode, struct file *filp)
{
	return 0;
}

/* icm20608操作函数 */
static const struct file_operations icm20608_fops = {
	.owner = THIS_MODULE,
	.open = icm20608_open,
	.read = icm20608_read,
	.release = icm20608_release,
};

//end of operations
//-------------------------------------------------------
//probe

/*
 * ICM20608内部寄存器初始化函数 
 * @param - spi : 要操作的设备
 * @return 	: 无
 */
void icm20608_reginit(struct icm20608_base *dev)
{
	u8 value = 0;
	
	icm20608_write_onereg(dev, ICM20_PWR_MGMT_1, 0x80);
	mdelay(50);
	icm20608_write_onereg(dev, ICM20_PWR_MGMT_1, 0x01);
	mdelay(50);

	value = icm20608_read_onereg(dev, ICM20_WHO_AM_I);
	printk("ICM20608 ID = %#X\r\n", value);	

	icm20608_write_onereg(dev, ICM20_SMPLRT_DIV, 0x00); 	/* 输出速率是内部采样率					*/
	icm20608_write_onereg(dev, ICM20_GYRO_CONFIG, 0x18); 	/* 陀螺仪±2000dps量程 				*/
	icm20608_write_onereg(dev, ICM20_ACCEL_CONFIG, 0x18); 	/* 加速度计±16G量程 					*/
	icm20608_write_onereg(dev, ICM20_CONFIG, 0x04); 		/* 陀螺仪低通滤波BW=20Hz 				*/
	icm20608_write_onereg(dev, ICM20_ACCEL_CONFIG2, 0x04); /* 加速度计低通滤波BW=21.2Hz 			*/
	icm20608_write_onereg(dev, ICM20_PWR_MGMT_2, 0x00); 	/* 打开加速度计和陀螺仪所有轴 				*/
	icm20608_write_onereg(dev, ICM20_LP_MODE_CFG, 0x00); 	/* 关闭低功耗 						*/
	icm20608_write_onereg(dev, ICM20_FIFO_EN, 0x00);		/* 关闭FIFO						*/
}

/*
 * @description		: platform驱动的probe函数，当驱动与设备匹配以后此函数就会执行
 * @param - dev 	: platform设备
 * @return 			: 0，成功;其他负值,失败
 */
static int icm20608_probe(struct spi_device *spi)
{	
	int ret = 0;
	struct icm20608_base *icm20608dev;

	/* 动态申请结构体 */
	icm20608dev = devm_kzalloc(&spi->dev, sizeof(*icm20608dev), GFP_KERNEL);
	if(!icm20608dev)
		return -ENOMEM;
	
	icm20608dev->spi = spi;
	
	/*初始化spi_device */
	spi->mode = SPI_MODE_0;	/*MODE0，CPOL=0，CPHA=0*/
	spi_setup(spi);
				
	/* 初始化ICM20608内部寄存器 */
	icm20608_reginit(icm20608dev);	
	/* 保存icm20608dev结构体 */
	spi_set_drvdata(spi, icm20608dev);
	
	/* 初始化第一个设备信息 */
	pBase_chr_icm20608 = &icm20608dev->parent;
	pBase_chr_icm20608->devid_minor = 0; 
	pBase_chr_icm20608->devid = MKDEV(devid_major, pBase_chr_icm20608->devid_minor);

	/* 设置最后一个参数作为设备挂载名称 */
	snprintf(pBase_chr_icm20608->dev_name, sizeof(pBase_chr_icm20608->dev_name), "icm20608");
	pBase_chr_icm20608->cdev.owner = THIS_MODULE;
	cdev_init(&pBase_chr_icm20608->cdev, &icm20608_fops);
	ret = cdev_add(&pBase_chr_icm20608->cdev, pBase_chr_icm20608->devid, DEV_MAX_CNT);
	if(ret < 0)
		return -EIO;
		
	pBase_chr_icm20608->device = device_create(icm20608_class, NULL, pBase_chr_icm20608->devid, NULL, pBase_chr_icm20608->dev_name);
	if (IS_ERR(pBase_chr_icm20608->device)) {
		goto del_cdev;
	}


	
	return 0;

del_cdev:
	cdev_del(&pBase_chr_icm20608->cdev);

	return -EIO;
}

/*
 * @description		: platform驱动的remove函数，移除platform驱动的时候此函数会执行
 * @param - dev 	: platform设备
 * @return 			: 0，成功;其他负值,失败
 */
static int icm20608_remove(struct spi_device *spi)
{	
	cdev_del(&pBase_chr_icm20608->cdev);/*  删除cdev */
	device_destroy(icm20608_class, pBase_chr_icm20608->devid);
	return 0;
}

/* 传统匹配方式ID列表 */
static const struct i2c_device_id icm20608_id[] = {
	{"robert,icm20608", 0},  
	{}
};

/* 设备树匹配列表 */
static const struct of_device_id icm20608_of_match[] = {
	{ .compatible = "robert,icm20608"},
	{ /* Sentinel */ }
};

//MODULE_DEVICE_TABLE(of, icm20608_of_match);

/* platform驱动结构体 */
static struct spi_driver icm20608_driver = {
	.driver		= {
		.owner = THIS_MODULE,
		.name	= MODULE_NAME,			/* 驱动名字，用于和设备匹配 */
		.of_match_table	= icm20608_of_match, 	/* 设备树匹配表 		 */
	},
	.probe		= icm20608_probe,
	.remove		= icm20608_remove,
};

//end of probe
//-------------------------------------------------------
//module

/*
 * @description	: 驱动入口函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init icm20608_init(void)
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

	icm20608_class = class_create(THIS_MODULE, MODULE_NAME);
	if (IS_ERR(icm20608_class)) {
		goto del_unregister;
	}

	return spi_register_driver(&icm20608_driver);
	
	//注册失败就无需单独注销class了
	//destroy_class:
		//class_destroy(icm20608_class);
	del_unregister:
		unregister_chrdev_region(pBase_chr_icm20608->devid, DEV_MAX_CNT);

	return ret;
}

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit icm20608_exit(void)
{
	spi_unregister_driver(&icm20608_driver);
	unregister_chrdev_region(pBase_chr_icm20608->devid, DEV_MAX_CNT);
	class_destroy(icm20608_class);
	
}

/* module_i2c_driver(icm20608_driver) */

module_init(icm20608_init);
module_exit(icm20608_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Robert");
MODULE_INFO(intree, "Y");