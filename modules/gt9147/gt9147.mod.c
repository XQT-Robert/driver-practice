#include <linux/build-salt.h>
#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(.gnu.linkonce.this_module) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used __section(__versions) = {
	{ 0x4b3323eb, "module_layout" },
	{ 0x53e83018, "i2c_del_driver" },
	{ 0x86150746, "i2c_register_driver" },
	{ 0x1f144533, "devm_request_threaded_irq" },
	{ 0x24d2061c, "input_register_device" },
	{ 0xac94c94a, "devm_gpio_request_one" },
	{ 0xbcd4527e, "input_mt_init_slots" },
	{ 0x703ec12c, "input_set_abs_params" },
	{ 0xfa3da99a, "devm_input_allocate_device" },
	{ 0x8e865d3c, "arm_delay_ops" },
	{ 0x5a0629c5, "gpiod_direction_input" },
	{ 0xf9a482f9, "msleep" },
	{ 0x6d234f6a, "gpiod_set_raw_value" },
	{ 0xc6e479b3, "gpio_to_desc" },
	{ 0xceb52a7e, "of_get_named_gpio_flags" },
	{ 0x746ae499, "devm_kmalloc" },
	{ 0xaefa381c, "input_mt_report_pointer_emulation" },
	{ 0x7eb04cec, "input_mt_report_slot_state" },
	{ 0x5de00dd, "input_event" },
	{ 0xc5850110, "printk" },
	{ 0x26a6195e, "input_unregister_device" },
	{ 0xefd6cf06, "__aeabi_unwind_cpp_pr0" },
	{ 0xdecd0b29, "__stack_chk_fail" },
	{ 0x81109725, "i2c_transfer" },
	{ 0x9d669763, "memcpy" },
	{ 0x8f678b07, "__stack_chk_guard" },
};

MODULE_INFO(depends, "");

