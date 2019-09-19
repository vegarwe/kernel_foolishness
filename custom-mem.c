#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/semaphore.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mm.h>


#define DEVICE_NAME                     "custom_mem_drv"
#define CLASS_NAME                      "custom_mem_drv"

#define DEV_MEM_ALLOC                   (0)
#define DEV_MEM_FREE                    (1)

#define CUSTOM_MEM_PAGE_SHIFT           12
#define CUSTOM_MEM_PAGE_SIZE            (1UL << CUSTOM_MEM_PAGE_SHIFT)
#define CUSTOM_MEM_PAGE_MASK            (~(CUSTOM_MEM_PAGE_SIZE-1))

#define CUSTOM_MEM_PAGE_ALIGN(addr)     (((addr)+CUSTOM_MEM_PAGE_SIZE-1)&CUSTOM_MEM_PAGE_MASK)
#define CUSTOM_MEM_IS_PAGE_ALIGNED(x)   (CUSTOM_MEM_PAGE_ALIGN((uintptr_t) (x)) == (uintptr_t) (x))

#define MIN_ALLOC_SIZE 4096 //page size

static int      majorNumber;                        ///< Stores the device number -- determined automatically
static int      numberOpens = 0;                    ///< Counts the number of times the device is opened
static struct   class*  customcharClass  = NULL;    ///< The device-driver class struct pointer
static struct   device* customcharDevice = NULL;    ///< The device-driver device struct pointer

static int      dev_open(struct inode *, struct file *);
static int      dev_release(struct inode *, struct file *);
static long     dev_ioctl(struct file *fp, uint32_t cmd, unsigned long arg);
static int      dev_mmap(struct file *fp, struct vm_area_struct *vma);


static DEFINE_MUTEX(dev_mem_lock);

struct test_struct
{
    unsigned long* addr;
};


static unsigned long* memAlloc(struct file* fp, size_t size, int node, int type)
{
    unsigned long *block_ctrl = NULL;
    int alloc_size = size * sizeof(unsigned long);

    if (!size || !fp)
    {
        printk("(custom_mem) Either size is 0 or FP is 0\n");
        return (unsigned long*)-1;
    }

    if (alloc_size < MIN_ALLOC_SIZE)
    {
        alloc_size = MIN_ALLOC_SIZE;
    }

    block_ctrl = kmalloc_node(alloc_size, GFP_KERNEL, node);

    printk("(custom_mem) kmalloc_node() returned %d %d %p\n", alloc_size, node, block_ctrl);

    if ((!block_ctrl) || !CUSTOM_MEM_IS_PAGE_ALIGNED(block_ctrl))
    {
        printk("(custom_mem) memAlloc() Unable to allocate memory slab"
                " or wrong alignment: %p\n", block_ctrl);
        kfree(block_ctrl);
        return NULL;
    }

    memset(block_ctrl, 0, alloc_size);
    printk("(custom_mem) userMemAlloc() Block ctrl allocated %p \n", block_ctrl);

    return block_ctrl;

}

static int dev_mem_free(struct file *fp, uint32_t cmd, unsigned long arg)
{
    unsigned long *mem_info_arg;
    if (fp == NULL)
    {
        printk("(custom_mem) dev_mem_free() Invalid file descriptor\n");
        return -EIO;
    }

    mem_info_arg = (long*)arg;

    if (fp->private_data)
    {
        printk("(custom_mem) dev_mem_free() %p %s\n", fp->private_data, (char*)fp->private_data);
        kfree(fp->private_data);
    }

    return 0;
}

static int dev_mem_alloc(struct file* fp, uint32_t cmd, unsigned long arg)
{
    unsigned long ret = 0;
    unsigned long requested_size = 0;
    unsigned long* mem_info = 0;
    fp->private_data = 0;

    if (fp == NULL)
    {
        printk("(custom_mem) dev_mem_alloc() Invalid file descriptor\n");
        return -EIO;
    }

    ret = copy_from_user(&requested_size, (unsigned long*)arg, sizeof( unsigned long));

    printk("(custom_mem) dev_mem_alloc() Allocation request size = %ld\n", requested_size);
    mem_info = memAlloc(fp, requested_size, (int)0, 0);

    if (mem_info)
    {
        fp->private_data = mem_info;
        strcpy((char*)mem_info, "Hello from kernel space");
        return ret;
    }

    return -1;
}


static int dev_mmap(struct file *fp, struct vm_area_struct *vma)
{
    int ret = 0;
    unsigned long size;
    unsigned long phys_kmalloc_area = 0;
    void *mem_info;

    /*
       What remap_pfn_range does is create another page table entry, with a different virtual address
       to the same physical memory page that doesn't have that bit set.
       Usually, it's a bad idea btw :-)
       */

    mem_info = fp->private_data;

    size = vma->vm_end - vma->vm_start;

    if (!mem_info)
    {
        printk("(custom_mem) Mem info not available.\n");
        return -1;
    }

    if (vma != 0)
    {
        printk("(custom_mem) vma_start = %03lx vma_end = %03lx size = %ld\n",
                vma->vm_start, vma->vm_end, size);
    }


    mutex_lock(&dev_mem_lock);
    phys_kmalloc_area = virt_to_phys(mem_info);
    mutex_unlock(&dev_mem_lock);


    printk("(custom_mem) dev_mmap() phys_kmalloc_area = %03lx virt_kmalloc = %p\n",
            phys_kmalloc_area, (unsigned long*)mem_info);

    ret = remap_pfn_range(vma,
            vma->vm_start,
            phys_kmalloc_area >> CUSTOM_MEM_PAGE_SHIFT, /*Describing a Page Table Entry (12 bits right shift)*/
            size,
            vma->vm_page_prot); //protection flags that are set for each PTE in this VMA"
    if (unlikely(ret))
    {
        printk("(custom_mem) remap_pfn_range failed, ret = %d\n",ret);
    }

    printk("(custom_mem) remap_pfn_range successful! ret = %d\n", ret);
    return ret;
}


static long dev_ioctl(struct file *fp, uint32_t cmd, unsigned long arg)
{
    int ret;
    printk(KERN_INFO "(custom_mem) dev_ioctl() cmd = %d arg = %ld\n", cmd, arg);
    ret = 0;
    switch(cmd) {
        case DEV_MEM_ALLOC:
            mutex_lock(&dev_mem_lock);
            ret = dev_mem_alloc(fp, cmd, arg);
            mutex_unlock(&dev_mem_lock);
            if(ret > 0) return -ENOMEM;
            break;

        case DEV_MEM_FREE:
            mutex_lock(&dev_mem_lock);
            ret = dev_mem_free(fp, cmd, arg);
            mutex_unlock(&dev_mem_lock);
            if(ret> 0) return -EIO;
            break;
        default:
            printk("(custom_mem) default IOCTL\n");
            return ret;
    }
    return 0;
}

static int dev_open(struct inode *inodep, struct file *filep){
    numberOpens++;
    printk("(custom_mem) dev_open(). Device has been opened %d time(s).\n", numberOpens);
    return 0;
}

static int dev_release(struct inode *inodep, struct file *filep){
    printk(KERN_INFO "(custom_mem) dev_release()\n");
    numberOpens = 0;
    return 0;
}


static struct file_operations fops =
{
    owner:THIS_MODULE,
    mmap:dev_mmap,
    unlocked_ioctl:dev_ioctl,
    compat_ioctl:dev_ioctl,
    open:dev_open,
    release:dev_release,
};

static int __init custom_mem_init(void)
{
    mutex_init(&dev_mem_lock);

    majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
    if (majorNumber<0)
    {
        printk(KERN_ALERT "custom_mem: custom_mem failed to register a major number\n");
        return majorNumber;
    }
    printk("(custom_mem) registered correctly with major number %d\n", majorNumber);

    // Register the device class
    customcharClass = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(customcharClass))                 // Check for error and clean up if there is
    {
        unregister_chrdev(majorNumber, DEVICE_NAME);
        printk(KERN_ALERT "(custom_mem) Failed to register device class\n");
        return PTR_ERR(customcharClass);          // Correct way to return an error on a pointer
    }
    printk("(custom_mem) device class registered correctly\n");

    // Register the device driver
    customcharDevice = device_create(customcharClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
    if (IS_ERR(customcharDevice))                // Clean up if there is an error
    {
        class_destroy(customcharClass);           // Repeated code but the alternative is goto statements
        unregister_chrdev(majorNumber, DEVICE_NAME);
        printk(KERN_ALERT "Failed to create the device\n");
        return PTR_ERR(customcharDevice);
    }
    printk("(custom_mem) device class created correctly!\n"); // Made it! device was initialized

    return 0;
}

static void __exit custom_mem_exit(void) /* Destructor */
{
    printk("(custom_mem) exit!\n");
    device_destroy(customcharClass, MKDEV(majorNumber, 0));     // remove the device
    class_unregister(customcharClass);                          // unregister the device class
    class_destroy(customcharClass);                             // remove the device class
    unregister_chrdev(majorNumber, DEVICE_NAME);             // unregister the major number
}

module_init(custom_mem_init);
module_exit(custom_mem_exit);

MODULE_LICENSE("GPL");

