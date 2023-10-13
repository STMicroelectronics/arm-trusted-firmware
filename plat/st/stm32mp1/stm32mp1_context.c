/*
 * Copyright (c) 2017-2025, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <string.h>

#include <arch_helpers.h>
#include <common/debug.h>
#include <drivers/clk.h>
#include <drivers/st/stm32mp1_ddr_helpers.h>
#include <drivers/st/stm32mp1_ddr_regs.h>
#include <dt-bindings/clock/stm32mp1-clks.h>
#include <lib/utils.h>

#include <platform_def.h>
#include <stm32mp1_context.h>
#include <stm32mp1_critic_power.h>

#define TRAINING_AREA_SIZE		64U

/*
 * MAILBOX_MAGIC relates to struct backup_data_s as defined
 *
 * MAILBOX_MAGIC_V1:
 * Context provides magic, resume entry, zq0cr0 zdata and DDR training buffer.
 *
 * MAILBOX_MAGIC_V2:
 * Context provides V1 content and PLL1 dual OPP settings structure (86 bytes).
 *
 * MAILBOX_MAGIC_V3:
 * Context provides V2 content, low power entry point, BL2 code start, end and
 * BL2_END (102 bytes). And, only for STM32MP13, adds MCE master key (16 bytes).
 */
#define MAILBOX_MAGIC_V1		(0x0001U << 16U)
#define MAILBOX_MAGIC_V2		(0x0002U << 16U)
#define MAILBOX_MAGIC_V3		(0x0003U << 16U)
#define MAILBOX_MAGIC			(MAILBOX_MAGIC_V3 | \
					 TRAINING_AREA_SIZE)

#define MAGIC_ID(magic)			((magic) & GENMASK_32(31, 16))
#define MAGIC_AREA_SIZE(magic)		((magic) & GENMASK_32(15, 0))

#if (PLAT_MAX_OPP_NB != 2U) || (PLAT_MAX_PLLCFG_NB != 6U)
#error MAILBOX_MAGIC_V2/_V3 does not support expected PLL1 settings
#endif

/* pll_settings structure size definitions (reference to clock driver) */
#define PLL1_SETTINGS_SIZE		(((PLAT_MAX_OPP_NB * \
					  (PLAT_MAX_PLLCFG_NB + 3U)) + 1U) * \
					 sizeof(uint32_t))

struct backup_data_s {
	uint32_t magic;
	uint32_t core0_resume_hint;
	uint32_t zq0cr0_zdata;
	uint8_t ddr_training_backup[TRAINING_AREA_SIZE];
	uint8_t pll1_settings[PLL1_SETTINGS_SIZE];
	uint32_t low_power_ep;
	uint32_t bl2_code_base;
	uint32_t bl2_code_end;
	uint32_t bl2_end;
#if STM32MP13
	uint8_t mce_mkey[MCE_KEY_SIZE_IN_BYTES];
	struct stm32_mce_region_s mce_regions[MCE_IP_MAX_REGION_NB];
#endif
};

uint32_t stm32_pm_get_optee_ep(void)
{
	struct backup_data_s *backup_data;
	uint32_t ep;

#if STM32MP15
	clk_enable(BKPSRAM);
#endif

	/* Context & Data to be saved at the beginning of Backup SRAM */
	backup_data = (struct backup_data_s *)STM32MP_BACKUP_RAM_BASE;

	switch (MAGIC_ID(backup_data->magic)) {
	case MAILBOX_MAGIC_V1:
	case MAILBOX_MAGIC_V2:
	case MAILBOX_MAGIC_V3:
		if (MAGIC_AREA_SIZE(backup_data->magic) != TRAINING_AREA_SIZE) {
			panic();
		}
		break;
	default:
		ERROR("PM context: bad magic\n");
		panic();
	}

	ep = backup_data->core0_resume_hint;

#if STM32MP15
	clk_disable(BKPSRAM);
#endif

	return ep;
}

void stm32_clean_context(void)
{
	if (!stm32mp_bkpram_get_access()) {
		return;
	}

	clk_enable(BKPSRAM);

	zeromem((void *)STM32MP_BACKUP_RAM_BASE, sizeof(struct backup_data_s));

	clk_disable(BKPSRAM);
}

#if defined(IMAGE_BL2)
void stm32_context_save_bl2_param(void)
{
	struct backup_data_s *backup_data;

	if (!stm32mp_bkpram_get_access()) {
		return;
	}

	clk_enable(BKPSRAM);

	backup_data = (struct backup_data_s *)STM32MP_BACKUP_RAM_BASE;

	backup_data->low_power_ep = (uint32_t)&stm32_pwr_down_wfi_wrapper;
	backup_data->bl2_code_base = BL_CODE_BASE;
	backup_data->bl2_code_end = BL_CODE_END;
	backup_data->bl2_end = BL2_END;
	backup_data->magic = MAILBOX_MAGIC_V3;
	backup_data->zq0cr0_zdata = ddr_get_io_calibration_val();

	clk_disable(BKPSRAM);
}
#endif

uint32_t stm32_get_zdata_from_context(void)
{
	struct backup_data_s *backup_data;
	uint32_t zdata;

	clk_enable(BKPSRAM);

	backup_data = (struct backup_data_s *)STM32MP_BACKUP_RAM_BASE;

	zdata = (backup_data->zq0cr0_zdata >> DDRPHYC_ZQ0CRN_ZDATA_SHIFT) &
		DDRPHYC_ZQ0CRN_ZDATA_MASK;

	clk_disable(BKPSRAM);

	return zdata;
}

bool stm32_pm_context_is_valid(void)
{
	struct backup_data_s *backup_data;
	bool ret;

	clk_enable(BKPSRAM);

	backup_data = (struct backup_data_s *)STM32MP_BACKUP_RAM_BASE;

	switch (MAGIC_ID(backup_data->magic)) {
	case MAILBOX_MAGIC_V1:
	case MAILBOX_MAGIC_V2:
	case MAILBOX_MAGIC_V3:
		ret = true;
		break;
	default:
		ret = false;
		break;
	}

	clk_disable(BKPSRAM);

	return ret;
}

void stm32_restore_ddr_training_area(void)
{
	struct backup_data_s *backup_data;

	clk_enable(BKPSRAM);

	backup_data = (struct backup_data_s *)STM32MP_BACKUP_RAM_BASE;

	inv_dcache_range(STM32MP_DDR_BASE, TRAINING_AREA_SIZE);

	memcpy((uint32_t *)STM32MP_DDR_BASE,
	       &backup_data->ddr_training_backup,
	       TRAINING_AREA_SIZE);

	flush_dcache_range(STM32MP_DDR_BASE, TRAINING_AREA_SIZE);

	dsb();

	clk_disable(BKPSRAM);
}

#if STM32MP13
void stm32mp1_pm_save_mce_mkey_in_context(uint8_t *data)
{
	struct backup_data_s *backup_data;

	backup_data = (struct backup_data_s *)STM32MP_BACKUP_RAM_BASE;

	if (!stm32mp_bkpram_get_access()) {
		return;
	}

	clk_enable(BKPSRAM);

	memcpy(backup_data->mce_mkey, data, MCE_KEY_SIZE_IN_BYTES);

	clk_disable(BKPSRAM);
}

void stm32mp1_pm_get_mce_mkey_from_context(uint8_t *data)
{
	struct backup_data_s *backup_data;

	backup_data = (struct backup_data_s *)STM32MP_BACKUP_RAM_BASE;

	if (!stm32mp_bkpram_get_access()) {
		ERROR("DDR encryption key not available\n");
		panic();
	}

	clk_enable(BKPSRAM);

	memcpy(data, backup_data->mce_mkey, MCE_KEY_SIZE_IN_BYTES);

	clk_disable(BKPSRAM);
}

void stm32mp1_pm_save_mce_region(uint32_t index, struct stm32_mce_region_s *config)
{
	struct backup_data_s *backup_data;

	if (index >= MCE_IP_MAX_REGION_NB) {
		panic();
	}

	if (!stm32mp_bkpram_get_access()) {
		return;
	}

	backup_data = (struct backup_data_s *)STM32MP_BACKUP_RAM_BASE;

	clk_enable(BKPSRAM);

	memcpy(&backup_data->mce_regions[index], config, sizeof(struct stm32_mce_region_s));

	clk_disable(BKPSRAM);
}

void stm32mp1_pm_get_mce_region(uint32_t index, struct stm32_mce_region_s *config)
{
	struct backup_data_s *backup_data;

	if (index >= MCE_IP_MAX_REGION_NB) {
		panic();
	}

	backup_data = (struct backup_data_s *)STM32MP_BACKUP_RAM_BASE;

	if (!stm32mp_bkpram_get_access()) {
		ERROR("MCE region not available\n");
		panic();
	}

	clk_enable(BKPSRAM);

	memcpy(config, &backup_data->mce_regions[index], sizeof(struct stm32_mce_region_s));

	clk_disable(BKPSRAM);
}
#endif
