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
 * 	2、DEVICE_NAME 		宏 			设备名			NULL					1
 * 	3、MISCkey_MINOR	宏 			子设备号		NULL					1
 * ------------		
 * 
 */
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/input.h>
#include <linux/timer.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>

#define MODULE_NAME			"input_key" /* 模块名称 */
#define DEVICE_NAME			"input_key_dev" /* 模块名称 */

/* 定义按键状态 */
enum key_status {
    KEY_PRESS = 0,      /* 按键按下 */ 
    KEY_RELEASE,        /* 按键松开 */ 
    KEY_KEEP,           /* 按键状态保持 */ 
};

/* 时钟结构 */
struct timer_base{
	struct timer_list timer;		
	int timeperiod; 				/* 定时周期,单位为ms */
	spinlock_t timer_spinlock;		
};

/* 按键属性 */
struct key_base{
	struct input_dev *idev;  /* 按键对应的input_dev指针 */
	int key_pin; 
	atomic_t status;   				
	struct timer_base timer;	

	int irq_num;					/* 中断号 */
	unsigned long irq_type; 		/* 中断类型 */
};

static struct key_base key_dev;
static struct timer_base *pBase_timer_key;	/* 指针_设备父类_所属设备 */


/* 中断返回 */
static irqreturn_t key_irq_callback(int irq, void *dev_id)
{
	if(key_dev.irq_num != irq)
		return IRQ_NONE;
	
	/* 按键防抖处理，开启定时器延时15ms */
	disable_irq_nosync(irq); /* 禁止按键中断 */
	mod_timer(&pBase_timer_key->timer, jiffies + msecs_to_jiffies(15));
    return IRQ_HANDLED;
}

static void timer_key_irq_callback(struct timer_list *arg)
{
int val;
	
	/* 读取按键值并上报按键事件 */
	val = gpio_get_value(key_dev.key_pin);
	input_report_key(key_dev.idev, KEY_0, !val);
	input_sync(key_dev.idev);
	
	enable_irq(key_dev.irq_num);

}

/** 
 * @brief			: 读取设备树信息 
 * @description		:
 * 		仅读取，和修改分离。不使用platform时，设备树节点匹配需要手动调用of函数，
 * 		查找具体的节点，在Platform中，该部分内容由机制完成，避免代码重复，
 * 		设备树节点匹配只要在match列表中写出即可，这里的nd参数则是获取到的
 * 		设备节点信息，在该函数下，利用函数仅获取匹配后设备树节点中的信息即可。
 * @return 			: 0 成功;其他 失败
 */
static int parse_devtree(struct device_node *nd)
{
	/* get pin */
	key_dev.key_pin = of_get_named_gpio(nd, "key-pin", 0);
	if(!gpio_is_valid(key_dev.key_pin)) {
        printk(KERN_ERR "keydev: Failed to get key-pin\n");
        return -EINVAL;
    }

	printk("key-pin num = %d\r\n", key_dev.key_pin);

	/* 获取GPIO对应的中断号 */
	key_dev.irq_num = irq_of_parse_and_map(nd, 0);
	if(!key_dev.irq_num){
        return -EINVAL;
    }

	/* 获取设备树中指定的中断触发类型 */
	key_dev.irq_type = irq_get_trigger_type(key_dev.irq_num);
	if (IRQF_TRIGGER_NONE == key_dev.irq_type)
		key_dev.irq_type = IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING;
	return 0;
}

/** 
 * @brief			: 根据设备树信息进行初始化 
 * @description		:
 * 		仅修改，和读取分离。根据前面获取的设备树信息进行初始化。
 * 		需要注意的是，驱动中关于设备树的初始化不包括pinctrl中的电气属性。
 * 		pinctrl的电气属性喝硬件相关一般是由厂家提供相关程序进行初始化，
 * 		如果需要在驱动中进行电气属性修改，需要通过寄存器进行操作。
 * @return 			: 0 成功;其他 失败
 */
static int devtree_init(void)
{
	int ret;

	/* 向gpio子系统申请使用GPIO */
	ret = gpio_request(key_dev.key_pin, "key0");
    if (ret) {
        printk(KERN_ERR "key: Failed to request key-pin\n");
        return ret;
	}

	gpio_direction_output(key_dev.key_pin,1);	
		
	/* 申请中断 */
	ret = request_irq(key_dev.irq_num, key_irq_callback, key_dev.irq_type, "Key0_IRQ", NULL);
	if (ret) {
        gpio_free(key_dev.key_pin);
        return ret;
    }
	return 0;
}

/** 
 * @brief			: 初始化设备属性 
 * @description		:
 * 		初始化设备所有需要的属性，包括电气属性（调用另外的函数）、定时器、
 * 		中断、自旋锁或者信号量等。
 * @return 			: 0 成功;其他 失败
 */
static int key_dev_init(struct platform_device *pdev)
{
	int ret;

	/* 读取设备树信息 */
	ret = parse_devtree(pdev->dev.of_node);
	if(ret < 0){
		pr_err("%s Couldn't parse dev-tree, ret=%d\r\n", MODULE_NAME, ret);
	}

	/* 设置pinctrl信息 */
	ret = devtree_init();
	if(ret){
		return ret;
	}



	return 0;
}

/*
 * @description		: platform驱动的probe函数，当驱动与设备匹配以后此函数就会执行
 * @param - dev 	: platform设备
 * @return 			: 0，成功;其他负值,失败
 */
static int key_probe(struct platform_device *pdev)
{	
	int ret = 0;
	
	//printk("key driver and device was matched!\r\n");

	ret = key_dev_init(pdev);
	if(ret < 0){
		pr_err("%s Couldn't init property, ret=%d\r\n", MODULE_NAME, ret);
	}

	pBase_timer_key = &key_dev.timer;

	/* 初始化定时器 */
	timer_setup(&pBase_timer_key->timer, timer_key_irq_callback, 0);
	
	/* 申请input_dev */
	key_dev.idev = input_allocate_device();
	key_dev.idev->name = DEVICE_NAME;

	#if 0
		/* 初始化input_dev，设置产生哪些事件 */
		__set_bit(EV_KEY, key.idev->evbit);	/* 设置产生按键事件 */
		__set_bit(EV_REP, key.idev->evbit);	/* 重复事件，比如按下去不放开，就会一直输出信息 */

		/* 初始化input_dev，设置产生哪些按键 */
		__set_bit(KEY_0, key.idev->keybit);	
	#endif

	#if 0
		key.idev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REP);
		key.idev->keybit[BIT_WORD(KEY_0)] |= BIT_MASK(KEY_0);
	#endif

	key_dev.idev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REP);
	input_set_capability(key_dev.idev, EV_KEY, KEY_0);

	/* 注册输入设备 */
	ret = input_register_device(key_dev.idev);
	if (ret) {
		printk("register input device failed!\r\n");
		goto free_prop;
	}
	
	return 0;

/* 在这里清除所有设备的属性，比如定时器或者释放引脚等 */
free_prop:
	free_irq(key_dev.irq_num,NULL);
	gpio_free(key_dev.key_pin);
	del_timer_sync(&pBase_timer_key->timer);
	return -EIO;
}

/*
 * @description		: platform驱动的remove函数，移除platform驱动的时候此函数会执行
 * @param - dev 	: platform设备
 * @return 			: 0，成功;其他负值,失败
 */
static int key_remove(struct platform_device *dev)
{	
	free_irq(key_dev.irq_num,NULL);
	gpio_free(key_dev.key_pin);
	del_timer_sync(&pBase_timer_key->timer);
	input_unregister_device(key_dev.idev);	/* 释放input_dev */
	return 0;
}

/* 匹配列表 */
static const struct of_device_id key_of_match[] = {
	{ .compatible = "robert,key" },
	{ /* Sentinel */ }
};

//MODULE_DEVICE_TABLE(of, key_of_match);

/* platform驱动结构体 */
static struct platform_driver key_driver = {
	.driver		= {
		.name	= MODULE_NAME,			/* 驱动名字，用于和设备匹配 */
		.of_match_table	= key_of_match, 	/* 设备树匹配表 		 */
	},
	.probe		= key_probe,
	.remove		= key_remove,
};

module_platform_driver(key_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Robert");
MODULE_INFO(intree, "Y");