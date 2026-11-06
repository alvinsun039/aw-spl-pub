#
# (C) Copyright 2000-2011
# Wolfgang Denk, DENX Software Engineering, wd@denx.de.
#
# (C) Copyright 2011
# Daniel Schwierzeck, daniel.schwierzeck@googlemail.com.
#
# (C) Copyright 2011
# Texas Instruments Incorporated - http://www.ti.com/
# Aneesh V <aneesh@ti.com>
#
# See the file COPYING in the root directory of the source tree for details.
#
# Based on top-level Makefile.
#

SKIP_AUTO_CONF:=yes
include $(TOPDIR)/mk/config.mk
include $(TOPDIR)/mk/out_file_manage.mk
include $(TOPDIR)/.module.common.mk
include $(TOPDIR)/board/$(CP_BOARD)/$(MAKECMDGOALS).mk


#This for Tina system, to copy bin to tina SDK
ifeq ($(TARGET_BUILD_VARIANT),tina)
tina_path=$(TOPDIR)/../../../target/allwinner/$(TARGET_PLATFORM)-common/bin
exist = $(shell if [ -d $(tina_path) ]; then echo "exist"; else echo "notexist"; fi;)
ifeq ($(exist),exist)
TARGETDIR=$(TOPDIR)/../../../target/allwinner/$(TARGET_PLATFORM)-common/bin
endif

tina_path=$(TOPDIR)/../../../device/config/chips/$(TARGET_PLATFORM)/bin
exist = $(shell if [ -d $(tina_path) ]; then echo "exist"; else echo "notexist"; fi;)
ifeq ($(exist),exist)
TARGETDIR=$(TOPDIR)/../../../device/config/chips/$(TARGET_PLATFORM)/bin
endif

else
#TOOLS_DIR  := $(TOPDIR)/../../../device/config
#TARGETDIR=$(TOPDIR)/../../../device/config/chips/$(PLATFORM)/bin
# Only stage into the vendor tree when its config path is known; a
# standalone build otherwise resolves TARGETDIR to "/bin".
ifneq ($(LICHEE_CHIP_CONFIG_DIR),)
ifneq ($(LICHEE_BUSSINESS), )
       TARGETDIR=$(LICHEE_CHIP_CONFIG_DIR)/$(LICHEE_BUSSINESS)/bin
else
       TARGETDIR=$(LICHEE_CHIP_CONFIG_DIR)/bin
endif
endif
endif
# Allwinner ships the DRAM training code as a prebuilt object in the
# vendor workspace (dramlib/<platform>/spl_libdram/libdram); it is not
# part of this repository.  Override FES_DRAM_LIB for another layout.
FES_DRAM_LIB ?= $(TOPDIR)/../dramlib/$(PLATFORM)/spl_libdram/libdram

CPUDIR := $(TOPDIR)/arch/$(ARCH)/cpu/$(CPU)
FES_LDSCRIPT := $(CPUDIR)/fes1.lds

# We want the final binaries in this directory
FES1_DIR := $(TOPDIR)/fes/

# Vendor USB EFEX stack + FES shims, built in-tree by fes/usb/Makefile.
FES_MMC_DEV ?= 2
FES_USB_DIR := $(TOPDIR)/fes/usb/build
FES_USB_STAMP := $(FES_USB_DIR)/.built
FES_USB_OBJS := $(FES_USB_DIR)/usbc.o $(FES_USB_DIR)/usbc_dev.o $(FES_USB_DIR)/usbc_dma.o \
	$(FES_USB_DIR)/usbc_host.o $(FES_USB_DIR)/usbc_phy.o $(FES_USB_DIR)/usb_base_common.o \
	$(FES_USB_DIR)/usb_base_platform.o $(FES_USB_DIR)/usb_dma_control.o $(FES_USB_DIR)/usb_efex.o \
	$(FES_USB_DIR)/efex_queue.o $(FES_USB_DIR)/buf_queue.o $(FES_USB_DIR)/usb_burn.o \
	$(FES_USB_DIR)/gic_v3.o $(FES_USB_DIR)/shims_efex.o $(FES_USB_DIR)/fes_boot.o \
	$(FES_USB_DIR)/vectors_fes.o \
	$(FES_USB_DIR)/fes_mmc_write.o $(FES_USB_DIR)/fes_mmc_glue.o
obj := $(TOPDIR)/fes/

ifdef DRAM_TYPE
	DRAM_TYPE_NAME=_$(DRAM_TYPE)
endif


LIBS-y += arch/$(ARCH)/cpu/$(CPU)/libarch.o
LIBS-y += fes/main/libmain.o
LIBS := $(addprefix $(TOPDIR)/,$(sort $(LIBS-y)))

A-LIBS-$(CFG_SUNXI_FES)  := $(TOPDIR)/board/$(CP_BOARD)/lib$(PLATFORM)_fes.a

A-OBJS := $(A-LIBS-y:%.a=%.o)

ifdef CFG_WEAK_SYMBOL
COBJS-y += $(A-OBJS)
else
COBJS-y += $(A-LIBS-y)
endif
COBJS-y += $(TOPDIR)/fes/main/fes1_main.o

OBJS = $(sort $(COBJS-y))

# Special flags for CPP when processing the linker script.
# Pass the version down so we can handle backwards compatibility
# on the fly.
LDPPFLAGS += \
	-DFES1ADDR=$(CFG_FES1_RUN_ADDR)	 \
	-DCPUDIR=$(CPUDIR) \
	$(shell $(LD) --version | \
	  sed -ne 's/GNU ld version \([0-9][0-9]*\)\.\([0-9][0-9]*\).*/-DLD_MAJOR=\1 -DLD_MINOR=\2/p')


# The vendor sdcard archive marks its stack executable and the linker
# script emits a single RWX load segment.  Both are expected before the
# MMU is up; force a non-exec stack and keep the link output quiet.
LDFLAGS += -z noexecstack --no-warn-rwx-segments

$(FES_USB_STAMP): $(TOPDIR)/fes/usb/Makefile \
		$(wildcard $(TOPDIR)/fes/usb/vendor/*.c) \
		$(wildcard $(TOPDIR)/fes/usb/shims/*.c) \
		$(wildcard $(TOPDIR)/fes/usb/shims/*.s)
	$(MAKE) -C $(TOPDIR)/fes/usb CC="$(CC)" TOPDIR="$(TOPDIR)" FES_MMC_DEV="$(FES_MMC_DEV)"
	@touch $@

$(FES_USB_OBJS): $(FES_USB_STAMP)

$(FES_DRAM_LIB):
	@echo "error: DRAM training object not found: $(FES_DRAM_LIB)" >&2
	@echo "       set FES_DRAM_LIB to the vendor dramlib object" >&2
	@false

# The vendor top-level Makefile creates this symlink only in its
# configure pass, so a one-shot build from a fresh checkout misses it.
$(TOPDIR)/include/commit_info.h: $(TOPDIR)/board/$(CP_BOARD)/commit_info.h
	@ln -sf $< $@

# fes1_head.c includes it, so it must exist before any lib object builds.
$(LIBS): | $(TOPDIR)/include/commit_info.h

fes: $(LIBS) fes1.lds $(FES_USB_STAMP) $(FES_DRAM_LIB)
ifdef CFG_WEAK_SYMBOL
	$(OBJCOPY) $(CFG_WEAK_SYMBOL) $(A-LIBS-y) $(A-OBJS)
endif
	$(LD) $(FES_DRAM_LIB) $(TOPDIR)/board/$(CP_BOARD)/lib$(PLATFORM)_sdcard.a $(OBJS) $(FES_USB_OBJS) $(PLATFORM_LIBGCC) $(LDFLAGS) $(LDFLAGS_GC) --allow-multiple-definition --defsym=BT0_head=fes1_head -Tfes1.lds -o fes1.elf -Map fes1.map
	$(OBJCOPY) $(OBJCFLAGS) -O binary fes1.elf fes1.bin
	$(Q)$(TOPDIR)/mk/gen_check_sum fes1.bin fes1_$(PLATFORM)$(DRAM_TYPE_NAME).bin > /dev/null
	@-cp -v $(TOPDIR)/fes/fes1.map $(PLATFORM_OUTDIR)/fes1.map
	@-cp -v $(TOPDIR)/fes/fes1.elf $(PLATFORM_OUTDIR)/fes1.elf
	$(call copy_bin_to_target_dir, fes1_$(PLATFORM)$(DRAM_TYPE_NAME).bin)


$(LIBS): depend
	$(MAKE) -C $(dir $@)


fes1.lds: $(FES_LDSCRIPT)
	$(Q)$(CPP) $(ALL_CFLAGS) $(LDPPFLAGS) -ansi -D__ASSEMBLY__ -P - <$^ >$@


sinclude $(TOPDIR)/mk/target_for_conf.mk
depend: .depend build-confs
#########################################################################

# defines $(NBOOT_DIR).depend target
include $(TOPDIR)/mk/rules.mk

sinclude .depend

#########################################################################
