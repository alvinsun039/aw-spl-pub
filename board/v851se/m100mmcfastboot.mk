
#
#config file for sun8iw21 fastboot
#
#stroage
FILE_EXIST=$(shell if [ -f $(TOPDIR)/board/$(PLATFORM)/common.mk ]; then echo yes; else echo no; fi;)
EXT_FILE_EXIST=$(shell if [ -f $(TOPDIR)/board/$(PLATFORM)/common$(LICHEE_BOARD).mk ]; then echo yes; else echo no; fi;)
ifeq (x$(EXT_FILE_EXIST),xyes)
include $(TOPDIR)/board/$(PLATFORM)/common$(LICHEE_BOARD).mk
else ifeq (x$(FILE_EXIST),xyes)
include $(TOPDIR)/board/$(PLATFORM)/common.mk
else
include $(TOPDIR)/board/$(CP_BOARD)/common.mk
endif


MODULE=mmcfastboot
CFG_SUNXI_SDMMC =y


CFG_SUNXI_FDT=y

CFG_BOOT0_LOAD_KERNEL=y
CFG_KERNEL_BOOTIMAGE=y
CFG_KERNEL_CHECKSUM=n
#CFG_MMC_KERNEL_OFFSET=0x400
#CFG_MMC_LOGICAL_OFFSET=40960 # 逻辑起始地址，打开ENV和GPT功能后，该配置可以去掉
CFG_KERNEL_LOAD_ADDR=0x40007800
CFG_SUNXI_FDT_ADDR=0x41f00000
CFG_RESERVE_FDT_SIZE=0x30000
CFG_SUNXI_NO_UPDATE_FDT_CHOSEN=y
CFG_LOAD_DTB_FROM_KERNEL=y # 将设备树放进内核中，主要用于AB系统切换系统使用，后续介绍
CFG_SUNXI_SUPPORT_RAMDISK=y # 支持ramdisk镜像，主要用于recovery系统
CFG_RAMDISK_ADDR=0x43000000 # ramdisk镜像存放地址

CFG_SET_GPIO_NEW=y

CFG_SUNXI_EFUSE =y

CFG_SUNXI_GPT=y
CFG_MMC_GPT_ARD=0 # 0 or 40960 sector
CFG_SUNXI_ENV=y
CFG_SUNXI_ENV_SIZE=0x20000 # linux should be the same with LICHEE_REDUNDANT_ENV_SIZE.
CFG_SUNXI_HAVE_REDUNDENV=y

