KERNEL_PATH ?= /lib/modules/$(shell uname -r)/build

obj-m += tracerhid_module.o
tracerhid_module-objs := tracerhid.o kallsyms.o

all:
	make -C $(KERNEL_PATH) M=$(PWD) modules

clean:
	make -C $(KERNEL_PATH) M=$(PWD) clean
