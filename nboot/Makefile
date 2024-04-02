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
ifneq ($(LICHEE_BUSSINESS), )
	TARGETDIR=$(LICHEE_CHIP_CONFIG_DIR)/$(LICHEE_BUSSINESS)/bin
else
	TARGETDIR=$(LICHEE_CHIP_CONFIG_DIR)/bin
endif
endif

CPU ?= armv7
CPUDIR = $(TOPDIR)/arch/$(ARCH)/cpu/$(CPU)
SOLUTIONDIR = $(TOPDIR)/solution
BOOT0_LDSCRIPT := $(CPUDIR)/boot0.lds

# We want the final binaries in this directory
NBOOT_DIR := $(TOPDIR)/nboot/
obj := $(TOPDIR)/nboot/


LIBS-y  += arch/$(ARCH)/cpu/$(CPU)/libarch.o
LIBS-y += nboot/main/libmain.o
LIBS := $(addprefix $(TOPDIR)/,$(sort $(LIBS-y)))

ifeq ($(MAKECMDGOALS), mmc)
A-LIBS-$(CFG_SUNXI_SDMMC)   := $(TOPDIR)/board/$(CP_BOARD)/lib$(PLATFORM)_sdcard.a
else
A-LIBS-y  := $(TOPDIR)/board/$(CP_BOARD)/lib$(PLATFORM)_$(MAKECMDGOALS).a
endif

ifeq (x$(MAKECMDGOALS),x$(BOARD_BUILD_NBOOT))
A-LIBS-y   := $(TOPDIR)/board/$(CP_BOARD)/lib$(PLATFORM)_$(BOARD_BUILD_NBOOT).a
endif


A-OBJS := $(A-LIBS-y:%.a=%.o)

ifdef CFG_WEAK_SYMBOL
COBJS-y += $(A-OBJS)
else
COBJS-y += $(A-LIBS-y)
endif

SULUTION_LIBS-y += solution/libsolution.o
SULUTION_LIBS := $(addprefix $(TOPDIR)/,$(sort $(SULUTION_LIBS-y)))

COBJS-y += $(TOPDIR)/nboot/main/boot0_main.o

OBJS = $(sort $(COBJS-y))

ifndef CONFIG_BOOT0_SIZE_LIMIT
	BOOT0SIZE = $(CFG_SYS_INIT_RAM_SIZE)
else
	BOOT0SIZE = $(CFG_BOOT0_SIZE_LIMIT)
endif

# Special flags for CPP when processing the linker script.
# Pass the version down so we can handle backwards compatibility
# on the fly.
LDPPFLAGS += \
	-DBOOT0ADDR=$(CFG_BOOT0_RUN_ADDR)	 \
	-DBOOT0SIZE=$(BOOT0SIZE)	 \
	-DCPUDIR=$(CPUDIR) \
	-DSOLUTIONDIR=$(SOLUTIONDIR) \
	$(shell $(LD) --version | \
	  sed -ne 's/GNU ld version \([0-9][0-9]*\)\.\([0-9][0-9]*\).*/-DLD_MAJOR=\1 -DLD_MINOR=\2/p')


define build_boot0_with_suffix
	$(Q)echo bootaddr is $(CFG_BOOT0_RUN_ADDR)
	$(LD) $(OBJS)  $(SULUTION_LIBS) $(PLATFORM_LIBGCC) $(LDFLAGS) $(LDFLAGS_GC) -Tboot0.lds -o boot0$1.elf -Map boot0$1.map
	$(OBJCOPY) $(OBJCFLAGS) -O binary  boot0$1.elf boot0$1.bin
	@$(TOPDIR)/mk/gen_check_sum boot0$1.bin boot0$1_$(PLATFORM)$(DRAM_TYPE_NAME).bin > /dev/null
	@$(TOPDIR)/mk/encrypto_boot0 -f boot0$1_$(PLATFORM)$(DRAM_TYPE_NAME).bin  -c $(LICHEE_IC) >/dev/null
endef

$(BOARD_BUILD_NBOOT): $(LIBS)  $(SULUTION_LIBS) boot0.lds
ifdef CFG_WEAK_SYMBOL
	$(OBJCOPY) $(CFG_WEAK_SYMBOL) $(A-LIBS-y) $(A-OBJS)
endif
	$(call build_boot0_with_suffix,_$@)
	$(call copy_bin_to_target_dir, boot0_$@_$(PLATFORM)$(DRAM_TYPE_NAME).bin)

spinor:	 $(LIBS)  $(SULUTION_LIBS) boot0.lds
ifdef CFG_WEAK_SYMBOL
	$(OBJCOPY) $(CFG_WEAK_SYMBOL) $(A-LIBS-y) $(A-OBJS)
endif
	$(call build_boot0_with_suffix,_spinor)
	$(call copy_bin_to_target_dir, boot0_spinor_$(PLATFORM)$(DRAM_TYPE_NAME).bin)

mmc:	 $(LIBS)  $(SULUTION_LIBS) boot0.lds
ifdef CFG_WEAK_SYMBOL
	$(OBJCOPY) $(CFG_WEAK_SYMBOL) $(A-LIBS-y) $(A-OBJS)
endif
	$(call build_boot0_with_suffix,_sdcard)
	$(call copy_bin_to_target_dir, boot0_sdcard_$(PLATFORM)$(DRAM_TYPE_NAME).bin)

nand:	 $(LIBS)  $(SULUTION_LIBS) boot0.lds
ifdef CFG_WEAK_SYMBOL
	$(OBJCOPY) $(CFG_WEAK_SYMBOL) $(A-LIBS-y) $(A-OBJS)
endif
	$(call build_boot0_with_suffix,_nand)
	$(call copy_bin_to_target_dir, boot0_nand_$(PLATFORM)$(DRAM_TYPE_NAME).bin)

mmc_$(DRAM_TYPE):mmc
nand_$(DRAM_TYPE):nand


$(LIBS): depend
	$(MAKE) -C $(dir $(subst $(OBJTREE),,$@))

$(SULUTION_LIBS): depend
	$(MAKE) -C $(dir $(subst $(OBJTREE),,$@))

$(LIBNAND): depend
	$(MAKE) -C $(dir $(subst $(OBJTREE),,$@))

$(LIBMMC): depend
	$(MAKE) -C $(dir $(subst $(OBJTREE),,$@))

$(LIBSPINOR): depend
	$(MAKE) -C $(dir $(subst $(OBJTREE),,$@))

boot0.lds: $(BOOT0_LDSCRIPT)
	$(Q)$(CPP) $(ALL_CFLAGS) $(LDPPFLAGS) -ansi -D__ASSEMBLY__ -P - <$^ >$@

$(NBOOT_DIR)cur.log:
	@git show HEAD --pretty=format:"%H" | head -n 1 > cur.log

sinclude $(TOPDIR)/mk/target_for_conf.mk
depend: .depend build-confs
#########################################################################

# defines $(NBOOT_DIR).depend target
include $(TOPDIR)/mk/rules.mk

sinclude .depend

#########################################################################
