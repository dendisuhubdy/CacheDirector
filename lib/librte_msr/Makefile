# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_msr.a

CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR) -O3
LDLIBS += 

EXPORT_MAP :=  rte_msr_version.map

LIBABIVER := 3

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_MSR) +=  rte_msr.c
# install includes
SYMLINK-$(CONFIG_RTE_LIBRTE_MSR)-include := rte_msr.h

include $(RTE_SDK)/mk/rte.lib.mk
