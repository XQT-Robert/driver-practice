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
	{ 0x75736401, "class_destroy" },
	{ 0xbb02a4d3, "driver_unregister" },
	{ 0x33c2388, "__spi_register_driver" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0x9192c2a4, "__class_create" },
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0x3fd78f3b, "register_chrdev_region" },
	{ 0x2131a349, "device_create" },
	{ 0xdd504b4a, "cdev_add" },
	{ 0x294045e0, "cdev_init" },
	{ 0x1bbe6db3, "spi_setup" },
	{ 0x746ae499, "devm_kmalloc" },
	{ 0x8e865d3c, "arm_delay_ops" },
	{ 0xc5850110, "printk" },
	{ 0x189c5980, "arm_copy_to_user" },
	{ 0xdecd0b29, "__stack_chk_fail" },
	{ 0x9d669763, "memcpy" },
	{ 0x37a0cba, "kfree" },
	{ 0xd3b7c6eb, "spi_sync" },
	{ 0x5f754e5a, "memset" },
	{ 0x2d6fcc06, "__kmalloc" },
	{ 0x66cbc2d8, "kmem_cache_alloc_trace" },
	{ 0xda5b3aa3, "kmalloc_caches" },
	{ 0x8f678b07, "__stack_chk_guard" },
	{ 0x7e5a3c80, "device_destroy" },
	{ 0xff2f3447, "cdev_del" },
	{ 0xefd6cf06, "__aeabi_unwind_cpp_pr0" },
};

MODULE_INFO(depends, "");

