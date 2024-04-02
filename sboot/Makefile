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
sinclude $(TOPDIR)/board/$(CP_BOARD)/$(MAKECMDGOALS).mk

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
SBOOT_LDSCRIPT := $(CPUDIR)/sboot.lds

# We want the final binaries in this directory
SBOOT_DIR := $(TOPDIR)/sboot/

ifdef DRAM_TYPE
	DRAM_TYPE_NAME=_$(DRAM_TYPE)
endif

LIBS-y += arch/$(ARCH)/cpu/$(CPU)/libarch.o
LIBS-y += sboot/main/libmain.o
LIBS := $(addprefix $(TOPDIR)/,$(sort $(LIBS-y)))

ifeq (x$(MAKECMDGOALS),xsboot_mmc)
A-LIBS-$(CFG_SUNXI_SBOOT)  := $(TOPDIR)/board/$(CP_BOARD)/lib$(PLATFORM)_sboot_sdcard.a
else
A-LIBS-$(CFG_SUNXI_SBOOT)  := $(TOPDIR)/board/$(CP_BOARD)/lib$(PLATFORM)_$(MAKECMDGOALS).a
endif

A-OBJS := $(A-LIBS-y:%.a=%.o)

ifdef CFG_WEAK_SYMBOL
COBJS-y += $(A-OBJS)
else
COBJS-y += $(A-LIBS-y)
endif
COBJS-y += $(TOPDIR)/sboot/main/sboot_main.o

OBJS = $(sort $(COBJS-y))

# Special flags for CPP when processing the linker script.
# Pass the version down so we can handle backwards compatibility
# on the fly.
LDPPFLAGS += \
	-DSBOOTADDR=$(CFG_SBOOT_RUN_ADDR)	 \
	-DCPUDIR=$(CPUDIR) \
	$(shell $(LD) --version | \
	  sed -ne 's/GNU ld version \([0-9][0-9]*\)\.\([0-9][0-9]*\).*/-DLD_MAJOR=\1 -DLD_MINOR=\2/p')

define build_sboot_with_suffix
	$(Q)echo bootaddr is $(CFG_SBOOT_RUN_ADDR)
	$(LD) $(OBJS) $(LIBSPINOR) $(PLATFORM_LIBGCC) $(LDFLAGS) $(LDFLAGS_GC) -Tsboot.lds -o sboot$1.elf -Map sboot$1.map
	$(OBJCOPY) $(OBJCFLAGS) -O binary  sboot$1.elf sboot$1.bin
	@$(TOPDIR)/mk/gen_check_sum $(SRCTREE)/sboot/sboot$1.bin sboot$1_$(PLATFORM)$(DRAM_TYPE_NAME).bin > /dev/null
	@$(TOPDIR)/mk/encrypto_boot0 -f sboot$1_$(PLATFORM)$(DRAM_TYPE_NAME).bin  -c $(LICHEE_IC) >/dev/null
endef

sboot:	 $(LIBS) sboot.lds
ifdef CFG_WEAK_SYMBOL
	$(OBJCOPY) $(CFG_WEAK_SYMBOL) $(A-LIBS-y) $(A-OBJS)
endif
	$(call build_sboot_with_suffix,)
	$(call copy_bin_to_target_dir, sboot_$(PLATFORM)$(DRAM_TYPE_NAME).bin)

sboot_mmc:	 $(LIBS) sboot.lds
ifdef CFG_WEAK_SYMBOL
	$(OBJCOPY) $(CFG_WEAK_SYMBOL) $(A-LIBS-y) $(A-OBJS)
endif
	$(call build_sboot_with_suffix,_sdcard)
	$(call copy_bin_to_target_dir, sboot_sdcard_$(PLATFORM)$(DRAM_TYPE_NAME).bin)

sboot_nor:	 $(LIBS) sboot.lds
ifdef CFG_WEAK_SYMBOL
	$(OBJCOPY) $(CFG_WEAK_SYMBOL) $(A-LIBS-y) $(A-OBJS)
endif
	$(call build_sboot_with_suffix,_nor)
	$(call copy_bin_to_target_dir, sboot_nor_$(PLATFORM)$(DRAM_TYPE_NAME).bin)

sboot_nand:	 $(LIBS) sboot.lds
ifdef CFG_WEAK_SYMBOL
	$(OBJCOPY) $(CFG_WEAK_SYMBOL) $(A-LIBS-y) $(A-OBJS)
endif
	$(call build_sboot_with_suffix,_nand)
	$(call copy_bin_to_target_dir, sboot_nand_$(PLATFORM)$(DRAM_TYPE_NAME).bin)

sboot_ufs:	 $(LIBS) sboot.lds
ifdef CFG_WEAK_SYMBOL
	$(OBJCOPY) $(CFG_WEAK_SYMBOL) $(A-LIBS-y) $(A-OBJS)
endif
	$(call build_sboot_with_suffix,_ufs)
	$(call copy_bin_to_target_dir, sboot_ufs_$(PLATFORM)$(DRAM_TYPE_NAME).bin)

sboot_$(DRAM_TYPE):sboot


$(LIBS): depend
	$(MAKE) -C $(dir $@)

sboot.lds: $(SBOOT_LDSCRIPT)
	$(Q)$(CPP) $(ALL_CFLAGS) $(LDPPFLAGS) -ansi -D__ASSEMBLY__ -P - <$^ >$@

$(NBOOT_DIR)cur.log:
	@git show HEAD --pretty=format:"%H" | head -n 1 > cur.log

depend: .depend build-confs
sinclude $(TOPDIR)/mk/target_for_conf.mk
#########################################################################

# defines $(NBOOT_DIR).depend target
include $(TOPDIR)/mk/rules.mk

sinclude .depend

#########################################################################
