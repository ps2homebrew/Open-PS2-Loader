#
# FreeType 2 configuration file to detect an PlayStation2 host platform.
# Modified by Nic (This is just a hack to make it work)
#


# Copyright 1996-2000 by
# David Turner, Robert Wilhelm, and Werner Lemberg.
#
# This file is part of the FreeType project, and may only be used, modified,
# and distributed under the terms of the FreeType project license,
# LICENSE.TXT.  By continuing to use, modify, or distribute this file you
# indicate that you have read the license and understand and accept it
# fully.


.PHONY: setup


ifeq ($(PLATFORM),ansi)

  ifdef PS2_HOST_CPU

    PLATFORM := ps2

  endif # test MACHTYPE ps2
endif

ifeq ($(PLATFORM),ps2)

  DELETE   := rm -f
  SEP      := /
  HOSTSEP  := $(SEP)
  BUILD    := $(TOP_DIR)/builds/ps2
  CONFIG_FILE := ps2.mk

  setup: std_setup

endif   # test PLATFORM ps2

# EOF

