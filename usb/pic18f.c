/*
 Filename: pic18f.c


 Description: Linux kernel driver for USB communication with Microchip PIC18F4550

 This USB class driver uses interrupt OUT for sending message to and interrupt IN for
 receiving message from  PIC18F4550. I am using interrupt endpoints because with lsusb command
 in the commandline PIC18F4550 device is attached to linux host with interrupt endpoint.
 lsusb -v

 After successful usb device register with usb_register_dev() a device named "pic18f" is created
 at /dev. User space application can use system calls (open, read, write ) on this device.
 use sudo chmod 777 /dev/pic18f for any user (other than root) to call open,read and write on the device.

*/


#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kref.h>
#include <asm/uaccess.h>
#include <linux/usb.h>
#include <linux/mutex.h>

//////////////////////////////////////////////////////////////////////////////////////////
//Defines

#define VENDOR_ID 0x04D8 //Microchip
#define PRODUCT_ID 0x000C //PIC18F4550

#define USB_PIC18F_MINOR_BASE 0

#define MAX_PKT_SIZE 64  //0x40
#define TIMEOUT 2000

//////////////////////////////////////////////////////////////////////////////////////////
/// \brief The usb_pic18f struct
///
struct usb_pic18f
{
    struct usb_device* udev;
    struct usb_interface *interface;

    struct mutex   pic18f_mutex;

    unsigned char intr_in_buffer[MAX_PKT_SIZE];
    struct usb_endpoint_descriptor*           intr_in_endpointAddr;
    struct usb_endpoint_descriptor*           intr_out_endpointAddr;

    int         open_count;

};



//////////////////////////////////////////////////////////////////////////////////////////
//Globals
static struct usb_driver pic18f_driver;

//////////////////////////////////////////////////////////////////////////////////////////
/// \brief pic18f_open
/// \param inode
/// \param file
/// \return
///
static int pic18f_open(struct inode *inode, struct file *file)
{
    int retval;
    struct usb_pic18f* dev;
    struct usb_interface* interface;

    printk("(pic18f device) pic18f_open() invoked.\n");

    interface = usb_find_interface(&pic18f_driver, USB_PIC18F_MINOR_BASE);
    if (!interface)
    {
        printk("(pic18f device) could not find the device for minor. \n");
        return 1;
    }

    dev = usb_get_intfdata(interface);
    if (!dev)
    {
        printk("(pic18f device) usb_get_intfdata() failed.\n");
        return 1;
    }

    // Save our object in the file's private structure
    file->private_data = dev;

    return 0;

}

//////////////////////////////////////////////////////////////////////////////////////////
/// \brief pic18f_release
/// \param inode
/// \param file
/// \return
///
static int pic18f_release(struct inode *inode, struct file *file)
{
    struct usb_pic18f* dev;

    dev = (struct usb_pic18f*)file->private_data;
    if ( dev == NULL)
    {
        return -ENODEV;
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
/// \brief pic18f_read
/// \param file
/// \param buffer
/// \param count
/// \param ppos
/// \return
///
static ssize_t pic18f_read(struct file *file, char *buffer, size_t count,
            loff_t *ppos)
{

    struct usb_pic18f *dev;
    int retval = 0;

    printk("(pic18f device) pic18f_read() invoked.\n");


    dev = (struct usb_pic18f *)file->private_data;
    if( dev == NULL)
    {
        return -ENODEV;
    }

    // Do a blocking interrupt read to get data from the device
    retval = usb_interrupt_msg(dev->udev,
                  usb_rcvintpipe(dev->udev, dev->intr_in_endpointAddr),
                  dev->intr_in_buffer,
                  MAX_PKT_SIZE,
                  &count,HZ*10);

    // if the read was successful, copy the data to userspace
    if (!retval)
    {
        if (copy_to_user(buffer, dev->intr_in_buffer, count))
            retval = -EFAULT;
        else
            retval = count;
    }

    return retval;
}

//////////////////////////////////////////////////////////////////////////////////////////
/// \brief pic18f_write_intr_callback
/// \param urb
/// \param regs
///
static void pic18f_write_intr_callback(struct urb *urb, struct pt_regs *regs)
{
    // sync/async unlink faults aren't errors
    if (urb->status &&
        !(urb->status == -ENOENT ||
          urb->status == -ECONNRESET ||
          urb->status == -ESHUTDOWN)) {
        printk("(pic18f device) %s - nonzero write intr status received: %d",
            __FUNCTION__, urb->status);
    }

    usb_free_coherent(urb->dev,
                      urb->transfer_buffer_length,
                      urb->transfer_buffer,
                      urb->transfer_dma);
}


//////////////////////////////////////////////////////////////////////////////////////////
/// \brief pic18f_write
/// \param file
/// \param user_buffer
/// \param count
/// \param ppos
/// \return
///
static ssize_t pic18f_write(struct file *file, const char *user_buffer,
        size_t count, loff_t *ppos)
{

    struct usb_pic18f *dev;
    struct urb *urb = NULL;
    char *buf = NULL;
    int retval = 0;

    printk("(pic18f device) pic18f_write() invoked.\n");

    dev = (struct usb_pic18f *)file->private_data;
    if( dev == NULL)
    {
        return -ENODEV;
    }


    // verify that we actually have some data to write
    if (count == 0)
        goto exit;

    // create a urb, and a buffer for it, and copy the data to the urb
    urb = usb_alloc_urb(0, GFP_KERNEL);
    if (!urb)
    {
        retval = -ENOMEM;
        goto error;
    }

    buf = usb_alloc_coherent(dev->udev, count, GFP_KERNEL,&urb->transfer_dma);

    if (!buf)
    {
        retval = -ENOMEM;
        goto error;
    }

    //Copy user space command to kernel space
    if (copy_from_user(buf, user_buffer, count))
    {
        retval = -EFAULT;
        goto error;
    }

    //This lock makes sure we dont submit URBs to gone devices
    mutex_lock(&dev->pic18f_mutex);

    // Initialize the urb properly
    usb_fill_int_urb(urb,
                     dev->udev,
                     usb_sndintpipe(dev->udev, dev->intr_out_endpointAddr),
                     buf,
                     count,
                     pic18f_write_intr_callback,
                     dev,1);

    urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

    // Send the data out the intr port
    retval = usb_submit_urb(urb, GFP_KERNEL);
    if (retval)
    {
        printk("(pic18f device) %s - failed submitting write urb, error %d", __FUNCTION__, retval);
        mutex_unlock(&dev->pic18f_mutex);
        goto error;
    }

    mutex_unlock(&dev->pic18f_mutex);

    //Release our reference to this urb, the USB core will eventually free it entirely
    usb_free_urb(urb);

exit:
    return count;

error:
    usb_free_coherent(dev->udev, count, buf, urb->transfer_dma);
    usb_free_urb(urb);
    kfree(buf);
    return retval;
  
}

//////////////////////////////////////////////////////////////////////////////////////////
//File operations
static const struct file_operations pic18f_fops =
{
    //.owner =	THIS_MODULE,
    .read =	pic18f_read,
    .write =	pic18f_write,
    .open =	pic18f_open,
    .release =	pic18f_release,
    
};

//////////////////////////////////////////////////////////////////////////////////////////
//usb class driver info
static struct usb_class_driver pic18f_class = {
    .name =		"usb/pic18f",
    .fops =		&pic18f_fops,
};


//////////////////////////////////////////////////////////////////////////////////////////
/// \brief pic18f_probe
/// \param interface
/// \param id
/// \return
///
static int pic18f_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
    struct usb_device *udev = interface_to_usbdev(interface);
    struct usb_pic18f *dev = NULL;

    struct usb_host_interface *iface_desc;
    struct usb_endpoint_descriptor *endpoint;
    int i, int_end_size;
    int retval = -ENODEV;

    printk("(pic18f device) probe (%04X:%04X) plugged.\n", id->idVendor, id->idProduct);

    if (!udev)
    {
        printk("(pic18f_probe) udev is NULL.\n");
        return 0;
    }

    dev = kzalloc(sizeof(struct usb_pic18f), GFP_KERNEL);

    if (! dev)
    {
        printk("(pic18f device) cannot allocate memory for struct usb_pic18f.\n");
        return 0;
    }

    mutex_init( &dev->pic18f_mutex);
    dev->open_count = 0;

    dev->udev = udev;
    dev->interface = interface;
    iface_desc = interface->cur_altsetting;


    // Set up interrupt endpoint information.
    for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i)
    {

        endpoint = (struct usb_endpoint_descriptor*)&iface_desc->endpoint[i].desc;


        if (((endpoint->bEndpointAddress &  USB_ENDPOINT_DIR_MASK) ==
                   USB_DIR_IN)
                   && ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) ==
                       USB_ENDPOINT_XFER_INT))

        {

            printk("(pic18f device)found interrupt in endpoint.\n");
            dev->intr_in_endpointAddr = endpoint->bEndpointAddress;

        }

        if (((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) ==
                   USB_DIR_OUT)
                   && ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) ==
                       USB_ENDPOINT_XFER_INT))
        {

            printk("(pic18f device)found interrupt out endpoint.\n");
            dev->intr_out_endpointAddr = endpoint->bEndpointAddress;

        }
    }

    if (!dev->intr_in_endpointAddr || !dev->intr_out_endpointAddr)
    {
        printk("(pic18f device)could not find either interrupt in or out endpoint.\n");
        return 0;
    }

    //Save our data pointer in this interface device
    usb_set_intfdata(interface, dev);

    //We can register the device now, as it is ready.
    if ((retval = usb_register_dev(interface, &pic18f_class)) < 0)
    {
        /* Something prevented us from registering this driver */
        printk("(pic18f device) Not able to get a minor for this device.");
    }
    else
    {
        printk("(pic18f device) device registered successfully. minor = %d\n", interface->minor);
    }

    return retval;
}

//////////////////////////////////////////////////////////////////////////////////////////
/// \brief pic18f_disconnect
/// \param interface
///
static void pic18f_disconnect(struct usb_interface *interface)
{
    struct usb_pic18f *dev;

    dev = usb_get_intfdata(interface);
    usb_set_intfdata(interface, NULL);

    usb_deregister_dev(interface, &pic18f_class);

    printk("(pic18f device) removed.\n");

}


//////////////////////////////////////////////////////////////////////////////////////////
//device table
static struct usb_device_id pic18f_table[] =
{
    {USB_DEVICE( VENDOR_ID, PRODUCT_ID) },
    {} /* Terminating entry */
};
MODULE_DEVICE_TABLE (usb, pic18f_table);


static struct usb_driver pic18f_driver =
{
    .name  = "pic18f_driver",
    .id_table = pic18f_table,
    .probe = pic18f_probe,
    .disconnect = pic18f_disconnect,
};

//////////////////////////////////////////////////////////////////////////////////////////
static int __init pic18f_init(void)
{
    printk( "(pic18f device) Module init.\n");

    return usb_register(&pic18f_driver);
}

//////////////////////////////////////////////////////////////////////////////////////////
static void __exit pic18f_exit(void)
{
    printk("(pic18f device) Module exit.\n");

    usb_deregister(&pic18f_driver);
}

module_init(pic18f_init);
module_exit(pic18f_exit);

MODULE_LICENSE("GPL");


