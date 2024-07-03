/*
 * Copyright (c) 2023-2024, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef STM32MP2_DEF_H
#define STM32MP2_DEF_H

#include <common/tbbr/tbbr_img_def.h>
#include <drivers/st/stm32mp2_pwr.h>
#include <drivers/st/stm32mp2_rcc.h>
#include <dt-bindings/clock/stm32mp25-clks.h>
#include <dt-bindings/clock/stm32mp25-clksrc.h>
#include <dt-bindings/gpio/stm32-gpio.h>
#include <dt-bindings/reset/stm32mp25-resets.h>
#include <dt-bindings/soc/rif.h>

#ifndef __ASSEMBLER__
#include <drivers/st/bsec.h>
#include <drivers/st/stm32mp2_clk.h>

#include <boot_api.h>
#include <stm32mp_common.h>
#include <stm32mp_dt.h>
#include <stm32mp_shared_resources.h>
#include <stm32mp2_private.h>
#endif

/*******************************************************************************
 * CHIP ID
 ******************************************************************************/
#define STM32MP2_CHIP_ID	U(0x505)

#define STM32MP251A_PART_NB	U(0x400B3E6D)
#define STM32MP251C_PART_NB	U(0x000B306D)
#define STM32MP251D_PART_NB	U(0xC00B3E6D)
#define STM32MP251F_PART_NB	U(0x800B306D)
#define STM32MP253A_PART_NB	U(0x400B3E0C)
#define STM32MP253C_PART_NB	U(0x000B300C)
#define STM32MP253D_PART_NB	U(0xC00B3E0C)
#define STM32MP253F_PART_NB	U(0x800B300C)
#define STM32MP255A_PART_NB	U(0x40082E00)
#define STM32MP255C_PART_NB	U(0x00082000)
#define STM32MP255D_PART_NB	U(0xC0082E00)
#define STM32MP255F_PART_NB	U(0x80082000)
#define STM32MP257A_PART_NB	U(0x40002E00)
#define STM32MP257C_PART_NB	U(0x00002000)
#define STM32MP257D_PART_NB	U(0xC0002E00)
#define STM32MP257F_PART_NB	U(0x80002000)

#define STM32MP2_REV_B		U(0x10)
#define STM32MP2_REV_X		U(0x12)
#define STM32MP2_REV_Y		U(0x11)
#define STM32MP2_REV_Z		U(0x09)

/*******************************************************************************
 * PACKAGE ID
 ******************************************************************************/
#define STM32MP25_PKG_CUSTOM		U(0)
#define STM32MP25_PKG_AL_VFBGA361	U(1)
#define STM32MP25_PKG_AK_VFBGA424	U(3)
#define STM32MP25_PKG_AI_TFBGA436	U(5)
#define STM32MP25_PKG_UNKNOWN		U(7)

/*******************************************************************************
 * STM32MP2 memory map related constants
 ******************************************************************************/
#define STM32MP_SYSRAM_BASE		U(0x0E000000)
#define STM32MP_SYSRAM_SIZE		U(0x00040000)
#define SRAM1_BASE			U(0x0E040000)
#define SRAM1_SIZE_FOR_TFA		U(0x00010000)
#define RETRAM_BASE			U(0x0E080000)
#define RETRAM_SIZE			U(0x00020000)
#define STM32MP_BACKUP_RAM_BASE		U(0x42000000)

/* the first 4KB of SRAM1 are reserved are for BSEC shadow */
#define STM32MP_SEC_SRAM1_SIZE		U(0x1000)

#define STM32MP_SEC_SYSRAM_BASE		STM32MP_SYSRAM_BASE

#if defined(IMAGE_BL2)
#if STM32MP_USB_PROGRAMMER
#define STM32MP_SEC_DEVICE_SYSRAM_SIZE		STM32MP_USB_DWC3_SIZE
#define STM32MP_SEC_DEVICE_SYSRAM_BASE		(STM32MP_SYSRAM_BASE + \
						 STM32MP_SYSRAM_SIZE - \
						 STM32MP_SEC_DEVICE_SYSRAM_SIZE)

#define STM32MP_SEC_SYSRAM_SIZE		(STM32MP_SYSRAM_SIZE - \
					 STM32MP_SEC_DEVICE_SYSRAM_SIZE)

#define STM32MP_USB_DWC3_BASE		STM32MP_SEC_DEVICE_SYSRAM_BASE
#define STM32MP_USB_DWC3_SIZE		PAGE_SIZE
#else /* STM32MP_USB_PROGRAMMER */
#define STM32MP_SEC_SYSRAM_SIZE		STM32MP_SYSRAM_SIZE
#endif /* STM32MP_USB_PROGRAMMER */
#else /* IMAGE_BL2 */
#define STM32MP_NS_SYSRAM_SIZE		PAGE_SIZE
#define STM32MP_NS_SYSRAM_BASE		(STM32MP_SYSRAM_BASE + \
					 STM32MP_SYSRAM_SIZE - \
					 STM32MP_NS_SYSRAM_SIZE)

#define STM32MP_SCMI_NS_SHM_BASE	STM32MP_NS_SYSRAM_BASE
#define STM32MP_SCMI_NS_SHM_SIZE	STM32MP_NS_SYSRAM_SIZE

#define STM32MP_SEC_SYSRAM_SIZE		(STM32MP_SYSRAM_SIZE - \
					 STM32MP_NS_SYSRAM_SIZE)
#endif

/* DDR configuration */
#define STM32MP_DDR_BASE		U(0x80000000)
#define STM32MP_DDR_MAX_SIZE		UL(0x100000000)	/* Max 4GB */
#define STM32MP_DDR_S_SIZE		U(0)
#define STM32MP_DDR_SHMEM_SIZE		U(0)

/* DDR power initializations */
#ifndef __ASSEMBLER__
enum ddr_type {
	STM32MP_DDR3,
	STM32MP_DDR4,
	STM32MP_LPDDR4
};
#endif

/* Section used inside TF binaries */
#define STM32MP_PARAM_LOAD_SIZE		U(0x00002400)	/* 9 KB for param */
/* 512 Octets reserved for header */
#define STM32MP_HEADER_SIZE		U(0x00000200)
#define STM32MP_HEADER_BASE		(STM32MP_SEC_SYSRAM_BASE +	\
					 STM32MP_PARAM_LOAD_SIZE)

/* round_up(STM32MP_PARAM_LOAD_SIZE + STM32MP_HEADER_SIZE, PAGE_SIZE) */
#define STM32MP_HEADER_RESERVED_SIZE	U(0x3000)

#define STM32MP_BINARY_BASE		(STM32MP_SEC_SYSRAM_BASE +	\
					 STM32MP_PARAM_LOAD_SIZE +	\
					 STM32MP_HEADER_SIZE)

#define STM32MP_BINARY_SIZE		(STM32MP_SEC_SYSRAM_SIZE -	\
					 (STM32MP_PARAM_LOAD_SIZE +	\
					  STM32MP_HEADER_SIZE))

#define STM32MP_BL31_SIZE		U(0x0001F000)	/* 124 KB for BL31 */

#if TRUSTED_BOARD_BOOT
#if STM32MP_UART_PROGRAMMER || STM32MP_USB_PROGRAMMER
#define STM32MP_BL2_RO_SIZE		U(0x00021000)	/* 132 KB */
#define STM32MP_BL2_SIZE		U(0x0002C000)	/* 176 KB for BL2 */
#else /* STM32MP_UART_PROGRAMMER || STM32MP_USB_PROGRAMMER */
#define STM32MP_BL2_RO_SIZE		U(0x00021000)	/* 132 KB */
#define STM32MP_BL2_SIZE		U(0x0002D000)	/* 180 KB for BL2 */
#endif /* STM32MP_UART_PROGRAMMER || STM32MP_USB_PROGRAMMER */
#else
#if STM32MP_UART_PROGRAMMER || STM32MP_USB_PROGRAMMER
#define STM32MP_BL2_RO_SIZE		U(0x0001F000)	/* 124 KB */
#define STM32MP_BL2_SIZE		U(0x00028000)	/* 160 KB for BL2 */
#else /* STM32MP_UART_PROGRAMMER || STM32MP_USB_PROGRAMMER */
#define STM32MP_BL2_RO_SIZE		U(0x00020000)	/* 128 KB */
#define STM32MP_BL2_SIZE		U(0x00029000)	/* 164 KB for BL2 */
#endif /* STM32MP_UART_PROGRAMMER || STM32MP_USB_PROGRAMMER */
#endif

#define STM32MP_BL2_BASE		(STM32MP_SEC_SYSRAM_BASE + \
					 STM32MP_SEC_SYSRAM_SIZE - \
					 STM32MP_BL2_SIZE)

#define STM32MP_BL2_RO_BASE		STM32MP_BL2_BASE

#define STM32MP_BL2_RW_BASE		(STM32MP_BL2_RO_BASE + \
					 STM32MP_BL2_RO_SIZE)

#define STM32MP_BL2_RW_SIZE		STM32MP_SYSRAM_BASE + \
					STM32MP_SYSRAM_SIZE - \
					STM32MP_BL2_RW_BASE

#define STM32MP_MBEDTLS_HEAP_SIZE	U(0x2000)
#define STM32MP_MBEDTLS_HEAP_BASE	(SRAM1_BASE + SRAM1_SIZE_FOR_TFA - \
					 STM32MP_MBEDTLS_HEAP_SIZE)

/* BL2 and BL32/sp_min require 4 tables */
#define MAX_XLAT_TABLES			U(4)	/* 16 KB for mapping */

/*
 * MAX_MMAP_REGIONS is usually:
 * BL stm32mp2_mmap size + mmap regions in *_plat_arch_setup
 */
#if STM32MP_USB_PROGRAMMER || defined(IMAGE_BL31)
#define MAX_MMAP_REGIONS		7
#else
#define MAX_MMAP_REGIONS		6
#endif

/* DTB initialization value */
#define STM32MP_BL2_DTB_SIZE		U(0x00006000)	/* 24 KB for DTB */

#define STM32MP_BL2_DTB_BASE		(STM32MP_BL2_BASE - \
					 STM32MP_BL2_DTB_SIZE)

#if defined(IMAGE_BL2)
#define STM32MP_DTB_SIZE		STM32MP_BL2_DTB_SIZE
#define STM32MP_DTB_BASE		STM32MP_BL2_DTB_BASE
#endif

#if STM32MP_DDR_FIP_IO_STORAGE
#define STM32MP_DDR_FW_BASE		(SRAM1_BASE + STM32MP_SEC_SRAM1_SIZE)
#define STM32MP_DDR_FW_DMEM_OFFSET	U(0x400)
#define STM32MP_DDR_FW_IMEM_OFFSET	U(0x800)
#define STM32MP_DDR_FW_MAX_SIZE		U(0x8800)
#endif

#define STM32MP_FW_CONFIG_MAX_SIZE	PAGE_SIZE
#define STM32MP_FW_CONFIG_BASE		STM32MP_SYSRAM_BASE

#define STM32MP_BL33_BASE		(STM32MP_DDR_BASE + U(0x04000000))
#define STM32MP_BL33_MAX_SIZE		U(0x400000)
#define STM32MP_HW_CONFIG_BASE		(STM32MP_BL33_BASE + \
					STM32MP_BL33_MAX_SIZE)
#define STM32MP_HW_CONFIG_MAX_SIZE	U(0x40000)
#define STM32MP_SOC_FW_CONFIG_MAX_SIZE	U(0x10000) /* 64kB for BL31 DT */

/* Define maximum page size for NAND devices */
#define PLATFORM_MTD_MAX_PAGE_SIZE	U(0x1000)

/*******************************************************************************
 * STM32MP2 device/io map related constants (used for MMU)
 ******************************************************************************/
#define STM32MP_DEVICE_BASE		U(0x40000000)
#define STM32MP_DEVICE_SIZE		U(0x40000000)

/*******************************************************************************
 * STM32MP2 IAC
 ******************************************************************************/
#define IAC_BASE			U(0x42090000)

#define IAC_NB				U(6)
#define IAC_IER0			U(0x00)
#define IAC_ISR0			U(0x80)
#define IAC_ICR0			U(0x100)

#define IAC_IER(n)			(IAC_IER0 + (U(4) * (n)))
#define IAC_ISR(n)			(IAC_ISR0 + (U(4) * (n)))
#define IAC_ICR(n)			(IAC_ICR0 + (U(4) * (n)))

/*******************************************************************************
 * STM32MP2 RCC
 ******************************************************************************/
#define RCC_BASE			U(0x44200000)

/*******************************************************************************
 * STM32MP2 PWR
 ******************************************************************************/
#define PWR_BASE			U(0x44210000)

/*******************************************************************************
 * STM32MP2 EXTI1
 ******************************************************************************/
#define STM32MP_EXTI1_BASE		U(0x44220000)

/* EXTI1 CPU1 wake-up with interrupt mask register */
#define EXTI1_C1IMR1			U(0x80)
#define EXTI1_C1IMR2			U(0x90)
#define EXTI1_C1IMR3			U(0xA0)

#define EXTI1_C1IMR1_PVD		BIT(16)
#define EXTI1_C1IMR1_PVM		BIT(17)

#define EXTI1_C1IMR2_WKUP1		BIT(52 - 32)
#define EXTI1_C1IMR2_WKUP2		BIT(53 - 32)
#define EXTI1_C1IMR2_WKUP3		BIT(54 - 32)
#define EXTI1_C1IMR2_WKUP4		BIT(55 - 32)
#define EXTI1_C1IMR2_WKUP5		BIT(56 - 32)
#define EXTI1_C1IMR2_WKUP6		BIT(57 - 32)

#define EXTI1_C1IMR2_WKUP_MASK		GENMASK_32(57 - 32, 52 - 32)

/*******************************************************************************
 * STM32MP2 EXTI2
 ******************************************************************************/
#define STM32MP_EXTI2_BASE		U(0x46230000)

#define EXTI2_C1IMR1			U(0x80)
#define EXTI2_C1IMR2			U(0x90)
#define EXTI2_C1IMR3			U(0xA0)
#define EXTI2_C2IMR1			U(0xC0)
#define EXTI2_C2IMR2			U(0xD0)
#define EXTI2_C2IMR3			U(0xE0)
#define EXTI2_C3IMR1			U(0x100)
#define EXTI2_C3IMR2			U(0x110)
#define EXTI2_C3IMR3			U(0x120)

/*******************************************************************************
 * STM32MP2 GPIO
 ******************************************************************************/
#define GPIOA_BASE			U(0x44240000)
#define GPIOB_BASE			U(0x44250000)
#define GPIOC_BASE			U(0x44260000)
#define GPIOD_BASE			U(0x44270000)
#define GPIOE_BASE			U(0x44280000)
#define GPIOF_BASE			U(0x44290000)
#define GPIOG_BASE			U(0x442A0000)
#define GPIOH_BASE			U(0x442B0000)
#define GPIOI_BASE			U(0x442C0000)
#define GPIOJ_BASE			U(0x442D0000)
#define GPIOK_BASE			U(0x442E0000)
#define GPIOZ_BASE			U(0x46200000)
#define GPIO_BANK_OFFSET		U(0x10000)

#define STM32MP_GPIOS_PIN_MAX_COUNT	16
#define STM32MP_GPIOZ_PIN_MAX_COUNT	8

/*******************************************************************************
 * STM32MP2 UART
 ******************************************************************************/
#define USART1_BASE			U(0x40330000)
#define USART2_BASE			U(0x400E0000)
#define USART3_BASE			U(0x400F0000)
#define UART4_BASE			U(0x40100000)
#define UART5_BASE			U(0x40110000)
#define USART6_BASE			U(0x40220000)
#define UART7_BASE			U(0x40370000)
#define UART8_BASE			U(0x40380000)
#define UART9_BASE			U(0x402C0000)
#define STM32MP_NB_OF_UART		U(9)

/* For UART crash console */
#define STM32MP_DEBUG_USART_CLK_FRQ	64000000
/* USART2 on HSI@64MHz, TX on GPIOA4 Alternate 6 */
#define STM32MP_DEBUG_USART_BASE	USART2_BASE
#define DEBUG_UART_TX_GPIO_BANK_ADDRESS	GPIOA_BASE
#define DEBUG_UART_TX_GPIO_BANK_CLK_REG	RCC_GPIOACFGR
#define DEBUG_UART_TX_GPIO_BANK_CLK_EN	RCC_GPIOxCFGR_GPIOxEN
#define DEBUG_UART_TX_GPIO_PORT		4
#define DEBUG_UART_TX_GPIO_ALTERNATE	6
#define DEBUG_UART_TX_CLKSRC_REG	RCC_XBAR8CFGR
#define DEBUG_UART_TX_CLKSRC		XBAR_SRC_HSI
#define DEBUG_UART_TX_EN_REG		RCC_USART2CFGR
#define DEBUG_UART_TX_EN		RCC_UARTxCFGR_UARTxEN
#define DEBUG_UART_RST_REG		RCC_USART2CFGR
#define DEBUG_UART_RST_BIT		RCC_UARTxCFGR_UARTxRST
#define DEBUG_UART_PREDIV_CFGR		RCC_PREDIV8CFGR
#define DEBUG_UART_FINDIV_CFGR		RCC_FINDIV8CFGR

/*******************************************************************************
 * STM32MP2 SDMMC
 ******************************************************************************/
#define STM32MP_SDMMC1_BASE		U(0x48220000)
#define STM32MP_SDMMC2_BASE		U(0x48230000)
#define STM32MP_SDMMC3_BASE		U(0x48240000)

/*******************************************************************************
 * STM32MP2 OSPI
 ******************************************************************************/
/* OSPI 1 & 2 memory map area */
#define STM32MP_OSPI_MM_BASE		U(0x60000000)
#define STM32MP_OSPI_MM_SIZE		U(0x10000000)

/*******************************************************************************
 * STM32MP2 BSEC / OTP
 ******************************************************************************/
/*
 * 367 available OTPs, the other are masked
 * - ECIES key: 368 to 375 (only readable by bootrom)
 * - HWKEY: 376 to 383 (never reloadable or readable)
 */
#define STM32MP2_OTP_MAX_ID		0x16FU
#define STM32MP2_MID_OTP_START		0x80U
#define STM32MP2_UPPER_OTP_START	0x100U

/* OTP labels */
#define PART_NUMBER_OTP			"part_number_otp"
#define REVISION_OTP			"rev_otp"
#define PACKAGE_OTP			"package_otp"
#define HCONF1_OTP			"otp124"
#define NAND_OTP			"otp16"
#define NAND2_OTP			"otp20"
#define BOARD_ID_OTP			"board_id"
#define UID_OTP				"uid_otp"
#define LIFECYCLE2_OTP			"otp18"
#define PKH_OTP				"otp144"
#define ENCKEY_OTP			"otp260"

/* OTP mask */
/* PACKAGE */
#define PACKAGE_OTP_PKG_MASK		GENMASK_32(2, 0)
#define PACKAGE_OTP_PKG_SHIFT		0

/* IWDG OTP */
#define HCONF1_OTP_IWDG_HW_MASK(i)		BIT((i) * 3U)
#define HCONF1_OTP_IWDG_FZ_STOP_MASK(i)		BIT((i) * 3U + 1U)
#define HCONF1_OTP_IWDG_FZ_STANDBY_MASK(i)	BIT((i) * 3U + 2U)

/* NAND OTP */
/* NAND parameter storage flag */
#define NAND_PARAM_STORED_IN_OTP	BIT(31)

/* NAND page size in bytes */
#define NAND_PAGE_SIZE_MASK		GENMASK_32(30, 29)
#define NAND_PAGE_SIZE_SHIFT		29
#define NAND_PAGE_SIZE_2K		U(0)
#define NAND_PAGE_SIZE_4K		U(1)
#define NAND_PAGE_SIZE_8K		U(2)

/* NAND block size in pages */
#define NAND_BLOCK_SIZE_MASK		GENMASK_32(28, 27)
#define NAND_BLOCK_SIZE_SHIFT		27
#define NAND_BLOCK_SIZE_64_PAGES	U(0)
#define NAND_BLOCK_SIZE_128_PAGES	U(1)
#define NAND_BLOCK_SIZE_256_PAGES	U(2)

/* NAND number of block (in unit of 256 blocks) */
#define NAND_BLOCK_NB_MASK		GENMASK_32(26, 19)
#define NAND_BLOCK_NB_SHIFT		19
#define NAND_BLOCK_NB_UNIT		U(256)

/* NAND bus width in bits */
#define NAND_WIDTH_MASK			BIT(18)
#define NAND_WIDTH_SHIFT		18

/* NAND number of ECC bits per 512 bytes */
#define NAND_ECC_BIT_NB_MASK		GENMASK_32(17, 15)
#define NAND_ECC_BIT_NB_SHIFT		15
#define NAND_ECC_BIT_NB_UNSET		U(0)
#define NAND_ECC_BIT_NB_1_BITS		U(1)
#define NAND_ECC_BIT_NB_4_BITS		U(2)
#define NAND_ECC_BIT_NB_8_BITS		U(3)
#define NAND_ECC_ON_DIE			U(4)

/* NAND number of planes */
#define NAND_PLANE_BIT_NB_MASK		BIT(14)

/* NAND2 OTP */
#define NAND2_PAGE_SIZE_SHIFT		16

/* NAND2 config distribution */
#define NAND2_CONFIG_DISTRIB		BIT(0)
#define NAND2_PNAND_NAND2_SNAND_NAND1	U(0)
#define NAND2_PNAND_NAND1_SNAND_NAND2	U(1)

/* MONOTONIC OTP */
#define MAX_MONOTONIC_VALUE		32

/* UID OTP */
#define UID_WORD_NB			U(3)

/* Lifecycle OTP */
#define SECURE_BOOT_CLOSED_SECURE	GENMASK_32(3, 0)

/*******************************************************************************
 * STM32MP2 TAMP
 ******************************************************************************/
#define PLAT_MAX_TAMP_INT		U(5)
#define PLAT_MAX_TAMP_EXT		U(3)
#define TAMP_BASE			U(0x46010000)
#define TAMP_CR2			U(0x4)
#define TAMP_CR2_MASK_POM		GENMASK_32(7, 0)
#define TAMP_CR3			U(0x8)
#define TAMP_CR3_MASK_POM		GENMASK_32(14, 0)
#define TAMP_SMCR			U(0x20)
#define TAMP_SR				U(0x30)
#define TAMP_SR_INT_SHIFT		U(16)
#define TAMP_BKP_REG_CLK		CK_BUS_RTC
#define TAMP_BKP_SEC_NUMBER		U(10)
#define TAMP_SCR			U(0x3C)
#define TAMP_COUNTR			U(0x40)

#define TAMP_SR_LSE_MONITORING		BIT(18)

/*******************************************************************************
 * STM32MP2 USB
 ******************************************************************************/
#define USB_DWC3_BASE			U(0x48300000)

/*******************************************************************************
 * STM32MP2 DDRCTRL
 ******************************************************************************/
#define DDRCTRL_BASE			U(0x48040000)

/*******************************************************************************
 * STM32MP2 DDRDBG
 ******************************************************************************/
#define DDRDBG_BASE			U(0x48050000)

/*******************************************************************************
 * STM32MP2 DDRPHYC
 ******************************************************************************/
#define DDRPHYC_BASE			U(0x48C00000)

/*******************************************************************************
 * STM32MP2 IWDG
 ******************************************************************************/
#define IWDG_MAX_INSTANCE		U(2)
#define IWDG1_INST			U(0)
#define IWDG2_INST			U(1)

#define IWDG1_BASE			U(0x44010000)
#define IWDG2_BASE			U(0x44020000)

/*******************************************************************************
 * Miscellaneous STM32MP2 peripherals base address
 ******************************************************************************/
#define BSEC_BASE			U(0x44000000)
#define DBGMCU_BASE			U(0x4A010000)
#define HASH_BASE			U(0x42010000)
#define RAMCFG_BASE			U(0x42070000)
#define RTC_BASE			U(0x46000000)
#define STGEN_BASE			U(0x48080000)
#define SYSCFG_BASE			U(0x44230000)

/*******************************************************************************
 * STM32MP RIF
 ******************************************************************************/
#define RIFSC_BASE			U(0x42080000)
#define RISAB1_BASE			U(0x420F0000)
#define RISAB2_BASE			U(0x42100000)
#define RISAB3_BASE			U(0x42110000)
#define RISAB4_BASE			U(0x42120000)
#define RISAB5_BASE			U(0x42130000)

#define RISAF1_INST			0
#define RISAF2_INST			1
#define RISAF4_INST			3
#define RISAF5_INST			4
#define RISAF_MAX_INSTANCE		5

#define RISAF2_BASE			U(0x420B0000)
#define RISAF4_BASE			U(0x420D0000)

#ifdef RISAF1_BASE
#define RISAF1_MAX_REGION		4
#else
#define RISAF1_MAX_REGION		0
#endif
#ifdef RISAF2_BASE
#define RISAF2_MAX_REGION		4
#else
#define RISAF2_MAX_REGION		0
#endif
#ifdef RISAF4_BASE
/* Consider only encrypted region maximum number, to save memory consumption */
#define RISAF4_MAX_REGION		4
#else
#define RISAF4_MAX_REGION		0
#endif
#ifdef RISAF5_BASE
#define RISAF5_MAX_REGION		2
#else
#define RISAF5_MAX_REGION		0
#endif
#define RISAF_MAX_REGION		(RISAF1_MAX_REGION + RISAF2_MAX_REGION + \
					RISAF4_MAX_REGION + RISAF5_MAX_REGION)

#define RISAF_KEY_SIZE_IN_BYTES		U(16)
#define RISAF_SEED_SIZE_IN_BYTES	U(4)

/*******************************************************************************
 * RIFSC IDs
 ******************************************************************************/
#define STM32MP25_RIFSC_USB3DR_ID	U(66)
#define STM32MP25_RIFSC_RNG_ID		U(92)
#define STM32MP25_RIFSC_PKA_ID		U(93)
#define STM32MP25_RIFSC_SAES_ID		U(94)

#define STM32MP25_RIMU_USB3DR		U(4)

#define RIFSC_USB3DR_PRIV		BIT(STM32MP25_RIFSC_USB3DR_ID % U(32))
#define RIFSC_USB3DR_SEC		BIT(STM32MP25_RIFSC_USB3DR_ID % U(32))

/*
 * USB3DR Secure/Priv Master (DMA) access
 */
#define RIFSC_USB_BOOT_USBDR_RIMC_CONF	(RIFSC_RIMC_ATTRx_MPRIV | RIFSC_RIMC_ATTRx_MSEC | \
					 RIF_CID1 << RIFSC_RIMC_ATTRx_MCID_SHIFT | \
					 RIFSC_RIMC_ATTRx_CIDSEL)

/*******************************************************************************
 * STM32MP CA35SSC
 ******************************************************************************/
#define A35SSC_BASE			U(0x48800000)

/*******************************************************************************
 * REGULATORS
 ******************************************************************************/
/* 3 PWR + 1 VREFBUF + 14 PMIC regulators + 1 FIXED */
#define PLAT_NB_RDEVS			U(19)
/* 2 FIXED */
#define PLAT_NB_FIXED_REGUS		U(2)
/* No GPIO regu */
#define PLAT_NB_GPIO_REGUS		U(0)
/* Number of low power modes defined in the device tree */
#define PLAT_NB_SUSPEND_MODES		7
/* TAMP BACKUP REGISTER */
#define PLAT_NB_NVMEM_DEVS		U(1)
#define MAX_TAMP_BACKUP_REGS_ZONES	U(7)

/*******************************************************************************
 * Device Tree defines
 ******************************************************************************/
#define DT_BSEC_COMPAT			"st,stm32mp25-bsec"
#define DT_DDR_COMPAT			"st,stm32mp2-ddr"
#define DT_IWDG_COMPAT			"st,stm32mp1-iwdg"
#define DT_MMIO_SRAM			"mmio-sram"
#define DT_PWR_COMPAT			"st,stm32mp25-pwr"
#define DT_RCC_CLK_COMPAT		"st,stm32mp25-rcc"
#define DT_SDMMC2_COMPAT		"st,stm32mp25-sdmmc2"
#define DT_TAMP_NVRAM_COMPAT		"st,stm32mp25-tamp-nvram"
#define DT_UART_COMPAT			"st,stm32h7-uart"

#endif /* STM32MP2_DEF_H */
