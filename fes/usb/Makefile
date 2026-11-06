# SPDX-License-Identifier: BSD-2-Clause-Patent
# Copyright (c) 2026, edk2-a733 contributors
#
# Build the Allwinner USB/EFEX stack and the FES shims for the in-tree
# FES1.  Invoked from fes/Makefile; objects land in build/ and are linked
# into fes1.elf.
#
# Everything the compiler needs is in this directory: vendor/ carries the
# u-boot-2018 drivers/sunxi_usb sources, include/ub/ the u-boot headers
# they consume (provenance in include/ub/README).  No U-Boot tree is
# required.

TOPDIR ?= $(abspath ../..)
HERE := $(CURDIR)

CC ?= arm-linux-gnueabi-gcc
FES_MMC_DEV ?= 2
OUT := build

COMMON := -marm -march=armv7-a -mthumb-interwork -Os -g -fno-common \
	-mfpu=neon -msoft-float -ffunction-sections -fno-builtin -ffreestanding \
	-D__KERNEL__ -DCONFIG_ARM -D__ARM__ -D__NEON_SIMD__ -mabi=aapcs-linux \
	-fno-stack-protector -Wno-format-security -pipe -mno-unaligned-access \
	-D__LINUX_ARM_ARCH__=7 -DCONFIG_FES_REAL_FLASH -DCONFIG_FES_SMHC_DMA \
	-DCONFIG_FES_TX_DMA \
	-DFES_MMC_DEV=$(FES_MMC_DEV) \
	-include $(HERE)/include/generated/autoconf.h

ifeq ($(FES_VERBOSE),1)
COMMON += -DCONFIG_FES_VERBOSE
endif

INCS := -I$(HERE)/include \
	-I$(HERE) \
	-I$(HERE)/shims \
	-I$(HERE)/arch/arm/include \
	-I$(HERE)/vendor \
	-I$(HERE)/vendor/include \
	-I$(HERE)/include/ub/include \
	-I$(HERE)/include/ub/arch/arm/include \
	-I$(HERE)/include/ub/arch/arm/mach-sunxi/include \
	-I$(HERE)/include/ub/board/sunxi

USB := usbc usbc_dev usbc_dma usbc_host usbc_phy usb_base_common \
       usb_base_platform usb_dma_control usb_efex efex_queue buf_queue usb_burn
SHIMS := fes_boot shims_efex fes_mmc_write fes_mmc_glue vendor_stubs

USB_OBJS := $(addprefix $(OUT)/,$(addsuffix .o,$(USB))) $(OUT)/gic_v3.o
SHIM_OBJS := $(addprefix $(OUT)/,$(addsuffix .o,$(SHIMS)))
OBJS := $(USB_OBJS) $(SHIM_OBJS) $(OUT)/vectors_fes.o

all: $(OBJS)

# The vendor stack prints through the FES1 console shim.
$(OUT)/%.o: vendor/%.c
	@mkdir -p $(OUT)
	$(CC) $(COMMON) $(INCS) \
		-include $(HERE)/shims/fes_printf.h -c -o $@ $<

$(OUT)/gic_v3.o: vendor/gic_v3.c
	@mkdir -p $(OUT)
	$(CC) $(COMMON) $(INCS) -c -o $@ $<

$(OUT)/%.o: shims/%.c
	@mkdir -p $(OUT)
	$(CC) $(COMMON) $(INCS) -c -o $@ $<

$(OUT)/vectors_fes.o: shims/vectors_fes.s
	@mkdir -p $(OUT)
	$(CC) -marm -march=armv7-a -c -o $@ $<

clean:
	rm -rf $(OUT)

.PHONY: all clean
