// my_mouse.c (corrected)
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/types.h>

#define DRIVER_NAME "my_usb_mouse"
#define DEVICE_NAME "my_mouse"
#define MAX_EVENTS 256

struct mouse_event {
    int8_t dx;
    int8_t dy;
    uint8_t buttons; /* bit0 = left, bit1 = right, bit2 = middle */
} __attribute__((packed));

struct my_mouse {
    struct usb_device *udev;
    struct usb_interface *intf;
    unsigned char *irq_buf;
    dma_addr_t irq_dma;
    struct urb *irq_urb;
    int irq_ep_addr;    /* full endpoint address (e.g. 0x81) */
    int irq_ep_maxp;
    unsigned int irq_interval;
    dev_t devt;
    struct cdev cdev;
    struct mutex lock; /* protection for ring buffer */
    wait_queue_head_t read_wait;
    struct mouse_event events[MAX_EVENTS];
    int ev_r; /* read idx */
    int ev_w; /* write idx */
    bool removed;
};

static struct class *my_mouse_class;

/* Helper: enqueue parsed event */
static void enqueue_event(struct my_mouse *dev, struct mouse_event *ev)
{
    mutex_lock(&dev->lock);
    dev->events[dev->ev_w] = *ev;
    dev->ev_w = (dev->ev_w + 1) % MAX_EVENTS;
    /* drop oldest if full */
    if (dev->ev_w == dev->ev_r)
        dev->ev_r = (dev->ev_r + 1) % MAX_EVENTS;
    mutex_unlock(&dev->lock);
    wake_up_interruptible(&dev->read_wait);
}

/* URB completion handler: called when interrupt data arrives */
static void irq_handler(struct urb *urb)
{
    struct my_mouse *dev = urb->context;
    int retval;
    unsigned char *buf;
    struct mouse_event ev;

    if (!dev || dev->removed)
        return;

    if (urb->status) {
        /* error or disconnect; try resubmit if appropriate */
        if (urb->status == -ENOENT || urb->status == -ENODEV)
            return;
        dev_err(&dev->intf->dev, "irq_handler: urb status %d\n", urb->status);
        /* continue and try resubmit below (if device still present) */
    } else {
        buf = dev->irq_buf;
        if (urb->actual_length >= 3) {
            ev.buttons = buf[0];
            ev.dx = (int8_t)buf[1];
            ev.dy = (int8_t)buf[2];
            enqueue_event(dev, &ev);
        }
    }

    /* Resubmit the urb for next interrupt (if device not removed) */
    if (dev->removed)
        return;

    usb_fill_int_urb(dev->irq_urb, dev->udev,
                     usb_rcvintpipe(dev->udev, dev->irq_ep_addr),
                     dev->irq_buf, dev->irq_ep_maxp,
                     irq_handler, dev, dev->irq_interval);

    retval = usb_submit_urb(dev->irq_urb, GFP_ATOMIC);
    if (retval)
        dev_err(&dev->intf->dev, "Failed to resubmit urb: %d\n", retval);
}

/* Char device read */
static ssize_t my_mouse_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    struct my_mouse *dev = file->private_data;

    if (count < sizeof(struct mouse_event))
        return -EINVAL;

    /* Wait for event if none */
    if (dev->ev_r == dev->ev_w) {
        if (file->f_flags & O_NONBLOCK)
            return -EAGAIN;
        if (wait_event_interruptible(dev->read_wait, dev->ev_r != dev->ev_w || dev->removed))
            return -ERESTARTSYS;
    }

    if (dev->ev_r == dev->ev_w) {
        if (dev->removed) return 0;
        return -EAGAIN;
    }

    if (copy_to_user(buf, &dev->events[dev->ev_r], sizeof(struct mouse_event)))
        return -EFAULT;

    dev->ev_r = (dev->ev_r + 1) % MAX_EVENTS;
    return sizeof(struct mouse_event);
}

static unsigned int my_mouse_poll(struct file *file, poll_table *wait)
{
    struct my_mouse *dev = file->private_data;
    unsigned int mask = 0;

    poll_wait(file, &dev->read_wait, wait);
    mutex_lock(&dev->lock);
    if (dev->ev_r != dev->ev_w)
        mask |= POLLIN | POLLRDNORM;
    mutex_unlock(&dev->lock);
    return mask;
}

static int my_mouse_open(struct inode *inode, struct file *file)
{
    struct my_mouse *dev = container_of(inode->i_cdev, struct my_mouse, cdev);
    file->private_data = dev;
    return 0;
}

static int my_mouse_release(struct inode *inode, struct file *file)
{
    file->private_data = NULL;
    return 0;
}

static const struct file_operations my_mouse_fops = {
    .owner = THIS_MODULE,
    .read = my_mouse_read,
    .open = my_mouse_open,
    .release = my_mouse_release,
    .poll = my_mouse_poll,
};

/* Probe: called when USB core matches this driver to the device */
static int my_mouse_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
    struct usb_host_interface *hi = intf->cur_altsetting;
    struct usb_endpoint_descriptor *ep = NULL;
    struct my_mouse *dev = NULL;
    int i, retval;
    int irq_ep = 0, maxp = 0;
    unsigned int interval = 0;

    /* find interrupt-in endpoint */
    for (i = 0; i < hi->desc.bNumEndpoints; ++i) {
        ep = &hi->endpoint[i].desc;
        if ((ep->bEndpointAddress & USB_DIR_IN) &&
            (ep->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_INT) {
            irq_ep = ep->bEndpointAddress; /* full address here (0x81 etc) */
            maxp = usb_endpoint_maxp(ep);
            interval = ep->bInterval;
            break;
        }
    }
    if (!irq_ep) {
        dev_err(&intf->dev, "No interrupt-in endpoint found\n");
        return -ENODEV;
    }

    dev_info(&intf->dev, "Found interrupt endpoint 0x%02x\n", irq_ep);

    dev = kzalloc(sizeof(*dev), GFP_KERNEL);
    if (!dev)
        return -ENOMEM;

    dev->udev = usb_get_dev(interface_to_usbdev(intf));
    dev->intf = intf;
    dev->irq_ep_addr = irq_ep;              /* store full endpoint address */
    dev->irq_ep_maxp = maxp;
    dev->irq_interval = interval ? interval : 10; /* fallback */
    mutex_init(&dev->lock);
    init_waitqueue_head(&dev->read_wait);
    dev->ev_r = dev->ev_w = 0;
    dev->removed = false;

    /* allocate URB */
    dev->irq_urb = usb_alloc_urb(0, GFP_KERNEL);
    if (!dev->irq_urb) {
        dev_err(&intf->dev, "Failed to allocate URB\n");
        retval = -ENOMEM;
        goto fail;
    }

    /* allocate coherent buffer for IRQ data */
    dev->irq_buf = usb_alloc_coherent(dev->udev, dev->irq_ep_maxp, GFP_KERNEL, &dev->irq_dma);
    if (!dev->irq_buf) {
        dev_err(&intf->dev, "Failed to allocate buffer\n");
        retval = -ENOMEM;
        goto fail;
    }
    dev->irq_urb->transfer_dma = dev->irq_dma;
    dev->irq_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

    /* fill interrupt urb */
    usb_fill_int_urb(dev->irq_urb, dev->udev,
                     usb_rcvintpipe(dev->udev, dev->irq_ep_addr),
                     dev->irq_buf, dev->irq_ep_maxp,
                     irq_handler, dev, dev->irq_interval);

    /* register char device */
    retval = alloc_chrdev_region(&dev->devt, 0, 1, DEVICE_NAME);
    if (retval < 0) {
        dev_err(&intf->dev, "alloc_chrdev_region failed\n");
        goto fail;
    }
    cdev_init(&dev->cdev, &my_mouse_fops);
    dev->cdev.owner = THIS_MODULE;
    retval = cdev_add(&dev->cdev, dev->devt, 1);
    if (retval) {
        dev_err(&intf->dev, "cdev_add failed\n");
        goto fail_chrdev;
    }

    device_create(my_mouse_class, &intf->dev, dev->devt, NULL, DEVICE_NAME);

    usb_set_intfdata(intf, dev);

    /* submit initial urb */
    retval = usb_submit_urb(dev->irq_urb, GFP_KERNEL);
    if (retval) {
        dev_err(&intf->dev, "failed to submit urb: %d\n", retval);
        goto fail_submit;
    }

    dev_info(&intf->dev, "my_mouse probe successful\n");
    return 0;

fail_submit:
    device_destroy(my_mouse_class, dev->devt);
    cdev_del(&dev->cdev);
    unregister_chrdev_region(dev->devt, 1);
fail_chrdev:
    /* no-op */
fail:
    if (dev) {
        if (dev->irq_urb)
            usb_free_urb(dev->irq_urb);
        if (dev->irq_buf)
            usb_free_coherent(dev->udev, dev->irq_ep_maxp, dev->irq_buf, dev->irq_dma);
        if (dev->udev)
            usb_put_dev(dev->udev);
        kfree(dev);
    }
    return retval;
}

/* Disconnect: called when device is unplugged or driver is removed */
static void my_mouse_disconnect(struct usb_interface *intf)
{
    struct my_mouse *dev = usb_get_intfdata(intf);
    if (!dev) return;

    dev_info(&intf->dev, "my_mouse disconnect\n");

    dev->removed = true;

    usb_kill_urb(dev->irq_urb);

    device_destroy(my_mouse_class, dev->devt);
    cdev_del(&dev->cdev);
    unregister_chrdev_region(dev->devt, 1);

    usb_free_urb(dev->irq_urb);
    if (dev->irq_buf)
        usb_free_coherent(dev->udev, dev->irq_ep_maxp, dev->irq_buf, dev->irq_dma);

    usb_set_intfdata(intf, NULL);
    usb_put_dev(dev->udev);
    kfree(dev);
}

/* match table: you can replace with specific VID:PID entries */
static const struct usb_device_id my_mouse_table[] = {
    { USB_DEVICE(0x413c, 0x301a) },   /* Dell MS116 (example) */
    { } /* terminating entry */
};
MODULE_DEVICE_TABLE(usb, my_mouse_table);

static struct usb_driver my_mouse_driver = {
    .name = DRIVER_NAME,
    .id_table = my_mouse_table,
    .probe = my_mouse_probe,
    .disconnect = my_mouse_disconnect,
};

static int __init my_mouse_init(void)
{
    int ret;
    my_mouse_class = class_create("my_mouse_class");
    if (IS_ERR(my_mouse_class)) return PTR_ERR(my_mouse_class);

    ret = usb_register(&my_mouse_driver);
    if (ret)
        class_destroy(my_mouse_class);
    pr_info(DRIVER_NAME ": init\n");
    return ret;
}

static void __exit my_mouse_exit(void)
{
    usb_deregister(&my_mouse_driver);
    class_destroy(my_mouse_class);
    pr_info(DRIVER_NAME ": exit\n");
}

module_init(my_mouse_init);
module_exit(my_mouse_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tutorial");
MODULE_DESCRIPTION("Simple USB mouse driver that exposes /dev/my_mouse");
