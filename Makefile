#---------------------------------------------------------------------------------------------------------------------
# gbawriter 10.0
# Copyright (c) 2026 Halim Jarrar. GPL-3.0-or-later, see LICENSE and NOTICES.md.
#
# Stack: Butano + devkitPro devkitARM, C++.
# Adapted from the canonical Butano text example through GBAReader v0.5.0.
#
# Build:    make LIBBUTANO=/absolute/path/to/butano/butano
# Clean:    make clean LIBBUTANO=...
#
# Requirements:
#   - devkitPro devkitARM (DEVKITPRO and DEVKITARM set)
#   - LIBBUTANO set to the pinned Butano checkout (see README)
#   - the gbasd submodule checked out (git submodule update --init)
#---------------------------------------------------------------------------------------------------------------------

TARGET      :=  gbawriter
BUILD       :=  build
LIBBUTANO   ?=  /path/to/butano/butano
PYTHON      :=  python3
SOURCES     :=  src gbasd/src gbasd/fatfs
INCLUDES    :=  include gbasd/include gbasd/fatfs third_party/superfw/src third_party/superfw/src/fonts
# butano treats INCLUDES as relative to CURDIR. Symlink the common
# headers we need into our own include dir so butano finds them.
DATA        :=
GRAPHICS    :=  graphics $(LIBBUTANO)/../common/graphics
AUDIO       :=
AUDIOBACKEND :=  null
AUDIOTOOL   :=
DMGAUDIO    :=
DMGAUDIOBACKEND :=  null
ROMTITLE    :=  GBA WRITER
ROMCODE     :=  AGBW
# Optional Supercard second-ROM-mirror transfers. Default 0 preserves the
# release-safe path; use `make SC_FAST_ROM_MIRROR=1 ...` only for hardware tests.
SC_FAST_ROM_MIRROR ?= 0
USERFLAGS   :=  -DSC_FAST_ROM_MIRROR=$(SC_FAST_ROM_MIRROR) -fstack-usage
USERCXXFLAGS :=
USERASFLAGS :=
USERLDFLAGS :=
USERLIBDIRS :=
USERLIBS    :=
DEFAULTLIBS :=
STACKTRACE  :=
USERBUILD   :=
EXTTOOL     :=

#---------------------------------------------------------------------------------------------------------------------
# Export absolute butano path:
#---------------------------------------------------------------------------------------------------------------------
ifndef LIBBUTANOABS
	export LIBBUTANOABS	:=	$(realpath $(LIBBUTANO))
endif

#---------------------------------------------------------------------------------------------------------------------
# Include main makefile:
#---------------------------------------------------------------------------------------------------------------------
include $(LIBBUTANOABS)/butano.mak

#---------------------------------------------------------------------------------------------------------------------
# Test targets (make test, host-test, memory-check) live with the tests.
#---------------------------------------------------------------------------------------------------------------------
-include tests/tests.mk
