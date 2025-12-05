/*
 * Copyright (C) 2019-2022, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <common/debug.h>
#include <drivers/arm/gicv2.h>
#include <drivers/generic_delay_timer.h>
#include <drivers/st/stm32_iwdg.h>
#include <drivers/st/stm32mp_clkfunc.h>
#include <drivers/st/stm32mp_pmic.h>
#include <drivers/st/stm32mp1_ddr_helpers.h>
#include <dt-bindings/power/stm32mp1-power.h>
#include <errno.h>
#include <plat/common/platform.h>

#include <platform_def.h>
#include <stm32mp1_context.h>
#include <stm32mp1_critic_power.h>

#define STM32_PM_CPU_STANDBY_FLAG		BIT_32(31)
#define STM32_PM_CARE_OTHER_CPU_STATE_FLAG	BIT_32(30)
#define STM32_PM_SOC_MODE_MASK			GENMASK_32(7, 0)

static void cstop_critic_enter(uint32_t mode)
{
	/* Init generic timer that is needed for udelay used in ddr driver */
	generic_delay_timer_init();

	/* Switch to Software Self-Refresh mode */
	ddr_set_sr_mode(DDR_SSR_MODE);

	/*
	 * Set DDR in Self-refresh,.
	 * This is also the procedure awaited when switching off power supply.
	 */
	if (ddr_standby_sr_entry() != 0) {
		ERROR("Unable to put DDR in SR\n");
		if (mode != (uint32_t)STM32_PM_SHUTDOWN) {
			panic();
		}
	}
}

static void shutdown_critic_enter(void)
{
	if (dt_pmic_status() > 0) {
		if (!initialize_pmic_i2c()) {
			panic();
		}

		pmic_switch_off();
	}
}

/*
 * stm32_exit_cstop_critic - Exit from CSTOP mode reenable DDR
 */
void stm32_pwr_cstop_critic_exit(void)
{
#if STM32MP13
	bsec_write_scratch((uint32_t)0U);
#endif

#if defined(IMAGE_BL2)
	/* Init generic timer that is needed for udelay used in ddr driver */
	stm32mp_stgen_restore_rate();

	generic_delay_timer_init();
#endif

	if (ddr_sw_self_refresh_exit() != 0) {
		panic();
	}
}

enum cstop_state_id {
	STATE_NONE = 0,
	STATE_AUTO_CSTOP_ENTRY
};

static volatile enum cstop_state_id cstop_cpux_state[2];
static volatile enum cstop_state_id cstop_enter;

static inline unsigned int stm32_get_pos_other(void) {
	return plat_my_core_pos() == 1 ? 0 : 1;
}

/*
 * Use Peterson's algorithm to implement sort of
 * spinlock without MMU and CPU cache enabled.
 */
static volatile bool flag[2] = {false, false};
static volatile unsigned int turn;

static void stm32mp1_power_lock(void) {
	unsigned int pos = plat_my_core_pos();
	unsigned int other_pos = stm32_get_pos_other();

	flag[pos] = true;
	turn = other_pos;

	while (flag[other_pos] && turn == other_pos);
}

static void stm32mp1_power_unlock(void) {
	unsigned int pos = plat_my_core_pos();

	flag[pos] = false;
}

static void stm32_cpu_standby(void) {
	unsigned int pos = plat_my_core_pos();

	stm32mp1_power_lock();
	cstop_cpux_state[pos] = STATE_AUTO_CSTOP_ENTRY;
	stm32mp1_power_unlock();

	/*
	 * Enter standby state.
	 * Synchronize on memory accesses and instruction flow before the WFI
	 * instruction.
	 */
	do {
		dsb();
		isb();
		wfi();
	} while (!read_isr());

	stm32mp1_power_lock();

	cstop_cpux_state[pos] = STATE_NONE;

	stm32mp1_power_unlock();

	while (cstop_enter != STATE_NONE)
		wfe();
};

int stm32_pwr_down_wfi_load(bool is_cstop, uint32_t mode)
{
	uint32_t soc_mode = mode & STM32_PM_SOC_MODE_MASK;

	if (mode & STM32_PM_CPU_STANDBY_FLAG) {
		stm32_cpu_standby();
		return 0;
	}

	if (mode & STM32_PM_CARE_OTHER_CPU_STATE_FLAG) {
		unsigned int other_pos = stm32_get_pos_other();

		stm32mp1_power_lock();

		if (cstop_cpux_state[other_pos] != STATE_AUTO_CSTOP_ENTRY) {
			stm32mp1_power_unlock();
			return -EPERM;
		}

		cstop_enter = STATE_AUTO_CSTOP_ENTRY;
		stm32mp1_power_unlock();
	}

	if (soc_mode == (uint32_t)STM32_PM_CSTOP_ALLOW_LPLV_STOP2) {
#if STM32MP15
		ERROR("LPLV-Stop2 mode not supported\n");
		panic();
#endif

#if STM32MP13
		/* If mode is STOP 2, set the entry point */
		bsec_write_scratch((uint32_t)stm32_pwr_back_from_stop2);
#endif
	}

	if (soc_mode != (uint32_t)STM32_PM_CSLEEP_RUN) {
		dcsw_op_all(DC_OP_CISW);
	}

	if (is_cstop) {
		cstop_critic_enter(soc_mode);
	}

	if (soc_mode == (uint32_t)STM32_PM_SHUTDOWN) {
		shutdown_critic_enter();
	}

	/*
	 * Synchronize on memory accesses and instruction flow before the WFI
	 * instruction.
	 */
	dsb();
	isb();
	wfi();

	stm32_iwdg_refresh();

	if (is_cstop) {
		stm32_pwr_cstop_critic_exit();
	}

	if (mode & STM32_PM_CARE_OTHER_CPU_STATE_FLAG) {
		stm32mp1_power_lock();
		cstop_enter = STATE_NONE;
		stm32mp1_power_unlock();
		sev();
	}

	return 0;
}

#if STM32MP13
void stm32_pwr_call_optee_ep(void)
{
	void (*optee_ep)(void);

	optee_ep = (void (*)(void))stm32_pm_get_optee_ep();

	optee_ep();

	/* This shouldn't be reached */
	panic();
}
#endif
