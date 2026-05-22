/*
 * Copyright (c) 2023-2026, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <errno.h>
#include <stdint.h>

#include <arch_helpers.h>
#include <drivers/st/stm32_rifsc.h>
#include <drivers/st/stm32mp_rifsc_regs.h>
#include <dt-bindings/soc/rif.h>
#include <lib/mmio.h>

#include <platform_def.h>

static unsigned long rifsc_periph[] = {
#if STM32MP21
	STM32MP21_RIFSC_CRYP1_ID,
#endif /* STM32MP21 */
#if STM32MP23
	STM32MP23_RIFSC_CRYP1_ID,
#endif /* STM32MP23 */
#if STM32MP25
	STM32MP25_RIFSC_CRYP1_ID,
#endif /* STM32MP25 */
};

static int stm32_rifsc_access_check(unsigned int id)
{
	unsigned int periph_offset = id % 32U;

	uint32_t sec_reg_value = mmio_read_32(RIFSC_BASE + _RIFSC_RISC_SECCFGR(id));
	uint32_t priv_reg_value = mmio_read_32(RIFSC_BASE + _RIFSC_RISC_PRIVCFGR(id));
	uint32_t cid_reg_value = mmio_read_32(RIFSC_BASE + _RIFSC_RISC_PERy_CIDCFGR(id));
	uint32_t sem_reg_value = mmio_read_32(RIFSC_BASE + _RIFSC_RISC_PERy_SEMCR(id));

	if ((cid_reg_value & _RIFSC_CIDCFGR_CFEN) == 0U) {
		return 0;
	}

	/*
	 * Peripheral used by TF-A are supposed to be secure privileged
	 */
	if (((sec_reg_value & BIT_32(periph_offset)) == 0U) &&
	    ((priv_reg_value & BIT_32(periph_offset)) == 0U)) {
		return -EACCES;
	}

	/*
	 * If semaphore is enabled CID1 must be whitelisted and
	 * semaphore taken by CID1 or free to take.
	 * Otherwise, static CID must be CID1.
	 */
	if ((cid_reg_value & _RIFSC_CIDCFGR_SEM_EN) != 0U) {

		/* Check if CID1 is whitelisted */
		if (((cid_reg_value & BIT(RIF_CID1 + _RIFSC_CIDCFGR_SEML_SHIFT)) == 0U)) {
			return -EACCES;
		}

		/*
		 * If the last successful acquisition of the semaphore
		 * is not done by CID1, then the semapohre must be free
		 * to take.
		 */
		if ((((sem_reg_value & _RIFSC_SEMCR_SEMCID_MASK) >>
		      _RIFSC_SEMCR_SEMCID_SHIFT) != RIF_CID1) &&
		    ((sem_reg_value & _RIFSC_SEMCR_SEM_MUTEX) != 0U)) {
				return -EACCES;
		}
	} else {
		if (((cid_reg_value & _RIFSC_CIDCFGR_SCID_MASK) >>
		     _RIFSC_CIDCFGR_SCID_SHIFT) != RIF_CID1) {
			return -EACCES;
		}
	}

	return 0;
}

int stm32_rifsc_check_peripheral_access(void)
{
	size_t i;
	int ret = 0;

	for (i = 0U; i < ARRAY_SIZE(rifsc_periph); i++) {
		if (stm32_rifsc_access_check(rifsc_periph[i]) != 0) {
			ERROR("%s: access denied. Periph ID: %lu\n",
			      __func__, rifsc_periph[i]);
			ret = -EACCES;
		}
	}

	return ret;
}

int stm32_rifsc_semaphore_init(void)
{
	size_t i;

	for (i = 0U; i < ARRAY_SIZE(rifsc_periph); i++) {
		uint32_t cidcfgr = mmio_read_32(RIFSC_BASE +
						_RIFSC_RISC_PERy_CIDCFGR(rifsc_periph[i]));
		uint32_t semcfgr = mmio_read_32(RIFSC_BASE +
						_RIFSC_RISC_PERy_SEMCR(rifsc_periph[i]));

		uint32_t sem_wl = (cidcfgr & _RIFSC_CIDCFGR_SEML_MASK) >> _RIFSC_CIDCFGR_SEML_SHIFT;

		if (!(((cidcfgr & _RIFSC_CIDCFGR_CFEN) != 0U) &&
		      ((cidcfgr & _RIFSC_CIDCFGR_SEM_EN) != 0U) &&
		      ((sem_wl & RIF_CID1_BF) == RIF_CID1_BF))) {
			continue;
		}

		if (((semcfgr & _RIFSC_SEMCR_SEM_MUTEX) != 0U) &&
		    ((semcfgr & _RIFSC_SEMCR_SEMCID_MASK) >> _RIFSC_SEMCR_SEMCID_SHIFT) != RIF_CID1) {
			return -EACCES;
		}

		mmio_write_32(RIFSC_BASE + _RIFSC_RISC_PERy_SEMCR(rifsc_periph[i]),
			      _RIFSC_SEMCR_SEM_MUTEX);

		if (((semcfgr & _RIFSC_SEMCR_SEM_MUTEX) != 0U) &&
		    ((semcfgr & _RIFSC_SEMCR_SEMCID_MASK) >> _RIFSC_SEMCR_SEMCID_SHIFT) != RIF_CID1) {
			return -EACCES;
		}
	}

	return 0;
}

int stm32_rifsc_semaphore_exit(void)
{
	size_t i;

	for (i = 0U; i < ARRAY_SIZE(rifsc_periph); i++) {
		uint32_t cidcfgr = mmio_read_32(RIFSC_BASE +
						_RIFSC_RISC_PERy_CIDCFGR(rifsc_periph[i]));
		uint32_t semcfgr = mmio_read_32(RIFSC_BASE +
						_RIFSC_RISC_PERy_SEMCR(rifsc_periph[i]));

		uint32_t sem_wl = (cidcfgr & _RIFSC_CIDCFGR_SEML_MASK) >> _RIFSC_CIDCFGR_SEML_SHIFT;

		if (!(((cidcfgr & _RIFSC_CIDCFGR_CFEN) != 0U) &&
		      ((cidcfgr & _RIFSC_CIDCFGR_SEM_EN) != 0U) &&
		      ((sem_wl & RIF_CID1_BF) == RIF_CID1_BF))) {
			continue;
		}

		if (((semcfgr & _RIFSC_SEMCR_SEMCID_MASK) >> _RIFSC_SEMCR_SEMCID_SHIFT) !=
		    RIF_CID1) {
			return -EACCES;
		}

		mmio_write_32(RIFSC_BASE + _RIFSC_RISC_PERy_SEMCR(rifsc_periph[i]), 0x0U);
	}

	return 0;
}

void stm32_rifsc_ip_configure(int rimu_id, int rifsc_id, uint32_t param)
{
	uint32_t bit;

#if STM32MP21
	assert(rifsc_id < STM32MP21_RIFSC_MAX_ID);
#endif /* STM32MP21 */
#if STM32MP23
	assert(rifsc_id < STM32MP23_RIFSC_MAX_ID);
#endif /* STM32MP23 */
#if STM32MP25
	assert(rifsc_id < STM32MP25_RIFSC_MAX_ID);
#endif /* STM32MP25 */

	bit = BIT_32(rifsc_id % U(32));

	/* Set peripheral accesses to Secure/Privilege only */
	mmio_setbits_32(RIFSC_BASE + _RIFSC_RISC_SECCFGR(rifsc_id), bit);
	mmio_setbits_32(RIFSC_BASE + _RIFSC_RISC_PRIVCFGR(rifsc_id), bit);

	/* Apply specific configuration to RIF master */
	mmio_write_32(RIFSC_BASE + _RIFSC_RIMC_ATTR(rimu_id), param);
}
