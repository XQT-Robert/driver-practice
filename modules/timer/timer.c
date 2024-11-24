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


#define MODULE_NAME		"timer" 	/* 模块名称 */
#define DEV_MAX_CNT 	 1			/* 驱动最高复用设备数，即挂载/dev/的节点数 */

#define ledOFF 		 	 0				/* 关灯 */
#define ledON 			 1				/* 开灯 */

#define TIMER_IOCTL_MARK  		'T'							/* 标记只允许单字节char */
#define TIMER_CLOSE_CMD 		(_IO(TIMER_IOCTL_MARK, 0x1))	/* 关闭定时器 */
#define TIMER_OPEN_CMD			(_IO(TIMER_IOCTL_MARK, 0x2))	/* 打开定时器 */
#define TIMER_SETPERIOD_CMD		(_IO(TIMER_IOCTL_MARK, 0x3))	/* 设置定时器周期命令 */


struct class *led_class; 			/* 表示当前驱动代表的类型，可复用只创建一个 */
dev_t module_devid;					/* 驱动模块的设备号，即第一个设备的设备号 */
int devid_major;					/* 一个驱动仅需要一个主设备号 */
int dev_num = 0;					/* 复用当前驱动的设备数，互斥访问 */

struct timer_base{
	struct timer_list timer;/* 定义一个定时器*/
	int timeperiod; 		/* 定时周期,单位为ms */
	spinlock_t timer_lock;		/* 定义自旋锁 */
};

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
	int led_pin; 
	struct mutex led_lock;
	struct timer_base led_timer;
};

struct chr_dev led_dev;
struct timer_base *timer_p = &led_dev.led_timer;


/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int led_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &led_dev; /* 设置私有数据 */
	timer_p->timeperiod = 1000;		/* 默认周期为1s */
		/* 获取互斥体,可以被信号打断 */
	if (mutex_lock_interruptible(&led_dev.led_lock)) {
		return -ERESTARTSYS;
	}
	#if 0
		mutex_lock(&led_dev.lock);	/* 不能被信号打断 */
	#endif

	/* 初始化自旋锁 */
	spin_lock_init(&timer_p->timer_lock);

	return 0;
}

static long led_unlocked_ioctl (struct file *flip, unsigned int cmd, unsigned long arg)
{
	//struct chr_dev *dev = (struct chr_dev *)flip->private_data;
	int timerperiod;
	unsigned long flags;

	switch(cmd) {
		case TIMER_OPEN_CMD:
			spin_lock_irqsave(&timer_p->timer_lock, flags);
			timerperiod = timer_p->timeperiod;
			spin_unlock_irqrestore(&timer_p->timer_lock, flags);
			mod_timer(&timer_p->timer, jiffies + msecs_to_jiffies(timerperiod));
			break;
		case TIMER_CLOSE_CMD:
			del_timer_sync(&timer_p->timer);
			break;
		case TIMER_SETPERIOD_CMD:
			spin_lock_irqsave(&timer_p->timer_lock, flags);
			timer_p->timeperiod = arg;
			spin_unlock_irqrestore(&timer_p->timer_lock, flags);
			mod_timer(&timer_p->timer, jiffies + msecs_to_jiffies(arg));
			break;
		default:
			break;
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
	struct chr_dev *dev = filp->private_data;

	/* 释放互斥锁 */
	mutex_unlock(&dev->led_lock);
	
	return 0;
}

static struct file_operations led_fops = {
	.owner = THIS_MODULE,
	.open = led_open,
	.release = led_release,
	.unlocked_ioctl = led_unlocked_ioctl,

};

/* 定时器回调函数 */
void timer_function(struct timer_list *arg)
{
	/* 	from_timer是个宏，可以根据结构体的成员地址，获取到这个结构体的首地址。
		使用该宏的原因在于，复用情况下不确定该回调函数对应的定时器是哪个。
		第一个参数就是新建的结构体本身，第二个参数就是回调函数的参数arg
		（类型表示一个定时器），第三个参数就是timer_base结构体，也就是指针dev
		的结构体类型中的timer_list的定时器变量，上面结构体中我命名为timer.

		已被弃用，原因在于我将计时器封装进入了结构体当中，该宏不支持嵌套，
		是计时器专用宏
	*/
	//struct timer_base *dev = from_timer(dev, arg, timer);

	/* 使用container_of宏代替，第一个参数ptr是一个指针，指向结构体内的某个成员
	   第二个参数是结构体实例本身的结构体类型，第三个参数就是被指结构体成员的名称，
		效果等同于上者，找到结构体首地址，但支持嵌套结构	
	*/
 	struct chr_dev *dev = container_of(arg, struct chr_dev, led_timer.timer);

	static int sta = 1;
	int timerperiod;
	unsigned long flags;

	sta = !sta;		/* 每次都取反，实现LED灯反转 */
	gpio_set_value(dev->led_pin, sta);
	
	/* 重启定时器 */
	spin_lock_irqsave(&timer_p->timer_lock, flags);
	timerperiod = timer_p->timeperiod;
	spin_unlock_irqrestore(&timer_p->timer_lock, flags);
	mod_timer(&timer_p->timer, jiffies + msecs_to_jiffies(timer_p->timeperiod)); 
 }


static int bind_devtree(void)
{
	int ret;
	const char *str;

	/* 1、find dev node */
	led_dev.nd = of_find_node_by_path("/gpioled");
	if(led_dev.nd == NULL ){
		printk("led node nost find!\r\n");
		return -EINVAL;
	}

	/* 2、status */
	ret = of_property_read_string(led_dev.nd,"status",&str);
	if(ret < 0){
		printk("led: Failed to get status property\n");
		return -EINVAL;
	} 

	if (strcmp(str, "okay"))
        return -EINVAL;

	/* 3、 compatible */
	ret = of_property_read_string(led_dev.nd,"compatible",&str);
	if(ret < 0){
		printk("led: Failed to get compatible property\n");
		return -EINVAL;
	} 

	if (strcmp(str, "robert,led")){
		 printk("leddev: Compatible match failed\n");
		 return -EINVAL;
	}
        

	/* 4、 get pin */
	led_dev.led_pin = of_get_named_gpio(led_dev.nd, "led-gpio", 0);
	if(led_dev.led_pin < 0) {
		printk("can't get led-pin");
		return -EINVAL;
	}
	printk("led-pin num = %d\r\n", led_dev.led_pin);

	/* 5.向gpio子系统申请使用GPIO */
	ret = gpio_request(led_dev.led_pin, "led-PIN-NAME");
    if (ret) {
        printk(KERN_ERR "led: Failed to request led-pin\n");
        return ret;
	}

	/* 6、设置PI0为输出，并且输出高电平，默认关闭LED灯 */
	ret = gpio_direction_output(led_dev.led_pin, 1);
	if(ret < 0) {
		printk("can't set gpio!\r\n");
		return ret;
	}

	return 0;
}

static int __init chr_dev_init(void)
{
	int ret = 0;

	spin_lock_init(&timer_p->timer_lock);
	mutex_init(&led_dev.led_lock);

	
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

	led_class = class_create(THIS_MODULE, MODULE_NAME);
	if (IS_ERR(led_class)) {
		goto del_cdev;
	}

	/* 绑定设备信息，仅在绑定成功后初始化设备 */
	ret = bind_devtree();
	if(ret < 0){
		pr_err("%s Couldn't bind dev-tree, ret=%d\r\n", MODULE_NAME, ret);
			
		/* 设备树信息绑定失败 */
		gpio_free(led_dev.led_pin);
		goto del_unregister;
	}

	/* 初始化第一个设备信息 */
	led_dev.devid_minor = 0;
	led_dev.devid = MKDEV(devid_major, led_dev.devid_minor);
	snprintf(led_dev.dev_name, sizeof(led_dev.dev_name), "led");

	led_dev.cdev.owner = THIS_MODULE;
	cdev_init(&led_dev.cdev, &led_fops);
	
	ret = cdev_add(&led_dev.cdev, led_dev.devid, DEV_MAX_CNT);
	if(ret < 0)
		goto destroy_class;
		
	led_dev.device = device_create(led_class, NULL, led_dev.devid, NULL, led_dev.dev_name);
	if (IS_ERR(led_dev.device)) {
		goto del_cdev;
	}

	/* 6、初始化timer，设置定时器处理函数,还未设置周期，所有不会激活定时器 */
	timer_setup(&timer_p->timer, timer_function, 0);
	
	return 0;

del_cdev:
	cdev_del(&led_dev.cdev);
destroy_class:
	class_destroy(led_class);
del_unregister:
	unregister_chrdev_region(led_dev.devid, DEV_MAX_CNT);

	return -EIO;
}

static void __exit chr_dev_exit(void)
{
   	del_timer_sync(&timer_p->timer);		/* 关闭定时器 */

	/* 注销字符设备驱动 */
	cdev_del(&led_dev.cdev);/*  删除cdev */
	unregister_chrdev_region(led_dev.devid, DEV_MAX_CNT); /* 注销设备号 */

	device_destroy(led_class, led_dev.devid);
	class_destroy(led_class);

	gpio_free(led_dev.led_pin); /* 释放GPIO */
}

module_init(chr_dev_init);
module_exit(chr_dev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Robert");
MODULE_INFO(intree, "Y");