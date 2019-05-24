
ifneq ($(KERNELRELEASE),)
	obj-m:= apfs.o
	apfs-objs := super.o dir.o file.o inode.o util.o
else
	KERNELDIR ?= /usr/src/linux
	PWD = $(shell pwd)
default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
endif
