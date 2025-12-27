

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb.h>

#define DRIVER_NAME "my_usb_mouse"

// Match table for Dell mouse
static const struct usb_device_id my_mouse_table[] = {
    { USB_DEVICE(0x413c, 0x301a) },
    { }
};
MODULE_DEVICE_TABLE(usb, my_mouse_table);

// Called when the mouse is plugged in
static int my_mouse_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
    pr_info(DRIVER_NAME ": Mouse plugged in! VID=0x%04x PID=0x%04x\n",
            id->idVendor, id->idProduct);
    return 0; // success
}

// Called when the mouse is unplugged
static void my_mouse_disconnect(struct usb_interface *intf)
{
    pr_info(DRIVER_NAME ": Mouse disconnected!\n");
}

// USB driver structure
static struct usb_driver my_mouse_driver = {
    .name = DRIVER_NAME,
    .id_table = my_mouse_table,
    .probe = my_mouse_probe,
    .disconnect = my_mouse_disconnect,
};

static int __init my_mouse_init(void)
{
    return usb_register(&my_mouse_driver);
}

static void __exit my_mouse_exit(void)
{
    usb_deregister(&my_mouse_driver);
}

module_init(my_mouse_init);
module_exit(my_mouse_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tutorial");
MODULE_DESCRIPTION("Step 1: Detect USB mouse");