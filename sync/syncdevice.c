/*
   ================================================================
Name        : syncdevice.c
Author      : 
Version     :
Copyright   : Your copyright notice
Description : 
================================================================
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/pci.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#include "rbuffer.h"


/*
   cat /proc/devices | head -28 | tail -10
   ls -l /dev | grep "250"
   sudo mknod /dev/syncdevice c 250 0
   sudo chmod 777 syncdevice
   */

#define DEVICE "syncdevice"
#define DEVICE_NAME "syncdevice"

static dev_t first; // Global variable for the first device number
static struct cdev c_dev; // Global variable for the character device structure
static struct class *cl; // Global variable for the device class

static struct RBuffer* pQueue = 0;

int stringToInt(char str[])
{
    int i=0,sum=0;

    while(str[i]!='\0'){
        if(str[i]< 48 || str[i] > 57){
            printk("(sync device) Unable to convert it into integer.\n");
            return 0;
        }
        else{
            sum = sum*10 + (str[i] - 48);
            i++;
        }

    }

    return sum;

}

static int syncdevice_open(struct inode *i, struct file *f)
{
    printk("(sync device) open()\n");

    //Reset the queue
    if(pQueue)
    {
        RBuffer_Init(pQueue);
        //RBuffer_ShowContents(pQueue);
    }

    return 0;
}
static int syncdevice_close(struct inode *i, struct file *f)
{
    printk("(sync device) close()\n");

    return 0;
}


static ssize_t syncdevice_read(struct file *f, char __user *buf, size_t	len, loff_t *off)
{
    printk("(sync device) read()\n");

    if( pQueue == 0 )
    {
        printk("(sync device) read() Queue does not exist.\n");
        return 0;
    }

    int size = RBuffer_Size();
    long token  = RBuffer_Remove(pQueue);
    printk("(sync device) read() Queue size = %d token = %d\n", size, token);
    char read_buffer[100];
    memset(read_buffer, 0, 100);

    if( token)
    {
        //unsigned long copy_to_user (void __user * to, const void * from, unsigned long n);
        sprintf(read_buffer, "%d", token);
        printk("(sync device) read() long value = %s %d\n", read_buffer, strlen(read_buffer));
        copy_to_user(buf, read_buffer , strlen(read_buffer) );
        return strlen(read_buffer);
    }

    return 0;
}


static ssize_t syncdevice_write(struct file *f, const char __user *buf, size_t len, loff_t *off)
{
    printk("(sync device) write()\n");

    if( pQueue == 0 )
    {
        printk("(sync device) write() Queue does not exist.\n");
        return 0;
    }

    int size = RBuffer_Size();
    printk("(sync device) write() Queue size = %d, buffer = %s\n", size, buf);


    long token = stringToInt(buf);
    printk("(sync device) write long value = %ld\n", token);
    if (token)
    {
        RBuffer_Insert(pQueue, token);
    }

    return len;
}




static struct file_operations syncdevice_fops =
{
    .owner = THIS_MODULE,
    .open = syncdevice_open,
    .release = syncdevice_close,
    .write = syncdevice_write,
    .read = syncdevice_read,
};


static int __init syncdevice_init(void)
{
    long deviceinfo;
    int ret;

    printk("(sync device) Module init()\n");

    pQueue = kmalloc( (sizeof(struct RBuffer) * MAX_SIZE) , GFP_KERNEL );

    //Register a range of char device numbers baseminor = 0, count = 1 name syncdevice
    if (alloc_chrdev_region(&first, 0, 1, "syncdevice") < 0)
    {
        return -1;
    }

    //Create a struct class structure
    if ((cl = class_create(THIS_MODULE, "chardrv")) == NULL)
    {
        unregister_chrdev_region(first, 1);
        return -1;
    }

    if (device_create(cl, NULL, first, NULL, "syncdevice") == NULL)
    {
        class_destroy(cl);
        unregister_chrdev_region(first, 1);
        return -1;
    }

    cdev_init(&c_dev, &syncdevice_fops);

    if (cdev_add(&c_dev, first, 1) == -1)
    {
        device_destroy(cl, first);
        class_destroy(cl);
        unregister_chrdev_region(first, 1);
        return -1;
    }

    return 0;
}

static void __exit syncdevice_exit(void)
{
    printk("(sync device)Module exit.\n");

    RBuffer_ShowContents(pQueue);

    kfree(pQueue);
    pQueue = 0;

    cdev_del(&c_dev);
    device_destroy(cl, first);
    class_destroy(cl);
    unregister_chrdev_region(first, 1);
}

module_init(syncdevice_init);
module_exit(syncdevice_exit);

MODULE_LICENSE("GPL");
