/*
 * Copyright (c) 2015-2022, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef STM32MP1_DEF_H
#define STM32MP1_DEF_H

#include <common/tbbr/tbbr_img_def.h>
#include <drivers/st/stm32mp1_rcc.h>
#include <dt-bindings/clock/stm32mp1-clks.h>
#include <dt-bindings/gpio/stm32-gpio.h>
#include <dt-bindings/reset/stm32mp1-resets.h>
#include <dt-bindings/soc/st,stm32-etzpc.h>
#include <lib/utils_def.h>
#include <lib/xlat_tables/xlat_tables_defs.h>

#ifndef __ASSEMBLER__
#include <drivers/st/bsec.h>
#include <drivers/st/stm32mp1_calib.h>
#include <drivers/st/stm32mp1_clk.h>
#include <drivers/st/stm32mp1_ddr_regs.h>
#include <drivers/st/stm32mp1_pwr.h>

#include <boot_api.h>
#include <stm32mp_common.h>
#include <stm32mp_dt.h>
#include <stm32mp_shres_helpers.h>
#include <stm32mp1_context.h>
#include <stm32mp1_dbgmcu.h>
#include <stm32mp1_private.h>
#include <stm32mp1_shared_resources.h>
#endif

#include "stm32mp1_fip_def.h"

/*******************************************************************************
 * CHIP ID
 ******************************************************************************/
#if STM32MP13
#define STM32MP1_CHIP_ID	U(0x501)

#define STM32MP135C_PART_NB	U(0x05010000)
#define STM32MP135A_PART_NB	U(0x05010001)
#define STM32MP133C_PART_NB	U(0x050100C0)
#define STM32MP133A_PART_NB	U(0x050100C1)
#define STM32MP131C_PART_NB	U(0x050106C8)
#define STM32MP131A_PART_NB	U(0x050106C9)
#define STM32MP135F_PART_NB	U(0x05010800)
#define STM32MP135D_PART_NB	U(0x05010801)
#define STM32MP133F_PART_NB	U(0x050108C0)
#define STM32MP133D_PART_NB	U(0x050108C1)
#define STM32MP131F_PART_NB	U(0x05010EC8)
#define STM32MP131D_PART_NB	U(0x05010EC9)
#endif
#if STM32MP15
#define STM32MP1_CHIP_ID	U(0x500)

#define STM32MP157C_PART_NB	U(0x05000000)
#define STM32MP157A_PART_NB	U(0x05000001)
#define STM32MP153C_PART_NB	U(0x05000024)
#define STM32MP153A_PART_NB	U(0x05000025)
#define STM32MP151C_PART_NB	U(0x0500002E)
#define STM32MP151A_PART_NB	U(0x0500002F)
#define STM32MP157F_PART_NB	U(0x05000080)
#define STM32MP157D_PART_NB	U(0x05000081)
#define STM32MP153F_PART_NB	U(0x050000A4)
#define STM32MP153D_PART_NB	U(0x050000A5)
#define STM32MP151F_PART_NB	U(0x050000AE)
#define STM32MP151D_PART_NB	U(0x050000AF)
#endif

#define STM32MP1_REV_B		U(0x2000)
#if STM32MP13
#define STM32MP1_REV_Y		U(0x1003)
#define STM32MP1_REV_Z		U(0x1001)
#endif
#if STM32MP15
#define STM32MP1_REV_Z		U(0x2001)
#endif

/*******************************************************************************
 * PACKAGE ID
 ******************************************************************************/
#if STM32MP15
#define PKG_AA_LFBGA448		U(4)
#define PKG_AB_LFBGA354		U(3)
#define PKG_AC_TFBGA361		U(2)
#define PKG_AD_TFBGA257		U(1)
#endif

/*******************************************************************************
 * STM32MP1 memory map related constants
 ******************************************************************************/
#define STM32MP_ROM_BASE		U(0x00000000)
#define STM32MP_ROM_SIZE		U(0x00020000)
#define STM32MP_ROM_SIZE_2MB_ALIGNED	U(0x00200000)

#if STM32MP13
#define STM32MP_SYSRAM_BASE		U(0x2FFE0000)
#define STM32MP_SYSRAM_SIZE		U(0x00020000)
#define SRAM1_BASE			U(0x30000000)
#define SRAM1_SIZE			U(0x00004000)
#define SRAM2_BASE			U(0x30004000)
#define SRAM2_SIZE			U(0x00002000)
#define SRAM3_BASE			U(0x30006000)
#define SRAM3_SIZE			U(0x00002000)
#define SRAMS_BASE			SRAM1_BASE
#define SRAMS_SIZE_2MB_ALIGNED		U(0x00200000)
#endif /* STM32MP13 */
#if STM32MP15
#define STM32MP_SYSRAM_BASE		U(0x2FFC0000)
#define STM32MP_SYSRAM_SIZE		U(0x00040000)
#endif /* STM32MP15 */

#define STM32MP_BACKUP_RAM_BASE		U(0x54000000)
#if STM32MP13
#define STM32MP_BACKUP_RAM_SIZE		U(0x00002000)	/* 8KB */
#endif /* STM32MP13 */
#if STM32MP15
#define STM32MP_BACKUP_RAM_SIZE		U(0x00001000)	/* 4KB */
#endif /* STM32MP15 */

#define STM32MP_NS_SYSRAM_SIZE		PAGE_SIZE
#define STM32MP_NS_SYSRAM_BASE		(STM32MP_SYSRAM_BASE + \
					 STM32MP_SYSRAM_SIZE - \
					 STM32MP_NS_SYSRAM_SIZE)

#define STM32MP_SCMI_NS_SHM_BASE	STM32MP_NS_SYSRAM_BASE
#define STM32MP_SCMI_NS_SHM_SIZE	STM32MP_NS_SYSRAM_SIZE

#define STM32MP_SEC_SYSRAM_BASE		STM32MP_SYSRAM_BASE
#define STM32MP_SEC_SYSRAM_SIZE		(STM32MP_SYSRAM_SIZE - \
					 STM32MP_NS_SYSRAM_SIZE)

/* DDR configuration */
#define STM32MP_DDR_BASE		U(0xC0000000)
#define STM32MP_DDR_MAX_SIZE		U(0x40000000)	/* Max 1GB */

/* DDR power initializations */
#ifndef __ASSEMBLER__
enum ddr_type {
	STM32MP_DDR3,
	STM32MP_LPDDR2,
	STM32MP_LPDDR3
};
#endif

/* Section used inside TF binaries */
#if STM32MP13
/* 512 Octets reserved for header */
#define STM32MP_HEADER_RESERVED_SIZE	U(0x200)

#define STM32MP_BINARY_BASE		STM32MP_SEC_SYSRAM_BASE

#define STM32MP_BINARY_SIZE		STM32MP_SEC_SYSRAM_SIZE
#endif
#if STM32MP15
#define STM32MP_PARAM_LOAD_SIZE		U(0x00002400)	/* 9 KB for param */
/* 256 Octets reserved for header */
#define STM32MP_HEADER_SIZE		U(0x00000100)
/* round_up(STM32MP_PARAM_LOAD_SIZE + STM32MP_HEADER_SIZE, PAGE_SIZE) */
#define STM32MP_HEADER_RESERVED_SIZE	U(0x3000)

#define STM32MP_BINARY_BASE		(STM32MP_SEC_SYSRAM_BASE +	\
					 STM32MP_PARAM_LOAD_SIZE +	\
					 STM32MP_HEADER_SIZE)

#define STM32MP_BINARY_SIZE		(STM32MP_SEC_SYSRAM_SIZE -	\
					 (STM32MP_PARAM_LOAD_SIZE +	\
					  STM32MP_HEADER_SIZE))
#endif

/* BL2 and BL32/sp_min require finer granularity tables */
#if defined(IMAGE_BL2)
#define MAX_XLAT_TABLES			U(2) /* 8 KB for mapping */
#endif

#if defined(IMAGE_BL32)
#define MAX_XLAT_TABLES			U(4) /* 16 KB for mapping */
#endif

/*
 * MAX_MMAP_REGIONS is usually:
 * BL stm32mp1_mmap size + mmap regions in *_plat_arch_setup
 */
#if defined(IMAGE_BL2)
 #if STM32MP_USB_PROGRAMMER
  #define MAX_MMAP_REGIONS		8
 #else
  #define MAX_MMAP_REGIONS		7
 #endif
#endif

#if STM32MP13
#define STM32MP_BL33_BASE		STM32MP_DDR_BASE
#endif
#if STM32MP15
#define STM32MP_BL33_BASE		(STM32MP_DDR_BASE + U(0x100000))
#endif
#define STM32MP_BL33_MAX_SIZE		U(0x400000)

/* Define Temporary Stack size use during low power mode */
#define STM32MP_INT_STACK_SIZE		0x200

/* Define maximum page size for NAND devices */
#define PLATFORM_MTD_MAX_PAGE_SIZE	U(0x1000)

/* Define location for the MTD scratch buffer */
#if STM32MP13
#define STM32MP_MTD_BUFFER		(SRAM1_BASE + \
					 SRAM1_SIZE - \
					 PLATFORM_MTD_MAX_PAGE_SIZE)
#endif
/*
 * Only used for MTD devices that need some backup blocks.
 * Must define a maximum size for a partition.
 */
#define PLATFORM_MTD_MAX_PART_SIZE	U(0x00400000)

/*******************************************************************************
 * STM32MP1 device/io map related constants (used for MMU)
 ******************************************************************************/
#define STM32MP1_DEVICE1_BASE		U(0x40000000)
#define STM32MP1_DEVICE1_SIZE		U(0x40000000)

#define STM32MP1_DEVICE2_BASE		U(0x80000000)
#define STM32MP1_DEVICE2_SIZE		U(0x40000000)

/*******************************************************************************
 * STM32MP1 RCC
 ******************************************************************************/
#define RCC_BASE			U(0x50000000)

/*******************************************************************************
 * STM32MP1 PWR
 ******************************************************************************/
#define PWR_BASE			U(0x50001000)

/*******************************************************************************
 * STM32MP1 EXTI
 ******************************************************************************/
#define EXTI_BASE			U(0x5000D000)
#define EXTI_TZENR1			U(0x14)
#define EXTI_RPR3			U(0x4C)
#define EXTI_FPR3			U(0x50)
#define EXTI_C1IMR1			U(0x80)
#define EXTI_C2IMR1			U(0xC0)
#define EXTI_C2IMR2			U(0xD0)
#define EXTI_C2IMR3			U(0xE0)
#define EXTI_TZENR1_TZEN18		BIT(18)
#define EXTI_IMR1_IM18			BIT(18)
#define EXTI_RPR3_RPIF65		BIT(1)
#define EXTI_FPR3_FPIF65		BIT(1)

/*******************************************************************************
 * STM32MP1 GPIO
 ******************************************************************************/
#define GPIOA_BASE			U(0x50002000)
#define GPIOB_BASE			U(0x50003000)
#define GPIOC_BASE			U(0x50004000)
#define GPIOD_BASE			U(0x50005000)
#define GPIOE_BASE			U(0x50006000)
#define GPIOF_BASE			U(0x50007000)
#define GPIOG_BASE			U(0x50008000)
#define GPIOH_BASE			U(0x50009000)
#define GPIOI_BASE			U(0x5000A000)
#if STM32MP15
#define GPIOJ_BASE			U(0x5000B000)
#define GPIOK_BASE			U(0x5000C000)
#define GPIOZ_BASE			U(0x54004000)
#endif
#define GPIO_BANK_OFFSET		U(0x1000)

#if STM32MP15
#define STM32MP_GPIOZ_PIN_MAX_COUNT	8
#endif

/*******************************************************************************
 * STM32MP1 UART
 ******************************************************************************/
#if STM32MP13
#define USART1_BASE			U(0x4C000000)
#define USART2_BASE			U(0x4C001000)
#endif
#if STM32MP15
#define USART1_BASE			U(0x5C000000)
#define USART2_BASE			U(0x4000E000)
#endif
#define USART3_BASE			U(0x4000F000)
#define UART4_BASE			U(0x40010000)
#define UART5_BASE			U(0x40011000)
#define USART6_BASE			U(0x44003000)
#define UART7_BASE			U(0x40018000)
#define UART8_BASE			U(0x40019000)

/* For UART crash console */
#define STM32MP_DEBUG_USART_BASE	UART4_BASE
#if STM32MP13
/* UART4 on HSI@64MHz, TX on GPIOF12 Alternate 8 (Disco board) */
#define STM32MP_DEBUG_USART_CLK_FRQ	64000000
#define DEBUG_UART_TX_GPIO_BANK_ADDRESS	GPIOD_BASE
#define DEBUG_UART_TX_GPIO_BANK_CLK_REG	RCC_MP_S_AHB4ENSETR
#define DEBUG_UART_TX_GPIO_BANK_CLK_EN	RCC_MP_S_AHB4ENSETR_GPIODEN
#define DEBUG_UART_TX_GPIO_PORT		6
#define DEBUG_UART_TX_GPIO_ALTERNATE	8
#define DEBUG_UART_TX_CLKSRC_REG	RCC_UART4CKSELR
#define DEBUG_UART_TX_CLKSRC		RCC_UART4CKSELR_HSI
#endif /* STM32MP13 */
#if STM32MP15
/* UART4 on HSI@64MHz, TX on GPIOG11 Alternate 6 */
#define STM32MP_DEBUG_USART_CLK_FRQ	64000000
#define DEBUG_UART_TX_GPIO_BANK_ADDRESS	GPIOG_BASE
#define DEBUG_UART_TX_GPIO_BANK_CLK_REG	RCC_MP_AHB4ENSETR
#define DEBUG_UART_TX_GPIO_BANK_CLK_EN	RCC_MP_AHB4ENSETR_GPIOGEN
#define DEBUG_UART_TX_GPIO_PORT		11
#define DEBUG_UART_TX_GPIO_ALTERNATE	6
#define DEBUG_UART_TX_CLKSRC_REG	RCC_UART24CKSELR
#define DEBUG_UART_TX_CLKSRC		RCC_UART24CKSELR_HSI
#endif /* STM32MP15 */
#define DEBUG_UART_TX_EN_REG		RCC_MP_APB1ENSETR
#define DEBUG_UART_TX_EN		RCC_MP_APB1ENSETR_UART4EN
#define DEBUG_UART_RST_REG		RCC_APB1RSTSETR
#define DEBUG_UART_RST_BIT		RCC_APB1RSTSETR_UART4RST

/*******************************************************************************
 * STM32MP1 ETZPC
 ******************************************************************************/
#define STM32MP1_ETZPC_BASE		U(0x5C007000)

/* ETZPC TZMA IDs */
#define STM32MP1_ETZPC_TZMA_ROM		U(0)
#define STM32MP1_ETZPC_TZMA_SYSRAM	U(1)

#define STM32MP1_ETZPC_TZMA_ALL_SECURE	GENMASK_32(9, 0)

/*******************************************************************************
 * STM32MP1 TZC (TZ400)
 ******************************************************************************/
#define STM32MP1_TZC_BASE		U(0x5C006000)
#define STM32MP1_TZC_MAX_REGIONS	U(8)

#if STM32MP13
#define STM32MP1_FILTER_BIT_ALL		TZC_400_REGION_ATTR_FILTER_BIT(0)
#endif
#if STM32MP15
#define STM32MP1_FILTER_BIT_ALL		(TZC_400_REGION_ATTR_FILTER_BIT(0) | \
					 TZC_400_REGION_ATTR_FILTER_BIT(1))
#endif

/*******************************************************************************
 * STM32MP1 SDMMC
 ******************************************************************************/
#define STM32MP_SDMMC1_BASE		U(0x58005000)
#define STM32MP_SDMMC2_BASE		U(0x58007000)
#define STM32MP_SDMMC3_BASE		U(0x48004000)

#define STM32MP_MMC_INIT_FREQ			U(400000)	/*400 KHz*/
#define STM32MP_SD_NORMAL_SPEED_MAX_FREQ	U(25000000)	/*25 MHz*/
#define STM32MP_SD_HIGH_SPEED_MAX_FREQ		U(50000000)	/*50 MHz*/
#define STM32MP_EMMC_NORMAL_SPEED_MAX_FREQ	U(26000000)	/*26 MHz*/
#define STM32MP_EMMC_HIGH_SPEED_MAX_FREQ	U(52000000)	/*52 MHz*/

/*******************************************************************************
 * STM32MP1 BSEC / OTP
 ******************************************************************************/
#define STM32MP1_OTP_MAX_ID		0x5FU
#define STM32MP1_UPPER_OTP_START	0x20U

#define OTP_MAX_SIZE			(STM32MP1_OTP_MAX_ID + 1U)

/* OTP labels */
#define CFG0_OTP			"cfg0_otp"
#define PART_NUMBER_OTP			"part_number_otp"
#if STM32MP15
#define PACKAGE_OTP			"package_otp"
#endif
#define HW2_OTP				"hw2_otp"
#if STM32MP13
#define NAND_OTP			"cfg9_otp"
#define NAND2_OTP			"cfg10_otp"
#endif
#if STM32MP15
#define NAND_OTP			"nand_otp"
#endif
#define MONOTONIC_OTP			"monotonic_otp"
#define UID_OTP				"uid_otp"
#define PKH_OTP				"pkh_otp"
#define ENCKEY_OTP			"enckey_otp"
#define BOARD_ID_OTP			"board_id"
#define CFG2_OTP			"cfg2_otp"
#define SSP_OTP				"ssp_otp"
#define CHIP_CERTIFICATE_OTP		"chip_otp"
#define RMA_OTP				"rma_otp"

/* OTP mask */
/* CFG0 */
#if STM32MP13
#define CFG0_OTP_MODE_MASK		GENMASK_32(9, 0)
#define CFG0_OTP_MODE_SHIFT		0
#define CFG0_OPEN_DEVICE		0x17U
#define CFG0_CLOSED_DEVICE		0x3FU
#define CFG0_CLOSED_DEVICE_NO_BOUNDARY_SCAN	0x17FU
#define CFG0_CLOSED_DEVICE_NO_JTAG	0x3FFU
#endif
#if STM32MP15
#define CFG0_CLOSED_DEVICE		BIT(6)
#endif

/* CFG2 */
#define OTP_CFG2_SEC_COUNTER_MASK	GENMASK_32(27, 20)
#define OTP_CFG2_SEC_COUNTER_SHIFT	U(20)
#define OTP_CFG2_ST_KEY_MASK		GENMASK_32(31, 28)
#define OTP_CFG2_ST_KEY_SHIFT		U(28)

/* SSP */
#define SSP_OTP_REQ			BIT(BOOT_API_OTP_SSP_REQ_BIT_POS)
#define SSP_OTP_SUCCESS			BIT(BOOT_API_OTP_SSP_SUCCESS_BIT_POS)
#define SSP_OTP_MASK			GENMASK_32(BOOT_API_OTP_SSP_SUCCESS_BIT_POS, \
						   BOOT_API_OTP_SSP_REQ_BIT_POS)
#define SSP_OTP_SECRET_BASE		U(59)
#define SSP_OTP_SECRET_END		U(95)

/* CHIP_CERT */
#define CHIP_CERTIFICATE_MAX_SIZE	U(0x40)

/* RMA */
#if STM32MP13
#define RMA_OTP_MASK			GENMASK_32(31, 0)
#endif
#if STM32MP15
#define RMA_OTP_MASK			GENMASK_32(29, 0)
#endif

/* PART NUMBER */
#if STM32MP13
#define PART_NUMBER_OTP_PART_MASK	GENMASK_32(14, 0)
#endif
#if STM32MP15
#define PART_NUMBER_OTP_PART_MASK	GENMASK_32(7, 0)
#endif
#define PART_NUMBER_OTP_PART_SHIFT	0

/* PACKAGE */
#if STM32MP15
#define PACKAGE_OTP_PKG_MASK		GENMASK_32(29, 27)
#define PACKAGE_OTP_PKG_SHIFT		27
#endif

/* IWDG OTP */
#define HW2_OTP_IWDG_HW_POS		U(3)
#define HW2_OTP_IWDG_FZ_STOP_POS	U(5)
#define HW2_OTP_IWDG_FZ_STANDBY_POS	U(7)

/* HW2 OTP */
#define HW2_OTP_PRODUCT_BELOW_2V5	BIT(13)

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

/* FWU configuration */
#define FWU_MAX_TRIAL_REBOOT		U(3)

/*******************************************************************************
 * STM32MP1 TAMP
 ******************************************************************************/
#define PLAT_MAX_TAMP_INT		U(6)
#define PLAT_MAX_TAMP_EXT		U(3)
#define TAMP_BASE			U(0x5C00A000)
#define TAMP_BKP_REGISTER_BASE		(TAMP_BASE + U(0x100))
#define TAMP_BKP_SEC_NUMBER		U(10)
#define TAMP_BKP_S_W_NS_R_NUMBER	U(5)
#define TAMP_COUNTR			U(0x40)

#if !(defined(__LINKER__) || defined(__ASSEMBLER__))
static inline uintptr_t tamp_bkpr(uint32_t idx)
{
	return TAMP_BKP_REGISTER_BASE + (idx << 2);
}
#endif

/*******************************************************************************
 * STM32MP1 USB
 ******************************************************************************/
#define USB_OTG_BASE			U(0x49000000)

/*******************************************************************************
 * STM32MP1 DDRCTRL
 ******************************************************************************/
#define DDRCTRL_BASE			U(0x5A003000)

/*******************************************************************************
 * STM32MP1 DDRPHYC
 ******************************************************************************/
#define DDRPHYC_BASE			U(0x5A004000)

/*******************************************************************************
 * STM32MP1 MCE
 ******************************************************************************/
#if STM32MP13
#define MCE_BASE			U(0x58001000)
#define MCE_KEY_SIZE_IN_BYTES		U(16)
#endif

/*******************************************************************************
 * STM32MP1 IWDG
 ******************************************************************************/
#define IWDG_MAX_INSTANCE		U(2)
#define IWDG1_INST			U(0)
#define IWDG2_INST			U(1)

#define IWDG1_BASE			U(0x5C003000)
#define IWDG2_BASE			U(0x5A002000)

/*******************************************************************************
 * Miscellaneous STM32MP1 peripherals base address
 ******************************************************************************/
#define BSEC_BASE			U(0x5C005000)
#if STM32MP13
#define CRYP_BASE			U(0x54002000)
#endif
#if STM32MP15
#define CRYP1_BASE			U(0x54001000)
#endif
#define DBGMCU_BASE			U(0x50081000)
#if STM32MP13
#define HASH_BASE			U(0x54003000)
#endif
#if STM32MP15
#define HASH1_BASE			U(0x54002000)
#endif
#if STM32MP13
#define I2C3_BASE			U(0x4C004000)
#define I2C4_BASE			U(0x4C005000)
#define I2C5_BASE			U(0x4C006000)
#endif
#if STM32MP15
#define I2C4_BASE			U(0x5C002000)
#define I2C6_BASE			U(0x5c009000)
#endif
#if STM32MP13
#define RNG_BASE			U(0x54004000)
#endif
#if STM32MP15
#define RNG1_BASE			U(0x54003000)
#endif
#define RTC_BASE			U(0x5c004000)
#if STM32MP13
#define SPI4_BASE			U(0x4C002000)
#define SPI5_BASE			U(0x4C003000)
#endif
#if STM32MP15
#define SPI6_BASE			U(0x5c001000)
#endif
#define STGEN_BASE			U(0x5c008000)
#define SYSCFG_BASE			U(0x50020000)

/*******************************************************************************
 * STM32MP1 TIMERS
 ******************************************************************************/
#define TIM12_BASE			U(0x40006000)
#define TIM15_BASE			U(0x44006000)
#define TIM_MAX_INSTANCE		U(2)

/*******************************************************************************
 * STM32MP13 SAES
 ******************************************************************************/
#define SAES_BASE			U(0x54005000)

/*******************************************************************************
 * STM32MP13 PKA
 ******************************************************************************/
#define PKA_BASE			U(0x54006000)

/*******************************************************************************
 * STM32MP1 OPP
 ******************************************************************************/
#define PLAT_OPP_ID1			U(1)
#define PLAT_OPP_ID2			U(2)
#define PLAT_MAX_OPP_NB			U(2)
#define PLAT_MAX_PLLCFG_NB		U(6)

/*******************************************************************************
 * REGULATORS
 ******************************************************************************/
/* 3 PWR + 1 VREFBUF + 14 PMIC regulators + 1 FIXED */
#define PLAT_NB_RDEVS			U(19)
/* 2 FIXED */
#define PLAT_NB_FIXED_REGUS		U(10)
/* No GPIO regu */
#define PLAT_NB_GPIO_REGUS		U(0)
/* Number of low power modes defined in the device tree */
#define PLAT_NB_SUSPEND_MODES		7

/*******************************************************************************
 * Device Tree defines
 ******************************************************************************/
#if STM32MP13
#define DT_BSEC_COMPAT			"st,stm32mp13-bsec"
#define DT_DDR_COMPAT			"st,stm32mp13-ddr"
#endif
#if STM32MP15
#define DT_BSEC_COMPAT			"st,stm32mp15-bsec"
#define DT_DDR_COMPAT			"st,stm32mp1-ddr"
#endif
#define DT_IWDG_COMPAT			"st,stm32mp1-iwdg"
#define DT_OPP_COMPAT			"operating-points-v2"
#define DT_PWR_COMPAT			"st,stm32mp1,pwr-reg"
#if STM32MP13
#define DT_RCC_CLK_COMPAT		"st,stm32mp13-rcc"
#define DT_RCC_SEC_CLK_COMPAT		"st,stm32mp13-rcc-secure"
#endif
#if STM32MP15
#define DT_RCC_CLK_COMPAT		"st,stm32mp1-rcc"
#define DT_RCC_SEC_CLK_COMPAT		"st,stm32mp1-rcc-secure"
#endif
#define DT_SDMMC2_COMPAT		"st,stm32-sdmmc2"
#define DT_UART_COMPAT			"st,stm32h7-uart"
#define DT_USBPHYC_COMPAT		"st,stm32mp1-usbphyc"

#endif /* STM32MP1_DEF_H */
