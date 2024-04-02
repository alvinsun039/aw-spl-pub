
#
# Makefile for sunxi bootloader
# wangwei@allwinnertech.com
#

TOPDIR=$(CURDIR)
SRCTREE=$(TOPDIR)

#Q: quit for compile
Q = @
ifeq ("$(origin V)", "command line")
VERBOSE=$(V)
endif
ifndef VERBOSE
VERBOSE=0
endif

ifeq ($(VERBOSE),1)
Q=
CMD_ECHO_SILENT := true
else
CMD_ECHO_SILENT := echo
endif

#P: platform: sun50iw3p1/sun8iw15p1 etc.
PLATFORM=NULL
ifeq ("$(origin p)", "command line")
PLATFORM=$(p)
endif


#M: compile module: nand mmc nor etc.
MODULE=NULL
ifeq ("$(origin m)", "command line")
MODULE=$(m)
endif

#ddr: compile module: dd3/lpddr3/ddr4/lpddr4
MODULE=NULL
ifeq ("$(origin ddr)", "command line")
DRAM_TYPE=$(ddr)
DRAM_TYPE_NAME=_$(DRAM_TYPE)
endif

ifeq ("$(origin b)", "command line")
BOARD=$(b)
endif


#check input param
MK_FILE_CHANGE := $(shell if [ x$(b) != x ]; then echo yes; else echo no; fi;)
#$(info MK_FILE_CHANGE=$(MK_FILE_CHANGE))
ifeq (x$(MK_FILE_CHANGE),xyes)
	FILE_EXIST=$(shell if [ -f $(TOPDIR)/board/$(b)/common.mk ]; then echo yes; else echo no; fi;)
ifeq (x$(FILE_EXIST),xno)
$(info ***);
$(info ***configure plat or module not exist, Please run some configurator);
$(info ***e.g. make p=sun50iw3p1; make boot0);
$(info ***e.g. make p=sun50iw3p1; make sboot);
$(info ***e.g. make p=sun50iw3p1; make fes);
$(info ***e.g. make p=sun8iw15p1 ddr=ddr3|lpddr3|ddr4|lpddr4; make boot0);
$(info ***e.g. make p=sun8iw15p1 ddr=ddr3|lpddr3|ddr4|lpddr4; make sboot);
$(info ***e.g. make p=sun8iw15p1 ddr=ddr3|lpddr3|ddr4|lpddr4; make fes);
$(info ***);
$(error exit);
endif
$(shell cp -f $(TOPDIR)/board/$(b)/common.mk $(TOPDIR)/.module.common.mk)
$(shell sed -i '$$a\DRAM_TYPE_NAME=$(DRAM_TYPE_NAME)' $(TOPDIR)/.module.common.mk)
$(shell sed -i '$$a\CURRENT_BOARD=$(BOARD)' $(TOPDIR)/.module.common.mk)
endif

buildconfig = ../../../.buildconfig
ifeq ($(buildconfig), $(wildcard $(buildconfig)))
	LICHEE_BUSSINESS=$(shell cat $(buildconfig) | grep -w "LICHEE_BUSSINESS" | awk -F= '{printf $$2}')
	LICHEE_CHIP_CONFIG_DIR=$(shell cat $(buildconfig) | grep -w "LICHEE_CHIP_CONFIG_DIR" | awk -F= '{printf $$2}')
	LICHEE_PLAT_OUT=$(shell cat $(buildconfig) | grep -w "LICHEE_PLAT_OUT" | awk -F= '{printf $$2}')
	LICHEE_IC=$(shell cat  $(buildconfig)| grep -w "LICHEE_IC" | awk -F= '{printf $$2}')
	export LICHEE_BUSSINESS LICHEE_CHIP_CONFIG_DIR LICHEE_PLAT_OUT LICHEE_IC
endif
#include config file
include $(TOPDIR)/mk/config.mk
include $(TOPDIR)/mk/checkconf.mk
sinclude $(TOPDIR)/.module.common.mk

BOARD_EXIST := $(shell if [ x$(BOARD) != x ]; then echo yes; else echo no; fi;)
ifeq (x$(BOARD_EXIST),xno)
BOARD=$(CURRENT_BOARD)
endif

CP_BOARD=$(BOARD)

ifeq (x$(CPU), xriscv64)
#riscv64
TOOLCHAIN_PATH:=riscv64-linux-x86_64-20200528
else ifeq (x$(CPU), xriscv32)
#riscv32
TOOLCHAIN_PATH:=nds32le-linux-glibc-v5d
else
#arm
TOOLCHAIN_PATH:=gcc-linaro-7.2.1-2017.11-x86_64_arm-linux-gnueabi
endif
toolchain_check=$(shell if [ ! -d ../tools/toolchain/$(TOOLCHAIN_PATH) ]; then echo yes; else echo no; fi;)
ifeq (x$(toolchain_check), xyes)
$(info Prepare riscv toolchain ...);
$(shell mkdir -p ../tools/toolchain/$(TOOLCHAIN_PATH) || exit 1)
$(shell tar --strip-components=1 -xf ../tools/toolchain/$(TOOLCHAIN_PATH).tar.xz -C ../tools/toolchain/$(TOOLCHAIN_PATH) || exit 1)
endif

CPU ?= armv7
export Q SOC TOPDIR SRCTREE CMD_ECHO_SILENT MODULE PLATFORM DRAM_TYPE_NAME CPU CP_BOARD

# $(info platform=$(PLATFORM) module=$(MODULE) cp_board=$(CP_BOARD))

define build-boot0-module-if-exist
	@if [ x$(NAND_EXIST) = xyes ]; then  \
	$(MAKE) -B -C  $(SRCTREE)/nboot nand; fi
	@if [ x$(MMC_EXIST) = xyes ]; then  \
	$(MAKE) -B -C  $(SRCTREE)/nboot mmc; fi
	@if [ x$(SPINOR_EXIST) = xyes ]; then  \
	$(MAKE) -B -C  $(SRCTREE)/nboot spinor; fi
endef


define build-sboot-module-if-exist
	@if [ x$(SBOOT_EXIST) = xyes ]; then  \
	$(MAKE) -B -C  $(SRCTREE)/sboot sboot; fi
	@if [ x$(SBOOT_NOR_EXIST) = xyes ]; then  \
	$(MAKE) -B -C  $(SRCTREE)/sboot sboot_nor; fi
	@if [ x$(SBOOT_MMC_EXIST) = xyes ]; then  \
	$(MAKE) -B -C  $(SRCTREE)/sboot sboot_mmc; fi
	@if [ x$(SBOOT_NAND_EXIST) = xyes ]; then  \
	$(MAKE) -B -C  $(SRCTREE)/sboot sboot_nand; fi
	@if [ x$(SBOOT_UFS_EXIST) = xyes ]; then  \
	$(MAKE) -B -C  $(SRCTREE)/sboot sboot_ufs; fi
endef

MAKEFLAGS +=  --no-print-directory

NAND_EXIST:=$(shell if [ -f $(TOPDIR)/board/$(CP_BOARD)/nand.mk ]; then echo yes; else echo no; fi;)
MMC_EXIST:=$(shell if [ -f $(TOPDIR)/board/$(CP_BOARD)/mmc.mk ]; then echo yes; else echo no; fi;)
SPINOR_EXIST:=$(shell if [ -f $(TOPDIR)/board/$(CP_BOARD)/spinor.mk ]; then echo yes; else echo no; fi;)
SBOOT_EXIST:=$(shell if [ -f $(TOPDIR)/board/$(CP_BOARD)/sboot.mk ]; then echo yes; else echo no; fi;)
SBOOT_NOR_EXIST:=$(shell if [ -f $(TOPDIR)/board/$(CP_BOARD)/sboot_nor.mk ]; then echo yes; else echo no; fi;)
SBOOT_MMC_EXIST:=$(shell if [ -f $(TOPDIR)/board/$(CP_BOARD)/sboot_mmc.mk ]; then echo yes; else echo no; fi;)
SBOOT_NAND_EXIST:=$(shell if [ -f $(TOPDIR)/board/$(CP_BOARD)/sboot_nand.mk ]; then echo yes; else echo no; fi;)
SBOOT_UFS_EXIST:=$(shell if [ -f $(TOPDIR)/board/$(CP_BOARD)/sboot_ufs.mk ]; then echo yes; else echo no; fi;)
ifeq (x$(MK_FILE_CHANGE),xyes)
#makefile is called to config platform, do nothing
all:
	@echo "platform set to $(PLATFORM)"
	@-ln -s $(TOPDIR)/board/$(CP_BOARD)/commit_info.h include/commit_info.h
else
all: mkdepend
	$(MAKE) -B -C $(SRCTREE)/fes fes
	$(call build-boot0-module-if-exist)
ifneq (x$(BOARD_BUILD_NBOOT),x)
	@for mk in $(BOARD_BUILD_NBOOT);\
	do\
		$(MAKE) -C $(SRCTREE)/nboot $$mk;\
	done
else
	$(warning not nboot board target!!!)
endif
endif
	$(call build-sboot-module-if-exist)

ifeq (x$(NAND_EXIST),xyes)
nand: mkdepend
	$(MAKE)  -C  $(SRCTREE)/nboot nand
endif
ifeq (x$(MMC_EXIST),xyes)
mmc: mkdepend
	$(MAKE)  -C  $(SRCTREE)/nboot mmc
endif
ifeq (x$(SPINOR_EXIST),xyes)
spinor: mkdepend
	$(MAKE)  -C  $(SRCTREE)/nboot spinor
endif

$(BOARD_BUILD_NBOOT):mkdepend
	$(MAKE) -C  $(SRCTREE)/nboot $@

fes: mkdepend
	$(MAKE) -C $(SRCTREE)/fes fes
boot0: mkdepend
	$(call build-boot0-module-if-exist)
ifneq (x$(BOARD_BUILD_NBOOT),x)
	@for mk in $(BOARD_BUILD_NBOOT);\
	do\
		$(MAKE) -C $(SRCTREE)/nboot $$mk;\
	done
else
	$(warning not nboot board target!!!)
endif
ifeq (x$(SBOOT_EXIST),xyes)
sboot: mkdepend
	$(MAKE) -C $(SRCTREE)/sboot sboot
endif

ifeq (x$(SBOOT_NOR_EXIST),xyes)
sboot_nor: mkdepend
	$(MAKE) -C $(SRCTREE)/sboot sboot_nor
endif

ifeq (x$(SBOOT_MMC_EXIST),xyes)
sboot_mmc: mkdepend
	$(MAKE) -C $(SRCTREE)/sboot sboot_mmc
endif

ifeq (x$(SBOOT_NAND_EXIST),xyes)
sboot_nand: mkdepend
	$(MAKE) -C $(SRCTREE)/sboot sboot_nand
endif

ifeq (x$(SBOOT_UFS_EXIST),xyes)
sboot_ufs: mkdepend
	$(MAKE) -C $(SRCTREE)/sboot sboot_ufs
endif

clean:
	@find $(TOPDIR) -type f \
		\( -name 'core' -o -name '*.bak' -o -name '*~' \
		-o -name '*.o'	-o -name '*.exe' -o -name '*.axf' \
		-o -name '*.elf' \
		-o -name '*.depend' \
		-o -name '*.bin' \
		-o -name '*.log' \
		-o -name '*.map' \) -print \
		| xargs rm -f

	@rm -f $(TOPDIR)/nboot/boot0.lds
	@rm -f $(TOPDIR)/fes/fes1.lds
	@rm -f $(TOPDIR)/sboot/sboot.lds
	@rm -f $(TOPDIR)/autoconf.mk
	@rm -rf $(TOPDIR)/out

distclean: clean
	@rm -f $(TOPDIR)/include/config.h
	@rm -f $(TOPDIR)/include/commit_info.h
	@rm -f $(TOPDIR)/.module.common.mk
	@rm -f $(cleanfiles)

mkdepend : 
	@if [ ! -f $(TOPDIR)/.module.common.mk ]; then  \
	rm -rf autoconf.mk; \
	echo "***"; \
	echo "*** Configuration file \".module.common.mk\" not found!"; \
	echo "***"; \
	echo "*** Please run some configurator (e.g. make p=sun50iw3p1)"; \
	echo "***"; \
	exit 1; \
	fi;
	$(call update-commit-info)
depend:

PHONY +=FORCE
FORCE:
.PHONY:$(PHONY)



