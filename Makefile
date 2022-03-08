obj-m := imx547.o

SRC := $(shell pwd)

EXTRA_CFLAGS := -I$(KERNEL_SRC)/drivers/media/platform/
EXTRA_CFLAGS += -I$(KERNEL_SRC)/include/linux
EXTRA_CFLAGS += -I$(KERNEL_SRC)/arch/arm64/include/
EXTRA_CFLAGS += -I$(KERNEL_SRC)/arch/arm64/include/generated/
EXTRA_CFLAGS += -I$(KERNEL_SRC)/arch/arm64/include/generated/uapi/
EXTRA_CFLAGS += -I$(KERNEL_SRC)/arch/arm64/include/uapi/

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) modules

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) modules_install

clean:
	rm -f *.o *~ core .depend .*.cmd *.ko *.mod.c
	rm -f Module.markers Module.symvers modules.order
	rm -f */modules.order */modules.builtin
	rm -rf .tmp_versions Modules.symvers
