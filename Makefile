
obj-m += custom-mem.o

all:
	echo ${CFLAGS}
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean