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
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>


static void __iomem *MPU_AHB4_PERIPH_RCC_PI;
static void __iomem *GPIOI_MODER_PI;
static void __iomem *GPIOI_OTYPER_PI;
static void __iomem *GPIOI_OSPEEDR_PI;
static void __iomem *GPIOI_PUPDR_PI;
static void __iomem *GPIOI_BSRR_PI; 




#define LED_NAME		"led_devtree" 	/* 设备名字 */

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
};

struct led_dev led_devtree;


/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int led_open(struct inode *inode, struct file *filp)
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
static ssize_t led_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
	return 0;
}

void led_switch(u8 sta)
{
	u32 val = 0;
	if(sta == LEDON) {
		val = readl(GPIOI_BSRR_PI);
		val |= (1 << 16);	
		writel(val, GPIOI_BSRR_PI);
	}else if(sta == LEDOFF) {
		val = readl(GPIOI_BSRR_PI);
		val|= (1 << 0);	
		writel(val, GPIOI_BSRR_PI);
	}	
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

	ret = copy_from_user(databuf, buf, cnt);
	if(ret < 0) {
		printk("kernel write failed!\r\n");
		return -EFAULT;
	}

	ledstat = databuf[0];		/* 获取状态值 */

	if(ledstat == LEDON) {	
		led_switch(LEDON);		/* 打开LED灯 */
	} else if(ledstat == LEDOFF) {
		led_switch(LEDOFF);		/* 关闭LED灯 */
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


static struct file_operations led_fops = {
	.owner = THIS_MODULE,
	.open = led_open,
	.release = led_release,
	.write = led_write,
	.read = led_read,

};

static int __init led_init(void)
{
	u32 val;
	int ret = 0;
	struct	property *led_proper;
	const char *str;
	u32 reg[12];

	led_devtree.nd = of_find_node_by_path("/stm32mp1_led");
	if(led_devtree.nd == NULL ){
		printk("stm32mp1_led node nost find!\r\n");
		return -EINVAL;
	} else {
		printk("stm32mp1_led node find!\r\n");
	}

	led_proper = of_find_property(led_devtree.nd, "compatible", NULL);
	if(led_proper == NULL) {
		printk("compatible property find failed\r\n");
	} else {
		printk("compatible = %s\r\n", (char*)led_proper->value);
	}

	ret = of_property_read_string(led_devtree.nd,"status",&str);
	if(ret < 0){
		printk("status read failed!\r\n");
	} else {
		printk("status = %s\r\n",str);
	}

		/* 4、获取reg属性内容 */
	ret = of_property_read_u32_array(led_devtree.nd, "reg", reg, 12);
	if(ret < 0) {
		printk("reg property read failed!\r\n");
	} else {
		u8 i = 0;
		printk("reg data:\r\n");
		for(i = 0; i < 12; i++)
			printk("%#X ", reg[i]);
		printk("\r\n");
	}

	/* 初始化LED */
	/* 1、寄存器地址映射 */
	MPU_AHB4_PERIPH_RCC_PI = of_iomap(led_devtree.nd, 0);
  	GPIOI_MODER_PI = of_iomap(led_devtree.nd, 1);
	GPIOI_OTYPER_PI = of_iomap(led_devtree.nd, 2);
	GPIOI_OSPEEDR_PI = of_iomap(led_devtree.nd, 3);
	GPIOI_PUPDR_PI = of_iomap(led_devtree.nd, 4);
	GPIOI_BSRR_PI = of_iomap(led_devtree.nd, 5);

/* 2、使能PI时钟 */
    val = readl(MPU_AHB4_PERIPH_RCC_PI);
    val &= ~(0X1 << 8); /* 清除以前的设置 */
    val |= (0X1 << 8);  /* 设置新值 */
    writel(val, MPU_AHB4_PERIPH_RCC_PI);

    /* 3、设置PI0通用的输出模式。*/
    val = readl(GPIOI_MODER_PI);
    val &= ~(0X3 << 0); /* bit0:1清零 */
    val |= (0X1 << 0);  /* bit0:1设置01 */
    writel(val, GPIOI_MODER_PI);

    /* 3、设置PI0为推挽模式。*/
    val = readl(GPIOI_OTYPER_PI);
    val &= ~(0X1 << 0); /* bit0清零，设置为上拉*/
    writel(val, GPIOI_OTYPER_PI);

    /* 4、设置PI0为高速。*/
    val = readl(GPIOI_OSPEEDR_PI);
    val &= ~(0X3 << 0); /* bit0:1 清零 */
    val |= (0x2 << 0); /* bit0:1 设置为10*/
    writel(val, GPIOI_OSPEEDR_PI);

    /* 5、设置PI0为上拉。*/
    val = readl(GPIOI_PUPDR_PI);
    val &= ~(0X3 << 0); /* bit0:1 清零*/
    val |= (0x1 << 0); /*bit0:1 设置为01*/
    writel(val,GPIOI_PUPDR_PI);

    /* 6、默认关闭LED */
    val = readl(GPIOI_BSRR_PI);
    val |= (0x1 << 0);
    writel(val, GPIOI_BSRR_PI);

	/* 注册字符设备驱动 */
	/* 1、创建设备号 */
	if (led_devtree.major) {		/*  定义了设备号 */
		led_devtree.devid = MKDEV(led_devtree.major, 0);
		ret = register_chrdev_region(led_devtree.devid, 1, LED_NAME);
		if(ret < 0) {
			pr_err("cannot register %s char driver [ret=%d]\n",LED_NAME, 1);
			goto fail_map;
		}
	} else {						/* 没有定义设备号 */
		ret = alloc_chrdev_region(&led_devtree.devid, 0, 1, LED_NAME);	/* 申请设备号 */
		if(ret < 0) {
			pr_err("%s Couldn't alloc_chrdev_region, ret=%d\r\n", LED_NAME, ret);
			goto fail_map;
		}
		led_devtree.major = MAJOR(led_devtree.devid);	/* 获取分配号的主设备号 */
		led_devtree.minor = MINOR(led_devtree.devid);	/* 获取分配号的次设备号 */
	}
	printk("newcheled major=%d,minor=%d\r\n",led_devtree.major, led_devtree.minor);	
	
	/* 2、初始化cdev */
	led_devtree.cdev.owner = THIS_MODULE;
	cdev_init(&led_devtree.cdev, &led_fops);
	
	/* 3、添加一个cdev */
	ret = cdev_add(&led_devtree.cdev, led_devtree.devid, 1);
	if(ret < 0)
		goto del_unregister;
		
	/* 4、创建类 */
	led_devtree.class = class_create(THIS_MODULE, LED_NAME);
	if (IS_ERR(led_devtree.class)) {
		goto del_cdev;
	}

	/* 5、创建设备 */
	led_devtree.device = device_create(led_devtree.class, NULL, led_devtree.devid, NULL, LED_NAME);
	if (IS_ERR(led_devtree.device)) {
		goto destroy_class;
	}
	
	return 0;

destroy_class:
	class_destroy(led_devtree.class);
del_cdev:
	cdev_del(&led_devtree.cdev);
del_unregister:
	unregister_chrdev_region(led_devtree.devid, 1);
fail_map:
	led_unmap();
	return -EIO;
}

static void __exit led_exit(void)
{
	/* 取消映射 */
   led_unmap();
   
	/* 注销字符设备驱动 */
	cdev_del(&led_devtree.cdev);/*  删除cdev */
	unregister_chrdev_region(led_devtree.devid, 1); /* 注销设备号 */

	device_destroy(led_devtree.class, led_devtree.devid);
	class_destroy(led_devtree.class);
}

module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Robert");
MODULE_INFO(intree, "Y");