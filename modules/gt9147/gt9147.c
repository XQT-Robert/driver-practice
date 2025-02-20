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

 #include <linux/module.h>
 #include <linux/i2c.h>
 #include <linux/regmap.h>
 #include <linux/gpio/consumer.h>
 #include <linux/of_irq.h>
 #include <linux/interrupt.h>
 #include <linux/input.h>
 #include <linux/input/mt.h>
 #include <linux/debugfs.h>
 #include <linux/delay.h>
 #include <linux/slab.h>
 #include <linux/gpio.h>
 #include <linux/of_gpio.h>
 #include <linux/input/mt.h>
 #include <linux/input/touchscreen.h>
 #include <linux/i2c.h>

 #include "gt9147Reg.h"

//-------------------------------------------------------
//properties

#define MODULE_NAME		"gt9147" 		/* 模块名称 */
#define DEV_MAX_CNT 	 1				/* 驱动最高复用设备数，即挂载/dev/的节点数 */

struct gt9147_base{
	struct input_dev *parent;  		/* 按键对应的input_dev指针 */
	struct i2c_client *client;		/* i2c 设备 */

	int irq_pin,reset_pin;					/* 中断和复位IO		*/
	int irqnum;								/* 中断号    		*/
	void *private_data;						/* 私有数据 		*/
};

//不适合，静态分配贯穿整个驱动模块周期，使用动态分devm_kzalloc，设备卸载时释放
//方法为在probe中申请空间，然后通过函数持续传递该地址指针
//static struct gt9147_base gt9147_dev;


//static struct chr_base *pBase_chr_gt9147 ;

const unsigned char GT9147_CT[]=
{
	0x48,0xe0,0x01,0x10,0x01,0x05,0x0d,0x00,0x01,0x08,
	0x28,0x05,0x50,0x32,0x03,0x05,0x00,0x00,0xff,0xff,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x89,0x28,0x0a,
	0x17,0x15,0x31,0x0d,0x00,0x00,0x02,0x9b,0x03,0x25,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x32,0x00,0x00,
	0x00,0x0f,0x94,0x94,0xc5,0x02,0x07,0x00,0x00,0x04,
	0x8d,0x13,0x00,0x5c,0x1e,0x00,0x3c,0x30,0x00,0x29,
	0x4c,0x00,0x1e,0x78,0x00,0x1e,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x08,0x0a,0x0c,0x0e,0x10,0x12,0x14,0x16,
	0x18,0x1a,0x00,0x00,0x00,0x00,0x1f,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0x00,0x02,0x04,0x05,0x06,0x08,0x0a,0x0c,
	0x0e,0x1d,0x1e,0x1f,0x20,0x22,0x24,0x28,0x29,0xff,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,
};

//end of properties
//-------------------------------------------------------
//operations

/*
 * @description	: 从GT9147读取多个寄存器数据
 * @param - dev:  GT9147设备
 * @param - reg:  要读取的寄存器首地址
 * @param - buf:  读取到的数据
 * @param - len:  要读取的数据长度
 * @return 		: 操作结果
 */
static int gt9147_read_regs(struct gt9147_base *dev, u16 reg, u8 *buf, int len)
{
	int ret;
    u8 regdata[2];
	struct i2c_msg msg[2];
	struct i2c_client *client = (struct i2c_client *)dev->client;

	if (!client) {
        pr_err("Client pointer is NULL in gt9147_read_regs\n");
        return -EINVAL;
    }
    
    /* GT9147寄存器长度为2个字节 */
    regdata[0] = reg >> 8;
    regdata[1] = reg & 0xFF;

	/* msg[0]为发送要读取的首地址 */
	msg[0].addr = client->addr;			/* ft5x06地址 */
	msg[0].flags = !I2C_M_RD;			/* 标记为发送数据 */
	msg[0].buf = &regdata[0];			/* 读取的首地址 */
	msg[0].len = 2;						/* reg长度*/

	/* msg[1]读取数据 */
	msg[1].addr = client->addr;			/* ft5x06地址 */
	msg[1].flags = I2C_M_RD;			/* 标记为读取数据*/
	msg[1].buf = buf;					/* 读取数据缓冲区 */
	msg[1].len = len;					/* 要读取的数据长度*/

	ret = i2c_transfer(client->adapter, msg, 2);
	if(ret == 2) {
		ret = 0;
	} else {
		ret = -EREMOTEIO;
	}
	return ret;
}

/*
 * @description	: 向GT9147多个寄存器写入数据
 * @param - dev:  GT9147设备
 * @param - reg:  要写入的寄存器首地址
 * @param - val:  要写入的数据缓冲区
 * @param - len:  要写入的数据长度
 * @return 	  :   操作结果
 */
static s32 gt9147_write_regs(struct gt9147_base *dev, u16 reg, u8 *buf, u8 len)
{
	u8 b[256];
	struct i2c_msg msg;
	struct i2c_client *client = (struct i2c_client *)dev->client;
	
	b[0] = reg >> 8;			/* 寄存器首地址低8位 */
    b[1] = reg & 0XFF;			/* 寄存器首地址高8位 */
	memcpy(&b[2],buf,len);		/* 将要写入的数据拷贝到数组b里面 */

	msg.addr = client->addr;	/* gt9147地址 */
	msg.flags = 0;				/* 标记为写数据 */

	msg.buf = b;				/* 要写入的数据缓冲区 */
	msg.len = len + 2;			/* 要写入的数据长度 */

	return i2c_transfer(client->adapter, &msg, 1);
}

static irqreturn_t gt9147_irq_handler(int irq, void *dev_id)
{
    int touch_num = 0;
    int input_x, input_y;
    int id = 0;
    int ret = 0;
    u8 data;
    u8 touch_data[5];
    struct gt9147_base *dev = dev_id;

    ret = gt9147_read_regs(dev, GT_GSTID_REG, &data, 1);
    if (data == 0x00)  {     /* 没有触摸数据，直接返回 */
        goto fail;
    } else {                 /* 统计触摸点数据 */
        touch_num = data & 0x0f;
    }

    /* 由于GT9147没有硬件检测每个触摸点按下和抬起，因此每个触摸点的抬起和按
     * 下不好处理，尝试过一些方法，但是效果都不好，因此这里暂时使用单点触摸 
     */
    if(touch_num) {         /* 单点触摸按下 */
        gt9147_read_regs(dev, GT_TP1_REG, touch_data, 5);
        id = touch_data[0] & 0x0F;
        if(id == 0) {
            input_x  = touch_data[1] | (touch_data[2] << 8);
            input_y  = touch_data[3] | (touch_data[4] << 8);

            input_mt_slot(dev->parent, id);
		    input_mt_report_slot_state(dev->parent, MT_TOOL_FINGER, true);
		    input_report_abs(dev->parent, ABS_MT_POSITION_X, input_x);
		    input_report_abs(dev->parent, ABS_MT_POSITION_Y, input_y);
        }
    } else if(touch_num == 0){                /* 单点触摸释放 */
        input_mt_slot(dev->parent, id);
        input_mt_report_slot_state(dev->parent, MT_TOOL_FINGER, false);
    }

	input_mt_report_pointer_emulation(dev->parent, true);
    input_sync(dev->parent);

    data = 0x00;                /* 向0X814E寄存器写0 */
    gt9147_write_regs(dev, GT_GSTID_REG, &data, 1);

fail:
	return IRQ_HANDLED;
}

//end of operations
//-------------------------------------------------------
//parse devtree and read the dev information  

/** 
 * @description	: 将设备信息读入驱动
 * @param -  dev: 读入信息写入的驱动设备
 * @return 		: 无
 * @‌postscript‌  : 为了避免使用全局变量，在probe中动态申请设备，然后传入此处
 */
static int read_dev_infor(struct gt9147_base *dev)
{
	struct i2c_client *client = dev->client;

	/* 1，获取设备树中的中断和复位引脚 */
	dev->reset_pin = of_get_named_gpio(client->dev.of_node, "reset-gpios", 0);
	if(!gpio_is_valid(dev->reset_pin)) {
		return -1;
	}

	dev->irq_pin = of_get_named_gpio(client->dev.of_node, "interrupt-gpios", 0);
	if(!gpio_is_valid(dev->irq_pin)) {
		return -1;
	}

	return 0;
}

//end of parse & read
//-------------------------------------------------------
//probe then set dev

/*
 * @description     : GT9147中断初始化
 * @param - client 	: 要操作的i2c
 * @param - multidev: 自定义的multitouch设备
 * @return          : 0，成功;其他负值,失败
 */
static int gt9147_ts_irq(struct i2c_client *client, struct gt9147_base *dev)
{
	int ret = 0;

	/* 2，申请中断,client->irq就是IO中断， */
	ret = devm_request_threaded_irq(&client->dev, client->irq, NULL,
					gt9147_irq_handler, IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					client->name, dev);
	if (ret) {
		//dev_err(&client->dev, "Unable to request touchscreen IRQ.\n");
		pr_err("Unable to request touchscreen IRQ, error code: %d\n", ret);
		return ret;
	}

	return 0;
}

/*
 * @description     : 复位GT9147
 * @param - client 	: 要操作的i2c
 * @param - multidev: 自定义的multitouch设备
 * @return          : 0，成功;其他负值,失败
 */
static int gt9147_ts_reset(struct i2c_client *client, struct gt9147_base *dev)
{
	int ret = 0;

    /* 申请复位IO*/
	if (gpio_is_valid(dev->reset_pin)) {  	
		/* 申请复位IO，并且默认输出高电平 */
		ret = devm_gpio_request_one(&client->dev,	
					dev->reset_pin, GPIOF_OUT_INIT_HIGH,
					"gt9147 reset");
		if (ret) {	
			return ret;
		}
	}

    /* 申请中断IO*/
	if (gpio_is_valid(dev->irq_pin)) {  				
		/* 申请复位IO，并且默认输出高电平 */
		ret = devm_gpio_request_one(&client->dev,	
					dev->irq_pin, GPIOF_OUT_INIT_HIGH,
					"gt9147 int");
		if (ret) {
			return ret;
		}
	}

    /* 4、初始化GT9147，要严格按照GT9147时序要求 */
    gpio_set_value(dev->reset_pin, 0); /* 复位GT9147 */
    msleep(10);
    gpio_set_value(dev->reset_pin, 1); /* 停止复位GT9147 */
    msleep(10);
    gpio_set_value(dev->irq_pin, 0);    /* 拉低INT引脚 */
    msleep(50);
    gpio_direction_input(dev->irq_pin); /* INT引脚设置为输入 */

	return 0;
}

/*
 * @description	: 发送GT9147配置参数
 * @param - client: i2c_client
 * @param - mode: 0,参数不保存到flash
 *                1,参数保存到flash
 * @return 		: 无
 */
void gt9147_send_cfg(struct gt9147_base *dev, unsigned char mode)
{
	unsigned char buf[2];
	unsigned int i = 0;

	buf[0] = 0;
	buf[1] = mode;	/* 是否写入到GT9147 FLASH?  即是否掉电保存 */
	for(i = 0; i < (sizeof(GT9147_CT)); i++) /* 计算校验和 */
        buf[0] += GT9147_CT[i];            
    buf[0] = (~buf[0]) + 1;

    /* 发送寄存器配置 */
    gt9147_write_regs(dev, GT_CFGS_REG, (u8 *)GT9147_CT, sizeof(GT9147_CT));
    gt9147_write_regs(dev, GT_CHECK_REG, buf, 2);/* 写入校验和,配置更新标记 */
} 

/*
 * @description		: platform驱动的probe函数，当驱动与设备匹配以后此函数就会执行
 * @param - dev 	: platform设备
 * @return 			: 0，成功;其他负值,失败
 */
static int gt9147_probe(struct i2c_client *client, const struct i2c_device_id *id)
{	
	u8 ret = 0;
	u8 data;
	struct gt9147_base *p_gt9147dev;

	/* 动态申请结构体 */
	p_gt9147dev = devm_kzalloc(&client->dev, sizeof(*p_gt9147dev), GFP_KERNEL);
	if(!p_gt9147dev)
		return -ENOMEM;

	p_gt9147dev->client = client;

	/* 保存gt9147dev结构体 */
	i2c_set_clientdata(client,p_gt9147dev);
	
	/* 读入设备信息 */
	read_dev_infor(p_gt9147dev);

	/* 2，复位GT9147 */
	ret = gt9147_ts_reset(client, p_gt9147dev);
	if(ret < 0) {
		goto fail;
    }

    /* 3，初始化GT9147 */
    data = 0x02;
    gt9147_write_regs(p_gt9147dev, GT_CTRL_REG, &data, 1); /* 软复位 */
    mdelay(100);
    data = 0x0;
    gt9147_write_regs(p_gt9147dev, GT_CTRL_REG, &data, 1); /* 停止软复位 */
    mdelay(100);

    /* 4,初始化GT9147，烧写固件 */
    gt9147_read_regs(p_gt9147dev, GT_CFGS_REG, &data, 1);
    printk("GT9147 ID =%#X\r\n", data);
    if(data <  GT9147_CT[0]) {
       gt9147_send_cfg(p_gt9147dev, 0);
    }

    /* 5，input设备注册 */
	p_gt9147dev->parent = devm_input_allocate_device(&client->dev);
	if (!(p_gt9147dev->parent)) {
		ret = -ENOMEM;
		goto fail;
	}
	p_gt9147dev->parent->name = client->name;
	p_gt9147dev->parent->id.bustype = BUS_I2C;
	p_gt9147dev->parent->dev.parent = &client->dev;

	__set_bit(EV_KEY, p_gt9147dev->parent->evbit);
	__set_bit(EV_ABS, p_gt9147dev->parent->evbit);
	__set_bit(BTN_TOUCH, p_gt9147dev->parent->keybit);

	input_set_abs_params(p_gt9147dev->parent, ABS_X, 0, 480, 0, 0);
	input_set_abs_params(p_gt9147dev->parent, ABS_Y, 0, 272, 0, 0);
	input_set_abs_params(p_gt9147dev->parent, ABS_MT_POSITION_X,0, 480, 0, 0);
	input_set_abs_params(p_gt9147dev->parent, ABS_MT_POSITION_Y,0, 272, 0, 0);	     
	ret = input_mt_init_slots(p_gt9147dev->parent, MAX_SUPPORT_POINTS, 0);
	if (ret) {
		goto fail;
	}

	ret = input_register_device(p_gt9147dev->parent);
	if (ret)
		goto fail;

    /* 6，最后初始化中断 */
	ret = gt9147_ts_irq(client, p_gt9147dev);
	if(ret < 0) {
		goto fail;
	}

    return 0;
	
fail:
	return ret;
}

/*
 * @description		: platform驱动的remove函数，移除platform驱动的时候此函数会执行
 * @param - dev 	: platform设备
 * @return 			: 0，成功;其他负值,失败
 */
static int gt9147_remove(struct i2c_client *client)
{	
	/* 获取设备结构体指针 */
	struct gt9147_base *dev = i2c_get_clientdata(client);  
	input_unregister_device(dev->parent);
	return 0;
}

//end of probe
//-------------------------------------------------------
//module init 

/* 传统匹配方式ID列表 */
static const struct i2c_device_id gt9147_id_table[] = {
	{"atk-gt9147", 0, },  
	{/* sentinel */ }
};

/* 设备树匹配列表 */
static const struct of_device_id gt9147_of_match[] = {
	{ .compatible = "atk-gt9147"},
	{ /* Sentinel */ }
};

//MODULE_DEVICE_TABLE(of, gt9147_of_match);

/* platform驱动结构体 */
static struct i2c_driver gt9147_driver = {
	.driver		= {
		.owner = THIS_MODULE,
		.name	= MODULE_NAME,			/* 驱动名字，用于和设备匹配 */
		.of_match_table	= gt9147_of_match, 	/* 设备树匹配表 		 */
	},
	.id_table = gt9147_id_table,
	.probe		= gt9147_probe,
	.remove		= gt9147_remove,
};

module_i2c_driver(gt9147_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Robert");
MODULE_INFO(intree, "Y");