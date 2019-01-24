# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_slice.a

CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR) -O3
LDLIBS += -lrte_msr

EXPORT_MAP := rte_slice_version.map

LIBABIVER := 3

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_SLICE) +=  rte_slice.c
# install includes
SYMLINK-$(CONFIG_RTE_LIBRTE_SLICE)-include := rte_slice.h

include $(RTE_SDK)/mk/rte.lib.mk
