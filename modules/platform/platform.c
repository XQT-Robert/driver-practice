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

/**
 * @brief:模块需要修改的内容简介
 * @include：
 * 		 |  修改名称  | 类型 |    作用		|	
 * 		1、MODULE_NAME 	宏 		模块名称
 * 		2、DEV_MAX_CNT 	宏 		设备数
 * 		3、struct class	类 		设备类型
 */

#define MODULE_NAME		"unblock_led" 	/* 模块名称 */
#define DEV_MAX_CNT 	 1			/* 驱动最高复用设备数，即挂载/dev/的节点数 */

/* 映射后的寄存器虚拟地址指针 */
static void __iomem *MPU_AHB4_PERIPH_RCC_PI;
static void __iomem *GPIOI_MODER_PI;
static void __iomem *GPIOI_OTYPER_PI;
static void __iomem *GPIOI_OSPEEDR_PI;
static void __iomem *GPIOI_PUPDR_PI;
static void __iomem *GPIOI_BSRR_PI;

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
	atomic_t status;   				
	
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
	u32 val = 0;
	if(sta == LED_ON) {
		val = readl(GPIOI_BSRR_PI);
		val |= (1 << 16);	
		writel(val, GPIOI_BSRR_PI);
	}else if(sta == LED_OFF) {
		val = readl(GPIOI_BSRR_PI);
		val|= (1 << 0);	
		writel(val, GPIOI_BSRR_PI);
	}	
}

/*
 * @description		: 取消映射
 * @return 			: 无
 */
void led_unmap(void)
{
		/* 取消映射 */
	iounmap(MPU_AHB4_PERIPH_RCC_PI);
	iounmap(GPIOI_MODER_PI);
	iounmap(GPIOI_OTYPER_PI);
	iounmap(GPIOI_OSPEEDR_PI);
	iounmap(GPIOI_PUPDR_PI);
	iounmap(GPIOI_BSRR_PI);
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
	unsigned char ledstat;
	
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
/*
 * @description		: platform驱动的probe函数，当驱动与设备匹配以后此函数就会执行
 * @param - dev 	: platform设备
 * @return 			: 0，成功;其他负值,失败
 */
static int led_probe(struct platform_device *dev)
{	
	int i = 0, ret;
	int ressize[6];
	u32 val = 0;
	struct resource *ledsource[6];

	printk("led driver and device has matched!\r\n");
	/* 1、获取资源 */
	for (i = 0; i < 6; i++) {
		ledsource[i] = platform_get_resource(dev, IORESOURCE_MEM, i); /* 依次MEM类型资源 */
		if (!ledsource[i]) {
			dev_err(&dev->dev, "No MEM resource for always on\n");
			return -ENXIO;
		}
		ressize[i] = resource_size(ledsource[i]);	
	}	

	/* 2、初始化LED */
	/* 寄存器地址映射 */
 	MPU_AHB4_PERIPH_RCC_PI = ioremap(ledsource[0]->start, ressize[0]);
	GPIOI_MODER_PI = ioremap(ledsource[1]->start, ressize[1]);
  	GPIOI_OTYPER_PI = ioremap(ledsource[2]->start, ressize[2]);
	GPIOI_OSPEEDR_PI = ioremap(ledsource[3]->start, ressize[3]);
	GPIOI_PUPDR_PI = ioremap(ledsource[4]->start, ressize[4]);
	GPIOI_BSRR_PI = ioremap(ledsource[5]->start, ressize[5]);
	
	/* 3、使能PI时钟 */
    val = readl(MPU_AHB4_PERIPH_RCC_PI);
    val &= ~(0X1 << 8); /* 清除以前的设置 */
    val |= (0X1 << 8);  /* 设置新值 */
    writel(val, MPU_AHB4_PERIPH_RCC_PI);

    /* 4、设置PI0通用的输出模式。*/
    val = readl(GPIOI_MODER_PI);
    val &= ~(0X3 << 0); /* bit0:1清零 */
    val |= (0X1 << 0);  /* bit0:1设置01 */
    writel(val, GPIOI_MODER_PI);

    /* 5、设置PI0为推挽模式。*/
    val = readl(GPIOI_OTYPER_PI);
    val &= ~(0X1 << 0); /* bit0清零，设置为上拉*/
    writel(val, GPIOI_OTYPER_PI);

    /* 6、设置PI0为高速。*/
    val = readl(GPIOI_OSPEEDR_PI);
    val &= ~(0X3 << 0); /* bit0:1 清零 */
    val |= (0x2 << 0); /* bit0:1 设置为10*/
    writel(val, GPIOI_OSPEEDR_PI);

    /* 7、设置PI0为上拉。*/
    val = readl(GPIOI_PUPDR_PI);
    val &= ~(0X3 << 0); /* bit0:1 清零*/
    val |= (0x1 << 0); /*bit0:1 设置为01*/
    writel(val,GPIOI_PUPDR_PI);

    /* 8、默认关闭LED */
    val = readl(GPIOI_BSRR_PI);
    val |= (0x1 << 0);
    writel(val, GPIOI_BSRR_PI);
	
	/* 注册字符设备驱动 */
	/* 1、申请设备号 */
	if (devid_major) {		/*  定义了设备号 */
		module_devid = MKDEV(devid_major, 0); /* 从0开始 */
		ret = register_chrdev_region(module_devid,DEV_MAX_CNT, MODULE_NAME);
		if(ret < 0) {
			pr_err("cannot register %s char driver [ret=%d]\n",MODULE_NAME, 1);
			goto fail_map;
			/* 该设备号已经使用，返回设备忙*/
			return -EBUSY;
		}
	} else {						/* 没有定义设备号 */
		ret = alloc_chrdev_region(&module_devid, 0,DEV_MAX_CNT, MODULE_NAME);	
		if(ret < 0) {
			pr_err("%s Couldn't alloc_chrdev_region, ret=%d\r\n", MODULE_NAME, ret);
			goto fail_map;
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
	
	pBase_chr_led = &led_dev.parent;
	/* 初始化第一个设备信息 */
	pBase_chr_led->devid_minor = 0;
	pBase_chr_led->devid = MKDEV(devid_major, pBase_chr_led->devid_minor);
	snprintf(pBase_chr_led->dev_name, sizeof(pBase_chr_led->dev_name), "led");

	pBase_chr_led->cdev.owner = THIS_MODULE;
	cdev_init(&pBase_chr_led->cdev, &led_fops);
	
	ret = cdev_add(&pBase_chr_led->cdev, pBase_chr_led->devid, DEV_MAX_CNT);
	if(ret < 0)
		goto destroy_class;
		
	pBase_chr_led->device = device_create(led_class, NULL, pBase_chr_led->devid, NULL, pBase_chr_led->dev_name);
	if (IS_ERR(pBase_chr_led->device)) {
		goto del_cdev;
	}

	return 0;
	
del_cdev:
	cdev_del(&pBase_chr_led->cdev);
destroy_class:
	class_destroy(led_class);
del_unregister:
	unregister_chrdev_region(pBase_chr_led->devid, DEV_MAX_CNT);
fail_map:
	led_unmap();
	return -EIO;
}

/*
 * @description		: platform驱动的remove函数，移除platform驱动的时候此函数会执行
 * @param - dev 	: platform设备
 * @return 			: 0，成功;其他负值,失败
 */
static int led_remove(struct platform_device *dev)
{
	led_unmap();	/* 取消映射 */
		/* 注销字符设备驱动 */
	cdev_del(&pBase_chr_led->cdev);/*  删除cdev */
	unregister_chrdev_region(pBase_chr_led->devid, DEV_MAX_CNT); /* 注销设备号 */
	device_destroy(led_class, pBase_chr_led->devid);
	class_destroy(led_class);
	return 0;
}

/* platform驱动结构体 */
static struct platform_driver led_driver = {
	.driver		= {
		.name	= "stm32mp1-led",			/* 驱动名字，用于和设备匹配 */
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