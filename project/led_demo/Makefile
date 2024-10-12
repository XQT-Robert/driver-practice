KERNELDIR := /home/robert/linux/atk-mp1/linux/my_linux/linux-5.4.31
EXTRA_CFLAGS += -DMODULE -D__KERNEL__ -DMODVERSIONS

CURRENT_PATH := $(shell pwd)

obj-m := led_demo.o

build: kernel_modules

kernel_modules:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean

