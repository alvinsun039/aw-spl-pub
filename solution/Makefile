
#
# Makefile for sunxi bootloader
# huangrongcun@allwinnertech.com
#

include $(TOPDIR)/mk/config.mk
LIB	=  libsolution.o

LIBS-n += solution/solution_drivers/libsolution_drivers.o
LIBS-$(CFG_KERNEL_BOOTIMAGE) += solution/solution_common/libsolution_common.o

LIBS := $(addprefix $(TOPDIR)/,$(sort $(LIBS-y)))

SRCS	:= $(COBJS-y:.o=.c)
OBJS	:= $(addprefix $(obj),$(COBJS-y))
OBJS	+= $(LIBS)
all:	$(obj).depend $(LIB)

$(LIB):	$(OBJS)
	$(call cmd_link_o_target, $(OBJS))

$(LIBS): depend
	$(MAKE) -C $(dir $(subst $(OBJTREE),,$@))
#########################################################################

depend:	$(obj).depend
.PHONY: depend

# defines $(obj).depend target
include $(TOPDIR)/mk/rules.mk