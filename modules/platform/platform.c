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

#include <linux/sched.h>
#include <linux/pid.h>


#define MODULE_NAME		"unblock_key" 	/* 模块名称 */
#define DEV_MAX_CNT 	 1			/* 驱动最高复用设备数，即挂载/dev/的节点数 */

/* 定义按键状态 */
enum key_status {
    KEY_PRESS = 0,      /* 按键按下 */ 
    KEY_RELEASE,        /* 按键松开 */ 
    KEY_KEEP,           /* 按键状态保持 */ 
};

/* 驱动全局属性 */
struct class *key_class; 			/* 表示当前驱动代表的类型，可复用只创建一个 */
dev_t module_devid;					/* 驱动模块的设备号，即第一个设备的设备号 */
int devid_major;					
int dev_num = 0;					/* 复用当前驱动的设备数，互斥访问 */


/* 字符型设备属性 */
struct chr_class{ 
	dev_t devid;					
	struct cdev cdev;
	struct device *device; 
	int devid_minor;				
	struct device_node *nd; 		
	char dev_name[20]; 				/* /dev/下挂载的设备名称 */
};

/* 时钟结构 */
struct timer_base{
	struct timer_list timer;		
	int timeperiod; 				/* 定时周期,单位为ms */
	spinlock_t timer_spinlock;		
};

/* 按键属性 */
struct key_class{
	struct chr_class parent;			/* 继承字符型设备属性 */
	int key_pin; 
	int irq_num;					/* 中断号 		*/
	atomic_t status;   				
	struct timer_base timer;	


	wait_queue_head_t r_wait;	/* 读等待队列头 */
	struct fasync_struct *async_queue;	/* fasync_struct结构体 */
};

static struct key_class key_dev;
static struct chr_class *p_chr_key;		/* 指针_设备父类_所属设备 */
static struct timer_base *p_timer_key;	/* 指针_设备父类_所属设备 */


static int key_open(struct inode *inode, struct file *filp)
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
static ssize_t key_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
    int ret;

	if (filp->f_flags & O_NONBLOCK) {	// 非阻塞方式访问
        if(KEY_KEEP == atomic_read(&key_dev.status))
            return -EAGAIN;
    } 
	else {							// 阻塞方式访问
   		/* 加入等待队列，当有按键按下或松开动作发生时，才会被唤醒 */
		ret = wait_event_interruptible(key_dev.r_wait, KEY_KEEP != atomic_read(&key_dev.status));
		if(ret)
			return ret;
	}
    /* 将按键状态信息发送给应用程序 */
    ret = copy_to_user(buf, &key_dev.status, sizeof(int));
	
    /* 状态重置 */
    atomic_set(&key_dev.status, KEY_KEEP);


    return ret;
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
 * @description     : poll函数，用于处理非阻塞访问
 * @param - filp    : 要打开的设备文件(文件描述符)
 * @param - wait    : 等待列表(poll_table)
 * @return          : 设备或者资源状态，
 */
static unsigned int key_poll(struct file *filp, struct poll_table_struct *wait)
{
	unsigned int mask = 0;

    poll_wait(filp, &key_dev.r_wait, wait);

    if(KEY_KEEP != atomic_read(&key_dev.status))	// 按键按下或松开动作发生
		mask = POLLIN | POLLRDNORM;	// 返回PLLIN

    return mask;
}

 /** 
  * @description	: fasync函数，用于处理异步通知
  * @param – fd		: 文件描述符
  * @param – filp	: 要打开的设备文件(文件描述符)
  * @param – on		: 模式
  * @return			: 负数表示函数执行失败
  */
static int key_fasync(int fd, struct file *filp, int on)
{
	return fasync_helper(fd, filp, on, &key_dev.async_queue);
}

/*
 * @description		: 关闭/释放设备
 * @param - filp 	: 要关闭的设备文件(文件描述符)
 * @return 			: 0 成功;其他 失败
 */
static int key_release(struct inode *inode, struct file *filp)
{
	return key_fasync(-1, filp, 0);
}


static struct file_operations key_fops = {
	.owner = THIS_MODULE,
	.open = key_open,
	.release = key_release,
	.write = key_write,
	.read = key_read,
	.poll = key_poll,
	.fasync	= key_fasync,
};

/* 中断返回 */
static irqreturn_t key_irq_callback(int irq, void *dev_id)
{
	/* 按键防抖处理，开启定时器延时15ms */
	mod_timer(&p_timer_key->timer, jiffies + msecs_to_jiffies(15));
    return IRQ_HANDLED;
}

static void p_timer_key_irq_callback(struct timer_list *arg)
{
    static int last_val = 1;
    int current_val;

    /* 读取按键值并判断按键当前状态 */
    current_val = gpio_get_value(key_dev.key_pin);
    if (0 == current_val && last_val){    		/* 按下 */ 
		atomic_set(&key_dev.status, KEY_PRESS);
		wake_up_interruptible(&key_dev.r_wait);	// 唤醒r_wait队列头中的所有队列

		/* 利用信号机制异步通知 */
		if(key_dev.async_queue)
			kill_fasync(&key_dev.async_queue, SIGIO, POLL_IN);
	}	
    else if (1 == current_val && !last_val){	/* 松开 */ 
		atomic_set(&key_dev.status, KEY_RELEASE);
		wake_up_interruptible(&key_dev.r_wait);	// 唤醒r_wait队列头中的所有队列

		/* 利用信号机制异步通知 */
		if(key_dev.async_queue)
			kill_fasync(&key_dev.async_queue, SIGIO, POLL_IN);
	} 			
    else{										/* 状态保持 */ 
		atomic_set(&key_dev.status, KEY_KEEP);
	}       	

    last_val = current_val;

}

static int key_gpio_init(void)
{
	int ret;
	unsigned long irq_flags;

	/* 5.向gpio子系统申请使用GPIO */
	ret = gpio_request(key_dev.key_pin, "key0");
    if (ret) {
        printk(KERN_ERR "key: Failed to request key-pin\n");
        return ret;
	}

	gpio_direction_input(key_dev.key_pin);
	
	/* 获取设备树中指定的中断触发类型 */
	irq_flags = irq_get_trigger_type(key_dev.irq_num);
	if (IRQF_TRIGGER_NONE == irq_flags)
		irq_flags = IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING;
		
	/* 申请中断 */
	ret = request_irq(key_dev.irq_num, key_irq_callback, irq_flags, "Key0_IRQ", NULL);
	if (ret) {
        gpio_free(key_dev.key_pin);
        return ret;
    }

	return 0;
}

/* 设备属性初始化 */
static int key_dev_init(void)
{
	int ret;

	ret = key_gpio_init();
	if(ret){
		free_irq(key_dev.irq_num, NULL);
		gpio_free(key_dev.key_pin);
		return ret;
	}
	
	/* 将父类指针指向设备父类属性方便访问 */
	p_timer_key = &key_dev.timer;

	/* 初始化等待队列头 */
	init_waitqueue_head(&key_dev.r_wait);

	/* 初始化按键状态 */
	atomic_set(&key_dev.status, KEY_KEEP);

	/* 6、初始化timer，设置定时器处理函数,还未设置周期，所有不会激活定时器 */
	timer_setup(&p_timer_key->timer, p_timer_key_irq_callback, 0);

	return 0;
}


static int parse_devtree(void)
{
	int ret;
	const char *str;

	/* 将父类指针指向设备父类属性方便访问 */
	p_chr_key = &key_dev.parent;	

	/* 1、find dev node */
	p_chr_key->nd = of_find_node_by_path("/key");
	if(p_chr_key->nd == NULL ){
		printk("key node nost find!\r\n");
		return -EINVAL;
	}

	/* 2、status */
	ret = of_property_read_string(p_chr_key->nd,"status",&str);
	if(ret < 0){
		printk("key: Failed to get status property\n");
		return -EINVAL;
	} 

	if (strcmp(str, "okay"))
        return -EINVAL;

	/* 3、 compatible */
	ret = of_property_read_string(p_chr_key->nd,"compatible",&str);
	if(ret < 0){
		printk("key: Failed to get compatible property\n");
		return -EINVAL;
	} 

	if (strcmp(str, "robert,key")){
		 printk("keydev: Compatible match failed\n");
		 return -EINVAL;
	}
        
	/* 4、 get pin */
	key_dev.key_pin = of_get_named_gpio(p_chr_key->nd, "key-pin", 0);
	if(key_dev.key_pin < 0) {
		printk("can't get key-pin");
		return -EINVAL;
	}

	/* 5 、获取GPIO对应的中断号 */
    key_dev.irq_num = irq_of_parse_and_map(p_chr_key->nd, 0);
    if(!key_dev.irq_num){
        return -EINVAL;
    }
	printk("key-pin num = %d\r\n", key_dev.key_pin);

	return 0;
}

static int __init chr_dev_init(void)
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

	key_class = class_create(THIS_MODULE, MODULE_NAME);
	if (IS_ERR(key_class)) {
		goto del_unregister;
	}

	/* 绑定设备信息，仅在绑定成功后初始化设备 */
	ret = parse_devtree();
	if(ret < 0){
		pr_err("%s Couldn't parse dev-tree, ret=%d\r\n", MODULE_NAME, ret);
		goto destroy_class;
	}

	/* 绑定并初始化特定设备属性 */
	ret = key_dev_init();
	if(ret < 0){
		pr_err("%s Couldn't init property, ret=%d\r\n", MODULE_NAME, ret);
		goto destroy_class;
	}

	//p_chr_key.dev_prop = &key_dev; 	

	/* 初始化第一个设备信息 */
	p_chr_key->devid_minor = 0;
	p_chr_key->devid = MKDEV(devid_major, p_chr_key->devid_minor);
	snprintf(p_chr_key->dev_name, sizeof(p_chr_key->dev_name), "key");

	p_chr_key->cdev.owner = THIS_MODULE;
	cdev_init(&p_chr_key->cdev, &key_fops);
	
	ret = cdev_add(&p_chr_key->cdev, p_chr_key->devid, DEV_MAX_CNT);
	if(ret < 0)
		goto free_prop;
		
	p_chr_key->device = device_create(key_class, NULL, p_chr_key->devid, NULL, p_chr_key->dev_name);
	if (IS_ERR(p_chr_key->device)) {
		goto del_cdev;
	}
	
	return 0;

del_cdev:
	cdev_del(&p_chr_key->cdev);
free_prop:
	free_irq(key_dev.irq_num, NULL);
	gpio_free(key_dev.key_pin);
	del_timer_sync(&p_timer_key->timer);		/* 删除timer */
destroy_class:
	class_destroy(key_class);
del_unregister:
	unregister_chrdev_region(p_chr_key->devid, DEV_MAX_CNT);

	return -EIO;
}

static void __exit chr_dev_exit(void)
{
	/* 注销字符设备驱动 */
	cdev_del(&p_chr_key->cdev);/*  删除cdev */
	unregister_chrdev_region(p_chr_key->devid, DEV_MAX_CNT); /* 注销设备号 */

	device_destroy(key_class, p_chr_key->devid);
	class_destroy(key_class);

	free_irq(key_dev.irq_num, NULL);	/* 释放中断 */
	gpio_free(key_dev.key_pin); /* 释放GPIO */

	del_timer_sync(&p_timer_key->timer);		/* 删除timer */
}

module_init(chr_dev_init);
module_exit(chr_dev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Robert");
MODULE_INFO(intree, "Y");