obj-m += bier6.o
bier6-objs := bier6_core.o bier6_netdev.o bier6_fib.o bier6_dev.o
CFLAGS_bier6.o := -DDEBUG
INCLUDE = /lib/modules/`uname -r`/build/include/linux

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) -I $INCLUDE modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean


