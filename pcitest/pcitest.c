#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/cdev.h>
#include <linux/pci.h>
#include <asm/uaccess.h>
#include <asm/io.h>

// make && sudo rmmod pcitest || true && sudo insmod pcitest.ko && sudo dmesg -c


#define DEVICE_NAME "pcitest"
#define VENDOR_ID 0x8086 //Intel
#define DEVICE_ID 0x100F //Ethernet Controller


// Assuming that our device’s configuration header requests 128 bytes on BAR 0
#define CONFIGURATION_HEADER_REQUEST 128
#define BAR_IO 0


/**
 * This table holds the list of (VendorID,DeviceID) supported by this driver
 *
 */
static struct pci_device_id pcidevice_ids[] = {
    { PCI_DEVICE(VENDOR_ID, DEVICE_ID), },
    { 0, }
};


MODULE_DEVICE_TABLE(pci, pcidevice_ids);


struct pcidevice_privdata
{
    u16 VendorID, DeviceID;
    u8 InterruptLine;
    void __iomem* regs;
};

static struct pcidevice_privdata * privdata;

static int pcidevice_probe(struct pci_dev *pdev, const struct pci_device_id* ent)
{
    printk("(pci test) probe()\n");

    u16 VendorID;
    u16 DeviceID;
    u8 InterruptLine;
    int i;
    int rc = 0;

    privdata = kzalloc(sizeof(*privdata), GFP_KERNEL);
    if (!privdata)
    {
        printk("(pci_test) Failed to allocated memory\n");
        return -ENOMEM;
    }


    pci_set_drvdata(pdev, privdata);


    // Access PCI configuration space - Quick Test
    //int pci_read_config_word(struct pci_dev *dev, int where, u16 *val);

    /*
       (pci_device) VendorID = 0x8086
       (pci_device) Device ID  = 0x100f
       (pci_device) Interrupt Line  = 5
       */

    pci_read_config_word(pdev,PCI_VENDOR_ID, &VendorID);
    printk("(pci_test) VendorID = 0x%x\n", VendorID);

    pci_read_config_word(pdev,PCI_DEVICE_ID, &DeviceID);
    printk("(pci_test) Device ID  = 0x%x\n", DeviceID);

    pci_read_config_byte(pdev,PCI_INTERRUPT_LINE, &InterruptLine);
    printk("(pci_test) Interrupt Line   = %d\n", InterruptLine);

    privdata->VendorID = VendorID;
    privdata->DeviceID = DeviceID;
    privdata->InterruptLine = InterruptLine;

    // Enable the device
    rc = pci_enable_device(pdev);
    if (rc)
    {
        printk("(pci_test) pci_enable_device() failed.\n");
        return -ENODEV;
    }

    // Cheking that BAR0 is defined and memory mapped
    if ((pci_resource_flags(pdev, BAR_IO) & IORESOURCE_MEM) != IORESOURCE_MEM)
    {
        printk("(pci_test) BAR0 is not defined in Memory space.\n");

        //cheking that BAR0 is defined and IO mapped
        if ((pci_resource_flags(pdev, BAR_IO) & IORESOURCE_IO) != IORESOURCE_IO)
        {
            printk("(pci_test) BAR0 is not defined in IO space or in Memory.\n");
            return -ENODEV;
        }
        else
        {
            printk("(pci_test) BAR0 is defined in IO space.\n");
        }
    }
    else
    {
        printk("(pci_test) BAR0 is defined in Memory space.\n");
    }

    // Total 6 BARS (regions) could be memory mapped or port-mapped.

    // Reserve pci I/O and memory resource
    rc = pci_request_region(pdev, BAR_IO, DEVICE_NAME);
    if (rc)
    {
        printk("(pci_test) BAR0 could not be requested.\n");
        return -ENODEV;
    }

    /* Using this function you will get a __iomem address to your device BAR.
     * You can access it using ioread*() and iowrite*(). */
    privdata->regs = pci_iomap(pdev, BAR_IO, CONFIGURATION_HEADER_REQUEST);
    if (!privdata->regs)
    {
        printk("(pci_test) Failed to map BAR 0.\n");
        return -ENODEV;
    }

    //iowrite32(1, &privdata->regs[0x10]);

    for (i = 0; i < 16; i++)
    {
        printk("(pci_test) Register 0x%x = 0x%04x \n", i, ioread32(&privdata->regs[i]));
    }


    //Enable bus mastering for the device
    pci_set_master(pdev);

    /*
    //Setup a single MSI interrupt
    if (pci_enable_msi(pdev))
    {
        printk("(pci_test) pci_enable_msi.\n");
        return -ENODEV;
    }

    if (pci_dma_supported(pdev, ) )
    {
        printk("(pci_test) dma not supported.\n");
        return _ENODEV;
    }

    if(!pci_set_dma_mask(pdev, DMA_BIT_MASK(64)))
    {
    }

    dma_addr_t dma_mem = pci_map_single( pdev,
    privtest->mem,
    PAGE_SIZE*(1<<memorder),
    PCI_DMA_FROMDEVICE);

    init_waitqueue_head(&privdata->waitq);
    rc = request_irq(pdev->irq, pcitest_msi, 0, DEVICE_NAME, privdata);

    //If error goto clean;
    */

    return  0;


clean:

    /*
       pci_unmap_single(pdev,
       privtest->dma_mem,
       PAGE_SIZE * (1 << memorder),
       PCI_DMA_FROMDEVICE);

       free_pages ((unsigned long) privdata->mem, memorder);
       free_irq(pdev->irq,      privdata);
       pci_disable_msi(pdev);
    */

    pci_release_region(pdev, BAR_IO);
    pci_clear_master(pdev);
    pci_disable_device(pdev);
    return -ENODEV;
}

/*
// Interrupt Handler
static irqreturn_t pcitest_msi(int irq, void *data)
{
    struct pcitest_privdata *privdata = data;
    privdata->flag = 1;
    wake_up_interruptible(&privdata->waitq);
    return IRQ_HANDLED;
}*/

static void pcidevice_remove(struct pci_dev* pdev)
{
    // Release the IO region
    printk("(pcie test) Module remove.\n");
    pci_release_region(pdev, BAR_IO);
    pci_clear_master(pdev);
    pci_disable_device(pdev);
    kfree(privdata);
}


static struct pci_driver pcidevice_driver =
{
    .name = DEVICE_NAME,
    .id_table = pcidevice_ids,
    .probe = pcidevice_probe,
    .remove = pcidevice_remove,
};


static int __init pcidevice_init(void)
{
    printk( "(pci test) Module init.\n");

    return pci_register_driver(&pcidevice_driver);
}

static void __exit pcidevice_exit(void)
{
    printk("(pci test) Module exit.\n");
    pci_unregister_driver(&pcidevice_driver);
}


module_init(pcidevice_init);
module_exit(pcidevice_exit);

MODULE_LICENSE("GPL");

