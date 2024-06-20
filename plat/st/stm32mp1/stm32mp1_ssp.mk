#
# Copyright (c) 2015-2024, ARM Limited and Contributors. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

ST_VERSION 		:=	r2.1-ssp
VERSION_STRING		:=	v${VERSION_MAJOR}.${VERSION_MINOR}-${PLAT}-${ST_VERSION}(${BUILD_TYPE}):${BUILD_STRING}

# Required to use BL2_IN_XIP_MEM
BL2_IN_XIP_MEM		:= 	1

SEPARATE_CODE_AND_RODATA :=	1

TRUSTED_BOARD_BOOT	:=	0

# Macros and rules to build TF-A binary
STM32_TF_STM32		:=	$(addprefix ${BUILD_PLAT}/tf-a-ssp-, $(patsubst %.dtb,%.stm32,$(DTB_FILE_NAME)))

PLAT_BL_COMMON_SOURCES	:=	common/fdt_wrappers.c					\
				common/uuid.c						\
				plat/st/common/stm32mp_common.c				\
				plat/st/stm32mp1/stm32mp1_private.c

PLAT_BL_COMMON_SOURCES	+=	drivers/st/uart/aarch32/stm32_console.S

PLAT_BL_COMMON_SOURCES	+=	drivers/st/regulator/regulator_core.c			\
				drivers/st/regulator/regulator_fixed.c

PLAT_BL_COMMON_SOURCES	+=	${XLAT_TABLES_LIB_SRCS}

PLAT_BL_COMMON_SOURCES	+=	lib/cpus/aarch32/cortex_a7.S

PLAT_BL_COMMON_SOURCES	+=	drivers/arm/tzc/tzc400.c				\
				drivers/clk/clk.c					\
				drivers/delay_timer/delay_timer.c			\
				drivers/delay_timer/generic_delay_timer.c		\
				drivers/st/bsec/bsec2.c					\
				drivers/st/clk/stm32mp_clkfunc.c			\
				drivers/st/gpio/stm32_gpio.c				\
				drivers/st/i2c/stm32_i2c.c				\
				drivers/st/iwdg/stm32_iwdg.c				\
				drivers/st/pmic/stm32mp_pmic.c				\
				drivers/st/pmic/stpmic1.c				\
				drivers/st/reset/stm32mp1_reset.c			\
				plat/st/common/stm32mp_dt.c				\
				plat/st/stm32mp1/stm32mp1_dbgmcu.c			\
				plat/st/stm32mp1/stm32mp1_helper.S			\
				plat/st/stm32mp1/stm32mp1_syscfg.c

ifeq ($(STM32MP13),1)
PLAT_BL_COMMON_SOURCES	+=	drivers/st/clk/clk-stm32-core.c				\
				drivers/st/clk/clk-stm32mp13.c
endif
ifeq ($(STM32MP15),1)
PLAT_BL_COMMON_SOURCES	+=	drivers/st/clk/stm32mp1_clk.c
endif

BL2_SOURCES		:=	drivers/io/io_storage.c					\
				plat/st/stm32mp1/stm32mp1_ssp.c

ifeq ($(STM32MP15),1)
BL2_SOURCES		+=	drivers/st/crypto/stm32_hash.c
endif


ifeq (${STM32MP_UART_PROGRAMMER},1)
BL2_SOURCES		+=	drivers/st/uart/stm32_uart.c				\
				plat/st/common/stm32cubeprogrammer_uart.c
endif

ifeq (${STM32MP_USB_PROGRAMMER},1)
#The DFU stack uses only one end point, reduce the USB stack footprint
$(eval $(call add_define_val,CONFIG_USBD_EP_NB,1U))
BL2_SOURCES		+=	drivers/st/usb/stm32mp1_usb.c				\
				drivers/usb/usb_device.c				\
				plat/st/common/stm32cubeprogrammer_usb.c		\
				plat/st/common/usb_dfu.c				\
				plat/st/stm32mp1/stm32mp1_usb_dfu.c
endif

ifeq ($(STM32MP13),1)
BL2_DTSI		:=	stm32mp13-ssp-bl2.dtsi
endif
ifeq ($(STM32MP15),1)
BL2_DTSI		:=	stm32mp15-ssp-bl2.dtsi
endif

check_boot_ssp:
	@if ([ ${STM32MP_UART_PROGRAMMER} = 1 ] && [ ${STM32MP_USB_PROGRAMMER} = 1 ]) || \
	([ ${STM32MP_UART_PROGRAMMER} = 0 ] && [ ${STM32MP_USB_PROGRAMMER} = 0 ]); then \
		echo "Error selecting serial boot device"; \
		false; \
	fi

bl2: check_boot_ssp

${BUILD_PLAT}/stm32mp1-ssp-%.o: ${BUILD_PLAT}/fdts/%-bl2.dtb plat/st/stm32mp1/stm32mp1.S bl2
	@echo "  SSP AS      stm32mp1.S"
	${Q}${AS} ${ASFLAGS} ${TF_CFLAGS} \
		-DDTB_BIN_PATH=\"$<\" \
		-c plat/st/stm32mp1/stm32mp1.S -o $@
