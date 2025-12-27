#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/export-internal.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

#ifdef CONFIG_UNWINDER_ORC
#include <asm/orc_header.h>
ORC_HEADER;
#endif

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
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
__used __section("__versions") = {
	{ 0xb56aaa4b, "usb_put_dev" },
	{ 0x37a0cba, "kfree" },
	{ 0x65487097, "__x86_indirect_thunk_rax" },
	{ 0x4dfa8d4b, "mutex_lock" },
	{ 0x3213f038, "mutex_unlock" },
	{ 0x6ca9b86a, "class_create" },
	{ 0x748aeeb3, "usb_register_driver" },
	{ 0x122c3a7e, "_printk" },
	{ 0x75646747, "class_destroy" },
	{ 0xc61fc253, "usb_deregister" },
	{ 0x6b10bee1, "_copy_to_user" },
	{ 0xe2c17b5d, "__SCT__might_resched" },
	{ 0xfe487975, "init_wait_entry" },
	{ 0x1000e51, "schedule" },
	{ 0x8c26d495, "prepare_to_wait_event" },
	{ 0x92540fbf, "finish_wait" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0x87a21cb3, "__ubsan_handle_out_of_bounds" },
	{ 0x54b1fac6, "__ubsan_handle_load_invalid_value" },
	{ 0x9c1f1e35, "_dev_err" },
	{ 0xc397ffa7, "usb_submit_urb" },
	{ 0xe2964344, "__wake_up" },
	{ 0x4c03a563, "random_kmalloc_seed" },
	{ 0x1bff00c8, "kmalloc_caches" },
	{ 0xd0c3484c, "kmalloc_trace" },
	{ 0x9145465a, "usb_get_dev" },
	{ 0xcefb0c9f, "__mutex_init" },
	{ 0xd9a5ea54, "__init_waitqueue_head" },
	{ 0xc588d635, "usb_alloc_urb" },
	{ 0xd0a9c05e, "usb_alloc_coherent" },
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0x22d6de43, "cdev_init" },
	{ 0xec957a9, "cdev_add" },
	{ 0x2e3443fd, "device_create" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0x9431f332, "_dev_info" },
	{ 0x79c2529f, "usb_kill_urb" },
	{ 0x19edaabb, "device_destroy" },
	{ 0xb1b9cfc9, "cdev_del" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0xde4d5928, "usb_free_urb" },
	{ 0xc0107a7f, "usb_free_coherent" },
	{ 0xe2fd41e5, "module_layout" },
};

MODULE_INFO(depends, "");

MODULE_ALIAS("usb:v413Cp301Ad*dc*dsc*dp*ic*isc*ip*in*");

MODULE_INFO(srcversion, "3DC8628208221E435F55BB6");
