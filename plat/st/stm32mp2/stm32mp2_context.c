/*
 * Copyright (c) 2023, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <common/tf_crc32.h>
#include <drivers/clk.h>
#include <lib/el3_runtime/context_mgmt.h>
#include <lib/el3_runtime/cpu_data.h>
#include <plat/common/platform.h>

#if defined(SPD_opteed)
#include "../../../services/spd/opteed/opteed_private.h"
#endif

#include "../../../lib/psci/psci_private.h"

#include <platform_def.h>
#include <stm32mp2_context.h>

#define BACKUP_CTX_ADDR		STM32MP_BACKUP_RAM_BASE
#define BACKUP_CTX_CLK		CK_BUS_BKPSRAM
#define ENC_KEY_SIZE_IN_BYTES	RISAF_KEY_SIZE_IN_BYTES

/* Size of data for caller, the PM functions */
#define DATA_SIZE		U(4)

/* Magic used to indicated valid = ' ' 'M' 'P' '2' */
#define CONTEXT_MAGIC			U(0x204D5032)

/* VERSION to check compatibility of pm backup data between bl2 and bl31 */
#define ST_CTX_VER_MAJOR		1U
#define ST_CTX_VER_MINOR		0U
#define ST_CTX_VERSION			(((uint16_t)ST_CTX_VER_MAJOR << 8U) | ST_CTX_VER_MINOR)
#define ST_CTX_VER_GET_MAJOR(version)	(((version) & GENMASK_32(15, 8)) >> 8U)
#define ST_CTX_VER_GET_MINOR(version)	((version) & GENMASK_32(7, 0))

struct backup_data_s {
	uint32_t magic;
#if STM32MP_CONTEXT_VERSION
	uint16_t version;
	uint16_t size;
	uint32_t crc_32;
#endif
	uint8_t enc_mkey[ENC_KEY_SIZE_IN_BYTES];
	psci_power_state_t standby_pwr_state;
	cpu_context_t saved_cpu_s_context[PLATFORM_CORE_COUNT];
	cpu_context_t saved_cpu_ns_context[PLATFORM_CORE_COUNT];
	suspend_mode_t psci_suspend_mode;
#if defined(SPD_opteed)
	uintptr_t optee_vector;
	optee_context_t opteed_sp_context[OPTEED_CORE_COUNT];
#endif
	uintptr_t fdt_bl31;
	uint8_t data[DATA_SIZE];
};

#if STM32MP_CONTEXT_VERSION
static uint32_t stm32mp_pm_crc_check(struct backup_data_s *data)
{
	uint32_t saved_crc = data->crc_32;
	uint32_t crc;

	data->crc_32 = 0U;
	crc =  tf_crc32(0U, (unsigned char *)data, data->size);
	data->crc_32 = saved_crc;

	return crc;
}
#endif

void stm32mp_pm_save_enc_mkey_in_context(uint8_t *data)
{
	struct backup_data_s *backup_data;

	backup_data = (struct backup_data_s *)BACKUP_CTX_ADDR;

	clk_enable(BACKUP_CTX_CLK);

	memcpy(backup_data->enc_mkey, data, ENC_KEY_SIZE_IN_BYTES);

	clk_disable(BACKUP_CTX_CLK);
}

void stm32mp_pm_get_enc_mkey_from_context(uint8_t *data)
{
	struct backup_data_s *backup_data;

	backup_data = (struct backup_data_s *)BACKUP_CTX_ADDR;

	clk_enable(BACKUP_CTX_CLK);

	memcpy(data, backup_data->enc_mkey, ENC_KEY_SIZE_IN_BYTES);

	clk_disable(BACKUP_CTX_CLK);
}

bool stm32_pm_context_is_valid(void)
{
	struct backup_data_s *backup_data;
	bool ret = true;
#if STM32MP_CONTEXT_VERSION
	uint32_t crc_32 = 0U;
#endif

	backup_data = (struct backup_data_s *)BACKUP_CTX_ADDR;

	clk_enable(BACKUP_CTX_CLK);

	if (backup_data->magic != CONTEXT_MAGIC) {
		ret = false;
	} else {
#if STM32MP_CONTEXT_VERSION
		if ((ST_CTX_VER_GET_MAJOR(backup_data->version) != ST_CTX_VER_MAJOR) ||
#if ST_CTX_VER_MINOR > 0
		    (ST_CTX_VER_GET_MINOR(backup_data->version) < ST_CTX_VER_MINOR) ||
#endif
		    (backup_data->size < sizeof(*backup_data))) {
			ret = false;
		} else {
			crc_32 = stm32mp_pm_crc_check(backup_data);
			if (crc_32 != backup_data->crc_32) {
				ret = false;
			}
		}

		if (!ret) {
			ERROR("pm context invalid (crc=%x vs %x, size=%u vs %lu, ver=%x vs %x)\n",
			      backup_data->crc_32, crc_32, backup_data->size, sizeof(*backup_data),
			      backup_data->version, ST_CTX_VERSION);
			backup_data->magic = 0U;
		}
#endif
	}

#if STM32MP_CONTEXT_VERSION
	VERBOSE("%s(crc=%x vs %x, size=%u vs %lu, ver=%x vs %x) = %d\n", __func__,
	        backup_data->crc_32, crc_32, backup_data->size, sizeof(*backup_data),
	        backup_data->version, ST_CTX_VERSION, ret);
#endif

	clk_disable(BACKUP_CTX_CLK);

	return ret;
}

void stm32_pm_context_save(const psci_power_state_t *state, const void *data, size_t size)
{
	void *cpu_context;
	struct backup_data_s *backup_data;
	void *fdt = NULL;

	if (fdt_get_address(&fdt) == 0) {
		ERROR("Cannot get BL31 DT address\n");
		panic();
	}

	clk_enable(BACKUP_CTX_CLK);
	backup_data = (struct backup_data_s *)BACKUP_CTX_ADDR;

	backup_data->magic = CONTEXT_MAGIC;

	/* Retrieve non-secure CPU context struct address */
	cpu_context = cm_get_context(NON_SECURE);

	/* Save context in Backup SRAM */
	memcpy(&backup_data->saved_cpu_ns_context[0], cpu_context,
	       sizeof(cpu_context_t) * PLATFORM_CORE_COUNT);

	/* Retrieve secure CPU context struct address */
	cpu_context = cm_get_context(SECURE);

	/* Save context in Backup SRAM */
	memcpy(&backup_data->saved_cpu_s_context[0], cpu_context,
	       sizeof(cpu_context_t) * PLATFORM_CORE_COUNT);

#if defined(SPD_opteed)
	backup_data->optee_vector = (uintptr_t)optee_vector_table;

	memcpy(&backup_data->opteed_sp_context[0], opteed_sp_context,
	       sizeof(optee_context_t) * OPTEED_CORE_COUNT);
#endif

	/* Save PSCI state in Backup SRAM */
	memcpy(&backup_data->standby_pwr_state, state, sizeof(psci_power_state_t));
	backup_data->psci_suspend_mode = psci_suspend_mode;

	backup_data->fdt_bl31 = (uintptr_t)fdt;

	/* Copy other data in Backup SRAM */
	assert(size <= (size_t)DATA_SIZE);
	if ((data != NULL) && (size != 0U)) {
		(void)memcpy(&backup_data->data, data, MIN(size, (size_t)DATA_SIZE));
	}

#if STM32MP_CONTEXT_VERSION
	backup_data->version = ST_CTX_VERSION;
	backup_data->size = (uint16_t)sizeof(*backup_data);
	backup_data->crc_32 = stm32mp_pm_crc_check(backup_data);

	VERBOSE("%s(crc=%x, size=%u, version=%x)\n", __func__,
	        backup_data->crc_32, backup_data->size, backup_data->version);
#endif

	clk_disable(BACKUP_CTX_CLK);
}

void stm32_pm_context_restore(void *data, size_t size)
{
	void *cpu_context;
	struct backup_data_s *backup_data;
	int ret;

	clk_enable(BACKUP_CTX_CLK);
	backup_data = (struct backup_data_s *)BACKUP_CTX_ADDR;

	/* Retrieve non-secure CPU context struct address */
	cpu_context = cm_get_context(NON_SECURE);

	/* Restore data from Backup SRAM */
	memcpy(cpu_context, backup_data->saved_cpu_ns_context,
	       sizeof(cpu_context_t) * PLATFORM_CORE_COUNT);

	/* Retrieve non-secure CPU context struct address */
	cpu_context = cm_get_context(SECURE);

	/* Restore data from Backup SRAM */
	memcpy(cpu_context, backup_data->saved_cpu_s_context,
	       sizeof(cpu_context_t) * PLATFORM_CORE_COUNT);

	psci_set_target_local_pwr_states(plat_my_core_pos(), PLAT_MAX_PWR_LVL,
					 &backup_data->standby_pwr_state);

	if (psci_set_suspend_mode(backup_data->psci_suspend_mode) != PSCI_E_SUCCESS) {
		panic();
	}

#if defined(SPD_opteed)
	optee_vector_table = (optee_vectors_t *)backup_data->optee_vector;

	memcpy(opteed_sp_context, backup_data->opteed_sp_context,
	       sizeof(optee_context_t) * OPTEED_CORE_COUNT);

	opteed_restore();
#endif

	ret = dt_open_and_check(backup_data->fdt_bl31);
	if (ret < 0) {
		ERROR("%s: failed to open DT (%d)\n", __func__, ret);
		panic();
	}

	if ((data != NULL) && (size != 0U)) {
		(void)memcpy(data, (const void *)&backup_data->data, MIN(size, (size_t)DATA_SIZE));
	}

	clk_disable(BACKUP_CTX_CLK);
}

void stm32_pm_context_clear(void)
{
	struct backup_data_s *backup_data;

	backup_data = (struct backup_data_s *)BACKUP_CTX_ADDR;

	clk_enable(BACKUP_CTX_CLK);

	backup_data->magic = 0U;
#if STM32MP_CONTEXT_VERSION
	backup_data->version = 0U;
	backup_data->size = 0U;
	backup_data->crc_32 = 0U;
#endif

	clk_disable(BACKUP_CTX_CLK);
}
