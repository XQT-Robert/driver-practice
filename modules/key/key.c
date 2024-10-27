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


#define MODULE_NAME		"key" 	/* 模块名称 */
#define DEV_MAX_CNT 	 1			/* 驱动最高复用设备数，即挂载/dev/的节点数 */

#define keyOFF 		 	 0				/* 关灯 */
#define keyON 			 1				/* 开灯 */


struct class *key_class; 			/* 表示当前驱动代表的类型，可复用只创建一个 */
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
	int key_pin; 
	struct mutex key_lock;
};

struct chr_dev key_dev;


/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int key_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &key_dev; /* 设置私有数据 */

		/* 获取互斥体,可以被信号打断 */
	if (mutex_lock_interruptible(&key_dev.key_lock)) {
		return -ERESTARTSYS;
	}
	#if 0
		mutex_lock(&key_dev.lock);	/* 不能被信号打断 */
	#endif

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
static ssize_t key_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
	int value = 0;
	int ret= 0;

	struct chr_dev *dev = filp->private_data;

	if (gpio_get_value(dev->key_pin) == 0) { 		/* key0按下 */
		while(!gpio_get_value(dev->key_pin));		/* 等待按键释放 */
		value = 0;
		ret = copy_to_user(buf, &value, sizeof(value));
	} else {	
		value = 1;
		ret = copy_to_user(buf, &value, sizeof(value));		/* 无效的按键值 */
	}
	
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
static ssize_t key_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
	return 0;
}

/*
 * @description		: 关闭/释放设备
 * @param - filp 	: 要关闭的设备文件(文件描述符)
 * @return 			: 0 成功;其他 失败
 */
static int key_release(struct inode *inode, struct file *filp)
{
	struct chr_dev *dev = filp->private_data;

	/* 释放互斥锁 */
	mutex_unlock(&dev->key_lock);
	return 0;
}

static struct file_operations key_fops = {
	.owner = THIS_MODULE,
	.open = key_open,
	.release = key_release,
	.write = key_write,
	.read = key_read,

};

static int bind_devtree(void)
{
	int ret;
	const char *str;

	/* 1、find dev node */
	key_dev.nd = of_find_node_by_path("/key");
	if(key_dev.nd == NULL ){
		printk("key node nost find!\r\n");
		return -EINVAL;
	}

	/* 2、status */
	ret = of_property_read_string(key_dev.nd,"status",&str);
	if(ret < 0){
		printk("key: Failed to get status property\n");
		return -EINVAL;
	} 

	if (strcmp(str, "okay"))
        return -EINVAL;

	/* 3、 compatible */
	ret = of_property_read_string(key_dev.nd,"compatible",&str);
	if(ret < 0){
		printk("key: Failed to get compatible property\n");
		return -EINVAL;
	} 

	if (strcmp(str, "robert,key")){
		 printk("keydev: Compatible match failed\n");
		 return -EINVAL;
	}
        

	/* 4、 get pin */
	key_dev.key_pin = of_get_named_gpio(key_dev.nd, "key-pin", 0);
	if(key_dev.key_pin < 0) {
		printk("can't get key-pin");
		return -EINVAL;
	}
	printk("key-pin num = %d\r\n", key_dev.key_pin);

	/* 5.向gpio子系统申请使用GPIO */
	ret = gpio_request(key_dev.key_pin, "key-PIN-NAME");
    if (ret) {
        printk(KERN_ERR "key: Failed to request key-pin\n");
        return ret;
	}

	/* 6、设置PI0为输出，并且输出高电平，默认关闭LED灯 */
	ret = gpio_direction_output(key_dev.key_pin, 1);
	if(ret < 0) {
		printk("can't set gpio!\r\n");
		return ret;
	}

	return 0;
}

static int __init chr_dev_init(void)
{
	int ret = 0;

	mutex_init(&key_dev.key_lock);
	
	if (devid_major) {		/*  定义了设备号 */
		module_devid = MKDEV(devid_major, 0); /* 从0开始 */
		ret = register_chrdev_region(module_devid,DEV_MAX_CNT, MODULE_NAME);
		if(ret < 0) {
			pr_err("cannot register %s char driver [ret=%d]\n",MODULE_NAME, 1);
			
			/* 该设备号已经使用，返回设备忙*/
			return -EBUSY;
		}
	} else {						/* 没有定义设备号 */
		ret = alloc_chrdev_region(&module_devid, 0,DEV_MAX_CNT, MODULE_NAME);	/* 申请设备号 */
		if(ret < 0) {
			pr_err("%s Couldn't alloc_chrdev_region, ret=%d\r\n", MODULE_NAME, ret);
			
			/* 动态分配设备号失败，超出内存范围*/
			return -ENOMEM;
		}
		devid_major = MAJOR(module_devid);	/* 获取分配号的主设备号 */
	}
	printk("devid_major=%d\r\n",devid_major);	

	key_class = class_create(THIS_MODULE, MODULE_NAME);
	if (IS_ERR(key_class)) {
		goto del_cdev;
	}

	/* 绑定设备信息，仅在绑定成功后初始化设备 */
	ret = bind_devtree();
	if(ret < 0){
		pr_err("%s Couldn't bind dev-tree, ret=%d\r\n", MODULE_NAME, ret);
			
		/* 设备树信息绑定失败 */
		gpio_free(key_dev.key_pin);
		return -ENXIO;
	}

	/* 初始化第一个设备信息 */
	key_dev.devid_minor = 0;
	key_dev.devid = MKDEV(devid_major, key_dev.devid_minor);
	snprintf(key_dev.dev_name, sizeof(key_dev.dev_name), "key");

	key_dev.cdev.owner = THIS_MODULE;
	cdev_init(&key_dev.cdev, &key_fops);
	
	ret = cdev_add(&key_dev.cdev, key_dev.devid, DEV_MAX_CNT);
	if(ret < 0)
		goto destroy_class;
		
	key_dev.device = device_create(key_class, NULL, key_dev.devid, NULL, key_dev.dev_name);
	if (IS_ERR(key_dev.device)) {
		goto del_cdev;
	}
	
	return 0;

del_cdev:
	cdev_del(&key_dev.cdev);
destroy_class:
	class_destroy(key_class);
	unregister_chrdev_region(key_dev.devid, DEV_MAX_CNT);

	return -EIO;
}

static void __exit chr_dev_exit(void)
{
   
	/* 注销字符设备驱动 */
	cdev_del(&key_dev.cdev);/*  删除cdev */
	unregister_chrdev_region(key_dev.devid, DEV_MAX_CNT); /* 注销设备号 */

	device_destroy(key_class, key_dev.devid);
	class_destroy(key_class);

	gpio_free(key_dev.key_pin); /* 释放GPIO */
}

module_init(chr_dev_init);
module_exit(chr_dev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Robert");
MODULE_INFO(intree, "Y");