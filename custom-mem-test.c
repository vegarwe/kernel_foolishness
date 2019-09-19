#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>


/* IOCTL number for use between the kernel and the user space application.
   _IOR  --- For reading from device to user space app,
   _IOW  --- Write data passed from user space app to device(Hardware) and
   _IOWR --- For both read/write data from/to device.
   */

#define DEV_MEM_ALLOC (0)
#define DEV_MEM_FREE (1)

#define DEVICE_MEM "/dev/custom_mem_drv"

static int fd;
#define PROT_READ	0x1     /* Page can be read.  */
#define PROT_WRITE	0x2     /* Page can be written.  */
#define PROT_EXEC	0x4     /* Page can be executed.  */
#define PROT_NONE	0x0     /* Page can not be accessed.  */

#define MAP_SHARED	0x01    /* Share changes.  */
#define MAP_PRIVATE	0x02    /* Changes are private.  */

#define PAGE_SIZE 4096

int init_memory(void)
{
    fd = open(DEVICE_MEM, O_RDWR);
    if (!fd)
    {
        printf("Error opening file.");
        return -1;
    }

    printf("(test) device %s opened!\n", DEVICE_MEM);
    return 0;
}


void* alloc_memory(int size)
{
    printf("(test) alloc_memory!\n");
    void*  p = (void*)0x42424000;
    ioctl(fd, DEV_MEM_ALLOC, (int32_t*)&size);

    mmap(p, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    printf("(test) Memory allocated! size = %d - %p\n", PAGE_SIZE, p);

    return p;
}



void free_memory(void* p)
{
    ioctl(fd, DEV_MEM_FREE, (int32_t*)p);

    printf("(test) Memory released!\n");

}

void release_memory(void)
{
    close(fd);

    printf("(test) device closed!\n");
}

int main()
{
    int i;
    void* p = 0;

    if (init_memory() == 0)
    {
        p =  alloc_memory(PAGE_SIZE);
        if (p)
        {
            printf("(test) str = %s\n", (char*)p);
            memset(p, 0, PAGE_SIZE);
            strcpy(p, "Hello to you too!");
            free_memory(p);
        }

        release_memory();
    }

    return 0;
}

