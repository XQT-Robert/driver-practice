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
	{ 0x7e5a3c80, "device_destroy" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0xff2f3447, "cdev_del" },
	{ 0x75736401, "class_destroy" },
	{ 0x2131a349, "device_create" },
	{ 0x9192c2a4, "__class_create" },
	{ 0xdd504b4a, "cdev_add" },
	{ 0x294045e0, "cdev_init" },
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0x3fd78f3b, "register_chrdev_region" },
	{ 0x9f87f6c4, "of_iomap" },
	{ 0xbdc03584, "of_property_read_variable_u32_array" },
	{ 0xf4d357f8, "of_property_read_string" },
	{ 0xc546631d, "of_find_property" },
	{ 0x95b97b18, "of_find_node_opts_by_path" },
	{ 0xedc03953, "iounmap" },
	{ 0xdecd0b29, "__stack_chk_fail" },
	{ 0x5f754e5a, "memset" },
	{ 0xc5850110, "printk" },
	{ 0x2cfde9a2, "warn_slowpath_fmt" },
	{ 0x514cc273, "arm_copy_from_user" },
	{ 0x8f678b07, "__stack_chk_guard" },
	{ 0xefd6cf06, "__aeabi_unwind_cpp_pr0" },
};

MODULE_INFO(depends, "");

