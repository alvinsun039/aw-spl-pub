
#
#config file for sun55iw3
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

MODULE=sboot

CFG_SUNXI_MMC =y
CFG_SUNXI_CE_30 =y
CFG_SUNXI_SBOOT =y
CFG_SUNXI_ITEM_HASH =y
CFG_SUNXI_KEY_PROVISION =y
CFG_SUNXI_KEY_PROVISION =y
CFG_SUNXI_RSA_SOFTWARE =y
CFG_SUNXI_HASH_SOFTWARE =y
CFG_SUNXI_BOOTUP_TIME_OPT =y
CFG_USE_DCACHE =y

CFG_SUNXI_MMC =y
CFG_SUNXI_HWSPINLOCK=y
CFG_SUNXI_FDT=y
CFG_RISCV_E906=y
CFG_SUNXI_ELF=y
CFG_MELISELF_LOAD_ADDR=0x5F000000
CFG_SUNXI_NO_UPDATE_FDT_PARA=y
CFG_BOOT0_POWER_CAMERA=y
