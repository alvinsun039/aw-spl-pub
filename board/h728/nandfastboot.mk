
#
#config file for sun55iw3 fastboot
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


MODULE=nandfastboot
CFG_SUNXI_FDT=y
CFG_SUNXI_NAND =y
CFG_SUNXI_SPINAND =y
CFG_SUNXI_DMA =y
CFG_SYS_INIT_RAM_SIZE=0x15000
CFG_BOOT0_LOAD_KERNEL=y
CFG_KERNEL_BOOTIMAGE=y
CFG_KERNEL_CHECKSUM=n #y will check kernel checksum in bimage, but slower
#CFG_SPINAND_KERNEL_OFFSET=0x8000 #first partition, 0x20
#CFG_SPINAND_LOGICAL_OFFSET=0
CFG_KERNEL_LOAD_ADDR=0x4007f800
CFG_SUNXI_FDT_ADDR=0x41f00000
CFG_BIMAGE_SIZE=0x01900000 #25MiByte
