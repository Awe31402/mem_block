PROJECT = mem_block
CFLAGS_$(PROJECT).o := -DDEBUG
obj-m := $(PROJECT).o
PWD := $(shell pwd)
KDIR = /lib/modules/$(shell uname -r)/build
MOD_EXIST=`lsmod | grep -i $(PROJECT) | wc -l`

all:
	@echo "Compiling module $(PROJECT)"
	@make -C $(KDIR) M=$(PWD) modules


install_mod:
	@echo "Installing module......"
	sudo cp $(PROJECT).ko /lib/modules/$(shell uname -r)
	sudo rmmod usbhid
	@echo "Please insert your usb key board within 30 secs..."
	@sleep 30
	@dmesg | grep -i hello | tee result.txt
	sudo modprobe usbhid
uninstall_mod:
	@echo "Uninstalling module....."
	sudo rmmod $(PROJECT).ko
clean:
	rm -rf *.ko  *.mod.c   *.o *.order *.symvers .*.cmd .*.swp .tmp_versions 2> /dev/null
