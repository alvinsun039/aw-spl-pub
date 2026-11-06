
PLATFORM_OUTDIR:=$(TOPDIR)/out/$(PLATFORM)
exist = $(shell if [ -d $(PLATFORM_OUTDIR) ]; then echo "exist"; else echo "notexist"; fi;)
ifneq (x$(exist), xexist)
$(shell mkdir -p $(PLATFORM_OUTDIR))
$(shell mkdir -p $(PLATFORM_OUTDIR)/bin)
$(shell mkdir -p $(PLATFORM_OUTDIR)/lib)
endif

exist = $(shell if [ -d $(PLATFORM_OUTDIR)/$(MAKECMDGOALS) ]; then echo "exist"; else echo "notexist"; fi;)
ifeq (x$(exist), xexist)
$(shell rm -rf $(PLATFORM_OUTDIR)/$(MAKECMDGOALS))
endif
$(shell mkdir -p $(PLATFORM_OUTDIR)/$(MAKECMDGOALS))

$(addprefix $(MAKECMDGOALS)/, $(patsubst %.c,%.d,$(notdir $(SRC_C))))

# $1:src file
define copy_bin_to_target_dir
	@-cp -v $1 $(PLATFORM_OUTDIR)/bin
	@-if [ -n "$(TARGETDIR)" ]; then cp -v $1 "$(TARGETDIR)/"; fi
	@-if [ -n "$(LICHEE_PLAT_OUT)" ]; then cp -v $1 "$(LICHEE_PLAT_OUT)/"; fi
endef

define copy_platform_outputfile_to_target_dir
	@-cp -v $1 $(PLATFORM_OUTDIR)/
	@-if [ -n "$(TARGETDIR)" ]; then cp -v $1 "$(TARGETDIR)/"; fi
	@-if [ -n "$(LICHEE_PLAT_OUT)" ]; then cp -v $1 "$(LICHEE_PLAT_OUT)/"; fi
endef