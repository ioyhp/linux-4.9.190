#KERNEL_SRC=/lib/modules/kernel_version/build
#ARCH=x86

ccflags-y += -w
ccflags-y += -Wno-error

SDIO_MOD_NAME = bt_mt7668
SDIO_CFILES := \
	btmtk_sdio.c btmtk_main.c btif_audio_ts.c
$(SDIO_MOD_NAME)-objs := $(SDIO_CFILES:.c=.o)
obj-m := $(SDIO_MOD_NAME).o

all:
	$(MAKE) EXTRA_CFLAGS="-w -Wno-error" ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERNEL_SRC) M=$(PWD) modules

clean:
	rm -f *.o *~ core .depend .*.cmd *.ko *.mod.c
	rm -f Module.markers Module.symvers modules.order
	rm -rf .tmp_versions Modules.symvers
