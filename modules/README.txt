#驱动模块
#该文档仅记录有一定参考意义的模块信息

#部分模块：
1、
@module			: led_two
@final_update	: 2024.10.23
@description	: 在/dev/挂载节点文件中创建两个设备节点,控制同一个led灯。
@detail			:	
	该模块主要是尝试使用两个设备节点操作同一个硬件资源，基于pinctrl，操纵的是同一个
led灯，重新改写了之前字符型模块中不恰当的地方，比如class\主设备号等同一个驱动文件
复用的资源，现在已经从设备的struct中独立了，设备结构中仅保留了cdev、device、minor
等和“设备”相关的变量。

2、
@module			: irq_key
@final_update	: 2024.10.31
@description	: 利用中断+定时器感应按键。
@detail			:	
	对字符型模块的结构做了进一步的分离，现在分为驱动、字符型设备、特定设备属性等个三
	部分。在模块Init函数中，获取设备树信息和申请中断和gpio子系统分离了，结构更加清晰。

3、
@module			: misc/dts_platform
@final_update	: 2025.1.25
@description	: 杂项驱动及总线模型
@detail			:	
	用面向对象的方法重新实现，添加了注释，重新迭代了整体驱动框架
