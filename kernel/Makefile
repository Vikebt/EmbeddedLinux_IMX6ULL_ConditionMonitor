ifneq ($(KERNELRELEASE),)
obj-m += icm20608_iio.o
else
KERNEL_SRC ?= /path/to/alientek_linux
ARCH ?= arm
CROSS_COMPILE ?= arm-linux-gnueabihf-

.PHONY: all clean
all:
	$(MAKE) -C $(KERNEL_SRC) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(CURDIR) clean
endif

