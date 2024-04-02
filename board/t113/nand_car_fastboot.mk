
#
#config file for sun8iw20
#
FILE_EXIST=$(shell if [ -f $(TOPDIR)/board/$(PLATFORM)/common.mk ]; then echo yes; else echo no; fi;)
ifeq (x$(FILE_EXIST),xyes)
include $(TOPDIR)/board/$(PLATFORM)/common.mk
else
include $(TOPDIR)/board/$(CP_BOARD)/common.mk
endif

MODULE=nand
CFG_SUNXI_NAND =y
CFG_SUNXI_SPINAND =y
CFG_SUNXI_DMA =y

CFG_SUNXI_FDT=y

#C906
CFG_RISCV_C906=y
CFG_SUNXI_ELF=y
CFG_MELISELF_LOAD_ADDR=0x45000000
CFG_ELF_64BIT=y

CFG_SUNXI_HWSPINLOCK=y
CFG_VIDEO_BIN=y
CFG_VIDEO_DATA_LOAD_ADDR=0x44000000
