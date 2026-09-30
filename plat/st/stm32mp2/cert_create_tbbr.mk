#
# Copyright (c) 2025-2026, STMicroelectronics - All Rights Reserved
#
# SPDX-License-Identifier: BSD-3-Clause
#

# Override TBBR Cert to update generic certificate

CRTTOOL_DEFINES += PLAT_DEF_OID
CRTTOOL_DEFINES += PDEF_CERTS
CRTTOOL_DEFINES += PDEF_EXTS

# Check the STM32MP_LEGACY_DDR_FW_OID is passed to the command line
# The definition in plat/st/common/common.mk is not forwarded to this makefile
ifeq (${STM32MP_LEGACY_DDR_FW_OID},1)
CRTTOOL_DEFINES += STM32MP_LEGACY_DDR_FW_OID=1
else
CRTTOOL_DEFINES += STM32MP_LEGACY_DDR_FW_OID=0
endif

CRTTOOL_INCLUDE_DIRS	+= ${PLAT_DIR}include

CRTTOOL_SOURCES		+= ${PLAT_DIR}stm32mp2_tbb_cert.c
