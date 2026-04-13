#include <linux/module.h>
#include <linux/export-internal.h>
#include <linux/compiler.h>

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



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x62543cd7, "__serdev_device_driver_register" },
	{ 0x9db46ab2, "serdev_device_close" },
	{ 0x531da4e9, "_dev_info" },
	{ 0xd30c3a4c, "serdev_device_open" },
	{ 0xbe2f7a74, "serdev_device_set_baudrate" },
	{ 0xb8b3d0cc, "serdev_device_set_flow_control" },
	{ 0xb5129dd9, "_dev_err" },
	{ 0x122c3a7e, "_printk" },
	{ 0x92893115, "driver_unregister" },
	{ 0x91a47972, "serdev_device_write_wakeup" },
	{ 0x474e54d2, "module_layout" },
};

MODULE_INFO(depends, "");

MODULE_ALIAS("of:N*T*Cmakerlab,gp02-gps");
MODULE_ALIAS("of:N*T*Cmakerlab,gp02-gpsC*");

MODULE_INFO(srcversion, "3427E3F19AC4F1CBB32BEF7");
