/*
 * Copyright (c) 2023, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <errno.h>

#include <arch_helpers.h>
#include <bl31/interrupt_mgmt.h>
#include <common/debug.h>
#include <common/fdt_wrappers.h>
#include <drivers/arm/gic_common.h>
#include <drivers/arm/gicv2.h>
#include <drivers/clk.h>
#include <drivers/generic_delay_timer.h>
#include <drivers/st/bsec3_reg.h>
#include <drivers/st/stm32mp_clkfunc.h>
#include <drivers/st/stm32mp_reset.h>
#include <drivers/st/stm32mp2_ddr_helpers.h>
#include <lib/mmio.h>
#include <lib/psci/psci.h>
#include <lib/spinlock.h>
#include <plat/common/platform.h>

#include <platform_def.h>
#include <stm32mp2_context.h>

/* Default value for STM32MP25 with STPMIC25, defined in AN5727 */
#define DEFAULT_POPL_D1		3U
#define DEFAULT_PODH_D2		1U
#define DEFAULT_POPL_D2		2U
#define DEFAULT_LPCFG_D2	1U		/* PWR_ON=0 for Standby1/2 = PMIC_PWRCTRL1 */
#define DEFAULT_LPLVDLY_D2	0U		/* 6xLSI cycle = 187 us */
#define DEFAULT_LPSTOP1DLY	100U		/* LP-Stop1 PWRLP_DLY to wait VTT */

#define CA35SS_SYSCFG_VBAR_CR	0x2084U

#define RAMCFG_RETRAMCR		0x180U
#define SRAMHWERDIS		BIT(12)

/* GIC interrupt number */
#define RCC_WAKEUP_IRQn		254

/* Value with 64 MHz HSI period */
#define PWRLPDLYCR_VAL(delay_us, lsmcu)	((64U * (delay_us)) / (1U + (lsmcu)))

#define STATE_START	U(0)
#define STATE_RUNNING	U(1)
#define STATE_DDR	U(2)
#define STATE_NB	U(3)

/*
 * Struct for synchronization between the cores, protected by stm32mp_state_lock
 * to protect simultaneous acces, 3 states:
 * - START: start is requested on the current core (hotplug/not in reset)
 * - RUNNING: currently running (not in WFI loop)
 * - DDR: the DDR is not in self refresh, usable after WFI loop exit
 *        used for handshake for standby exit / supend entry
 *        only configured for STM32MP_PRIMARY_CPU
 * The element of the array is cache-line aligned to allow cache maintenance
 */
static struct stm32_psci_percpu_data {
	bool state[STATE_NB];
} __aligned(CACHE_WRITEBACK_GRANULE) stm32_percpu_data[PLATFORM_CORE_COUNT];

static spinlock_t stm32mp_state_lock;

uintptr_t stm32_sec_entrypoint;

static u_register_t saved_scr_el3;

static uint32_t lpstop1_pwrlpdly;
static uint32_t stop2_pwrlpdly;

/* Support PSCI v1.0 Extended State-ID with the recommended encoding */
#define LVL_CORE		U(0)
#define LVL_D1			U(1)
#define LVL_D1_LPLV		U(2)
#define LVL_D2			U(3)
#define LVL_D2_LPLV		U(4)

#define stm32_make_pwrstate(lvl4, lvl3, lvl2, lvl1, lvl0, type) \
	(((STM32MP_LOCAL_STATE_ ## lvl4) << (PLAT_LOCAL_PSTATE_WIDTH * 4)) | \
	 ((STM32MP_LOCAL_STATE_ ## lvl3) << (PLAT_LOCAL_PSTATE_WIDTH * 3)) | \
	 ((STM32MP_LOCAL_STATE_ ## lvl2) << (PLAT_LOCAL_PSTATE_WIDTH * 2)) | \
	 ((STM32MP_LOCAL_STATE_ ## lvl1) << (PLAT_LOCAL_PSTATE_WIDTH * 1)) | \
	 ((STM32MP_LOCAL_STATE_ ## lvl0) << (PLAT_LOCAL_PSTATE_WIDTH * 0)) | \
	 ((type) << PSTATE_TYPE_SHIFT))

#define stm32_get_stateid_lvl(pwr_domain_state, lvl) \
	(pwr_domain_state[(lvl)] << (PLAT_LOCAL_PSTATE_WIDTH * (lvl)))

#define stm32_get_stateid(pwr_domain_state) \
	(stm32_get_stateid_lvl(pwr_domain_state, LVL_CORE) | \
	 stm32_get_stateid_lvl(pwr_domain_state, LVL_D1) | \
	 stm32_get_stateid_lvl(pwr_domain_state, LVL_D1_LPLV) | \
	 stm32_get_stateid_lvl(pwr_domain_state, LVL_D2) | \
	 stm32_get_stateid_lvl(pwr_domain_state, LVL_D2_LPLV) | \
	 ((pwr_domain_state[LVL_D1_LPLV] == STM32MP_LOCAL_STATE_OFF ? PSTATE_TYPE_POWERDOWN : 0) \
		<< PSTATE_TYPE_SHIFT))

/* State-id - 0x00000001 */
#define PWRSTATE_RUN \
	stm32_make_pwrstate(RUN, RUN, RUN, RUN, RET, PSTATE_TYPE_STANDBY)

/* State-id - 0x00000011 Stop1 */
#define PWRSTATE_STOP1 \
	stm32_make_pwrstate(RUN, RUN, RUN, RET, RET, PSTATE_TYPE_STANDBY)

/* State-id - 0x00000021 LP-Stop1*/
#define PWRSTATE_LP_STOP1 \
	stm32_make_pwrstate(RUN, RUN, RUN, LP, RET, PSTATE_TYPE_STANDBY)

/* State-id - 0x00000211 LPLV-Stop1*/
#define PWRSTATE_LPLV_STOP1 \
	stm32_make_pwrstate(RUN, RUN, LP, RET, RET, PSTATE_TYPE_STANDBY)

/* State-id - 0x40001333 Stop2 */
#define PWRSTATE_STOP2 \
	stm32_make_pwrstate(RUN, RET, OFF, OFF, OFF, PSTATE_TYPE_POWERDOWN)

/* State-id - 0x40002333 LP-Stop2*/
#define PWRSTATE_LP_STOP2 \
	stm32_make_pwrstate(RUN, LP, OFF, OFF, OFF, PSTATE_TYPE_POWERDOWN)

/* State-id - 0x40023333 LPLV-Stop2*/
#define PWRSTATE_LPLV_STOP2 \
	stm32_make_pwrstate(LP, OFF, OFF, OFF, OFF, PSTATE_TYPE_POWERDOWN)

/* State-id - 0x40033333 Standby */
#define PWRSTATE_STANDBY \
	stm32_make_pwrstate(OFF, OFF, OFF, OFF, OFF, PSTATE_TYPE_POWERDOWN)

/*
 *  The table storing the valid idle power states. Ensure that the
 *  array entries are populated in ascending order of state-id to
 *  enable us to use binary search during power state validation.
 *  The table must be terminated by a NULL entry.
 */
static const unsigned int stm32mp_pm_idle_states[] = {
	PWRSTATE_RUN,
	PWRSTATE_STOP1,
	PWRSTATE_LP_STOP1,
	PWRSTATE_LPLV_STOP1,
	PWRSTATE_STOP2,
	PWRSTATE_LP_STOP2,
	PWRSTATE_LPLV_STOP2,
	0U, /* sentinel */
};

#define PM_IDLE_STATES_SIZE ARRAY_SIZE(stm32mp_pm_idle_states)

/* The supported low power mode on the board, including STANDBY */
static unsigned int stm32mp_supported_pwr_states[PM_IDLE_STATES_SIZE + 1U];

extern void stm32_stop2_entrypoint(void);

static bool stm32mp_state_check(unsigned int core_id, unsigned int state_id)
{
	bool val;
	bool spin_lock_available = stm32mp_lock_available();

	if (spin_lock_available) {
		spin_lock(&stm32mp_state_lock);
	}
	val = stm32_percpu_data[core_id].state[state_id];
	if (spin_lock_available) {
		spin_unlock(&stm32mp_state_lock);
	}

	return val;
}

static void stm32mp_state_set(unsigned int core_id, unsigned int state_id, bool state)
{
	bool spin_lock_available = stm32mp_lock_available();

	if (spin_lock_available) {
		spin_lock(&stm32mp_state_lock);
	}
	stm32_percpu_data[core_id].state[state_id] = state;
	flush_dcache_range((uintptr_t)&stm32_percpu_data[core_id], sizeof(stm32_percpu_data[0]));
	if (spin_lock_available) {
		spin_unlock(&stm32mp_state_lock);
	}
}

/*******************************************************************************
 * STM32MP2 handler called when a CPU is about to enter standby.
 ******************************************************************************/
static void stm32_cpu_standby(plat_local_state_t cpu_state)
{
	u_register_t scr = read_scr_el3();
	u_register_t mpidr = read_mpidr();
	unsigned int core_id = MPIDR_AFFLVL0_VAL(mpidr);

	assert(cpu_state == STM32MP_LOCAL_STATE_RET);

	stm32mp_state_set(core_id, STATE_RUNNING, false);

	/* Enable the Non-secure interrupt to wake the CPU. */
	write_scr_el3(scr | SCR_IRQ_BIT | SCR_FIQ_BIT);
	isb();
	/*
	 * Enter standby state.
	 * dsb is good practice before using wfi to enter low power states.
	 */
	dsb();
	wfi();

	write_scr_el3(scr);

	/* Coordinate the cores to prevent DDR access in self refresh */
	stm32mp_state_set(core_id, STATE_RUNNING, true);

	/* Force wake-up of other core, waiting in WFI */
	plat_ic_set_interrupt_pending(RCC_WAKEUP_IRQn);

	/* Handshake to wait DDR */
	while (!stm32mp_state_check(STM32MP_PRIMARY_CPU, STATE_DDR)) {
		wfe();
	}

	/* DDR is available, the other core is running */
	plat_ic_clear_interrupt_pending(RCC_WAKEUP_IRQn);
}

/*******************************************************************************
 * STM32MP2 handler called when a power domain is about to be turned on. The
 * mpidr determines the CPU to be turned on.
 * Called by core 0 to activate core 1.
 ******************************************************************************/
static int stm32_pwr_domain_on(u_register_t mpidr)
{
	unsigned int core_id = MPIDR_AFFLVL0_VAL(mpidr);

	if (stm32mp_is_single_core()) {
		return PSCI_E_NOT_SUPPORTED;
	}

	stm32mp_state_set(core_id, STATE_START, true);

	if (core_id == STM32MP_PRIMARY_CPU) {
		/* Cortex-A35 core0 can't be turned OFF, emulate it with a WFE loop */
		VERBOSE("BL31: Releasing core0 from wait loop...\n");
		dsb();
		isb();
		sev();
	} else {
		/* Reset the secondary core */
		mmio_write_32(RCC_BASE + RCC_C1P1RSTCSETR, RCC_C1P1RSTCSETR_C1P1PORRST);
	}

	return PSCI_E_SUCCESS;
}

static void stm32_pwr_domain_off(const psci_power_state_t *target_state)
{
	u_register_t mpidr = read_mpidr();
	unsigned int core_id = MPIDR_AFFLVL0_VAL(mpidr);

	/* Prevent interrupts from spuriously waking up this cpu */
	stm32mp_gic_cpuif_disable();

	/* Force state to OFF to allow synchronization */
	stm32mp_state_set(core_id, STATE_START, false);
}

static void stm32mp2_setup_rcc_wakeup_irq(uintptr_t rcc_base)
{
	plat_ic_set_interrupt_type(RCC_WAKEUP_IRQn, INTR_TYPE_S_EL1);
	plat_ic_set_interrupt_priority(RCC_WAKEUP_IRQn, GIC_HIGHEST_SEC_PRIORITY);
}

static void stm32mp2_enable_rcc_wakeup_irq(uintptr_t rcc_base)
{
	plat_ic_set_spi_routing(RCC_WAKEUP_IRQn, INTR_ROUTING_MODE_ANY, 0x0U);
	/* Enable RCC Wake-up interrupt, */
	mmio_setbits_32(rcc_base + RCC_C1CIESETR, RCC_C1CIESETR_WKUPIE);
	plat_ic_enable_interrupt(RCC_WAKEUP_IRQn);
}

static void stm32mp2_disable_rcc_wakeup_irq(uintptr_t rcc_base)
{
	plat_ic_disable_interrupt(RCC_WAKEUP_IRQn);

	/* Clear wakeup flag */
	mmio_setbits_32(rcc_base + RCC_C1CIFCLRR, RCC_C1CIFCLRR_WKUPF);

	/* Disable RCC Wake-up interrupt */
	mmio_clrbits_32(rcc_base + RCC_C1CIESETR, RCC_C1CIESETR_WKUPIE);
}

/*
 * Core synchronization to protect DDR access for running cores
 * Handshake by using the state 'DDR' and 'RUNNING' to safely block
 * the exit of stm32_cpu_standby() for other core,
 * The 'DDR' flag is used only on STM32MP_PRIMARY_CPU core
 *  = true when DDR is accessble for any cores
 *  = false when 'core_id' try to set the DDR in self refresh mode
 * This function return true if other core is blocked in stm32_cpu_standby()
 */
static bool stm32_freeze_other_core(unsigned int core_id)
{
	bool result = true;
	unsigned int id;

	/* Forbid access on DDR in stm32_cpu_standby() */
	stm32mp_state_set(STM32MP_PRIMARY_CPU, STATE_DDR, false);

	/*
	 * Check if the other core is not started or running:
	 * no more in WFI loop in stm32_cpu_standby() after interuption
	 */
	for (id = 0U; id < PLATFORM_CORE_COUNT; id++) {
		if (id == core_id) {
			continue;
		}
		if (stm32mp_state_check(id, STATE_START) &&
		    stm32mp_state_check(id, STATE_RUNNING)) {
			result = false;
			break;
		}
	}
	if (!result) {
		/* Unblock the other core in stm32_cpu_standby() on error */
		stm32mp_state_set(STM32MP_PRIMARY_CPU, STATE_DDR, true);
		sev();
	}

	return result;
}

static int stm32_pwr_domain_validate_suspend(const psci_power_state_t *target_state)
{
	uintptr_t pwr_base = stm32mp_pwr_base();
	uint32_t stateid = stm32_get_stateid(target_state->pwr_domain_state);
	u_register_t mpidr = read_mpidr();
	unsigned int core_id = MPIDR_AFFLVL0_VAL(mpidr);

	/* If retention only at D1 level return as nothing is to be done */
	if (stateid == PWRSTATE_RUN) {
		return PSCI_E_SUCCESS;
	}

	/* If CPU2 is not in reset: low power mode is not supported by CPU1 */
	if ((mmio_read_32(pwr_base + PWR_CPU2D2SR) & PWR_CPU2D2SR_CSTATE_MASK) != 0U) {
		WARN("PSCI power domain supend request with Cortex M33 running.\n");
		return PSCI_E_INVALID_PARAMS;
	}

	if (!stm32_freeze_other_core(core_id)) {
		return PSCI_E_DENIED;
	}

	return PSCI_E_SUCCESS;
}

/* Display Stop2 or Standby1 modes, when console is available */
static void print_mode_info(const char *mode)
{
	INFO("Entering %s low power mode\n", mode);
}

/* Display Stop1 modes, at verbose level to avoid this trace with CPUIdle */
static void print_mode_verbose(const char *mode)
{
	VERBOSE("Entering %s low power mode\n", mode);
}

static void stm32_pwr_domain_suspend(const psci_power_state_t *target_state)
{
	uintptr_t pwr_base = stm32mp_pwr_base();
	uintptr_t rcc_base = stm32mp_rcc_base();
	bool standby = false;
	uint32_t stateid = stm32_get_stateid(target_state->pwr_domain_state);

	/* If retention only at D1 level return as nothing is to be done */
	if (stateid == PWRSTATE_RUN) {
		return;
	}

	/* Request STOP for both cores */
	mmio_write_32(rcc_base + RCC_C1SREQSETR, RCC_C1SREQSETR_STPREQ_MASK);

	/*
	 * No PWR_LP delay by default, because VTT_DRR is not stopped (for Stop1)
	 * or VTT ramp-up delay is masked by VDD CPU delay (for other modes except LP-Stop1).
	 * It is required only for LP-Stop1 mode for design with STPMIC25.
	 */
	mmio_write_32(rcc_base + RCC_PWRLPDLYCR, 0U);

	/* Switch to Software Self-Refresh mode */
	if (stateid == PWRSTATE_STANDBY) {
		standby = true;
	}

	dcsw_op_all(DCCISW);
	ddr_save_sr_mode();
	ddr_set_sr_mode(DDR_SSR_MODE);
	if (ddr_sr_entry(standby) != 0) {
		panic();
	}

	/* Disable DDRSHR to avoid STANDBY/STOP exit issue */
	mmio_clrbits_32(rcc_base + RCC_DDRITFCFGR, RCC_DDRITFCFGR_DDRSHR);

	/* Perform the PWR configuration for the requested mode */
	switch (stateid) {
	case PWRSTATE_STOP1:
		print_mode_verbose("Stop1");
		mmio_write_32(pwr_base + PWR_CPU1CR, 0U);
		mmio_write_32(pwr_base + PWR_CPU2CR, 0U);
		stm32mp2_enable_rcc_wakeup_irq(rcc_base);
		break;

	case PWRSTATE_LP_STOP1:
		print_mode_verbose("LP_Stop1");
		mmio_write_32(pwr_base + PWR_CPU1CR, PWR_CPU1CR_LPDS_D1);
		mmio_write_32(pwr_base + PWR_CPU2CR, PWR_CPU2CR_LPDS_D2);
		/* Wait VTT ramp-up delay for LP-Stop1 */
		mmio_write_32(rcc_base + RCC_PWRLPDLYCR, lpstop1_pwrlpdly);
		stm32mp2_enable_rcc_wakeup_irq(rcc_base);
		break;

	case PWRSTATE_LPLV_STOP1:
		print_mode_verbose("LPLV_Stop1");
		mmio_write_32(pwr_base + PWR_CPU1CR, PWR_CPU1CR_LPDS_D1 | PWR_CPU1CR_LVDS_D1);
		mmio_write_32(pwr_base + PWR_CPU2CR, PWR_CPU2CR_LPDS_D2 | PWR_CPU2CR_LVDS_D2);
		stm32mp2_enable_rcc_wakeup_irq(rcc_base);
		break;

	case PWRSTATE_STOP2:
		print_mode_info("Stop2");
		mmio_write_32(pwr_base + PWR_CPU1CR, PWR_CPU1CR_PDDS_D1);
		mmio_write_32(pwr_base + PWR_CPU2CR, 0U);
		mmio_write_32(rcc_base + RCC_PWRLPDLYCR, stop2_pwrlpdly);
		stm32mp_gic_cpuif_disable();
		stm32mp_gic_save();
		stm32mp2_pll1_disable();
		break;

	case PWRSTATE_LP_STOP2:
		print_mode_info("LP_Stop2");
		mmio_write_32(pwr_base + PWR_CPU1CR, PWR_CPU1CR_PDDS_D1);
		mmio_write_32(pwr_base + PWR_CPU2CR, PWR_CPU2CR_LPDS_D2);
		mmio_write_32(rcc_base + RCC_PWRLPDLYCR, stop2_pwrlpdly);
		stm32mp_gic_cpuif_disable();
		stm32mp_gic_save();
		stm32mp2_pll1_disable();
		break;

	case PWRSTATE_LPLV_STOP2:
		print_mode_info("LPLV_Stop2");
		mmio_write_32(pwr_base + PWR_CPU1CR, PWR_CPU1CR_PDDS_D1);
		mmio_write_32(pwr_base + PWR_CPU2CR, PWR_CPU2CR_LPDS_D2 | PWR_CPU2CR_LVDS_D2);
		mmio_write_32(rcc_base + RCC_PWRLPDLYCR, stop2_pwrlpdly);
		stm32mp_gic_cpuif_disable();
		stm32mp_gic_save();
		stm32mp2_pll1_disable();
		break;

	case PWRSTATE_STANDBY:
		print_mode_info("Standby1");
		mmio_write_32(pwr_base + PWR_CPU1CR, PWR_CPU1CR_PDDS_D1 | PWR_CPU1CR_PDDS_D2);
		mmio_write_32(pwr_base + PWR_CPU2CR, PWR_CPU2CR_PDDS_D2);

		stm32mp_gic_cpuif_disable();
		stm32mp2_pll1_disable();
		break;

	default:
		panic();
		break;
	}

	/* Clear previous status */
	mmio_setbits_32(pwr_base + PWR_CPU1CR, PWR_CPU1CR_CSSF);
	mmio_setbits_32(pwr_base + PWR_CPU2CR, PWR_CPU2CR_CSSF);
	mmio_setbits_32(pwr_base + PWR_CPU3CR, PWR_CPU3CR_CSSF);

	/* Enable the Non-secure interrupt to wake up the CPU with WFI for pending interrupt */
	saved_scr_el3 = read_scr_el3();
	write_scr_el3(saved_scr_el3 | SCR_IRQ_BIT | SCR_FIQ_BIT);
}

/*******************************************************************************
 * STM32MP2 handler called when a power domain has just been powered on after
 * being turned off earlier. The target_state encodes the low power state that
 * each level has woken up from.
 * Called by core 1 just after wake up.
 ******************************************************************************/
static void stm32_pwr_domain_on_finish(const psci_power_state_t *target_state)
{
	uintptr_t rcc_base = stm32mp_rcc_base();
	u_register_t mpidr = read_mpidr();
	unsigned int core_id = MPIDR_AFFLVL0_VAL(mpidr);

	stm32mp_state_set(core_id, STATE_RUNNING, true);
	if (core_id == STM32MP_PRIMARY_CPU) {
		dsb();
		isb();
	} else {
		/* Restore generic timer after reset */
		stm32mp_stgen_restore_rate();
	}

	stm32mp2_disable_rcc_wakeup_irq(rcc_base);

	stm32mp_gic_pcpu_init();
	stm32mp_gic_cpuif_enable();

	mmio_write_32(rcc_base + RCC_C1SREQCLRR, RCC_C1SREQCLRR_STPREQ_MASK);
}

/*******************************************************************************
 * STM32MP2 handler called when a power domain has just been powered on after
 * having been suspended earlier. The target_state encodes the low power state
 * that each level has woken up from.
 ******************************************************************************/
static void stm32_pwr_domain_suspend_finish(const psci_power_state_t
					    *target_state)
{
	uintptr_t rcc_base = stm32mp_rcc_base();
	u_register_t mpidr = read_mpidr();
	unsigned int core_id = MPIDR_AFFLVL0_VAL(mpidr);
	uint32_t stateid = stm32_get_stateid(target_state->pwr_domain_state);

	stm32mp_state_set(core_id, STATE_RUNNING, true);

	stm32mp2_disable_rcc_wakeup_irq(rcc_base);

	mmio_write_32(rcc_base + RCC_C1SREQCLRR, RCC_C1SREQCLRR_STPREQ_MASK);

	/* Restore DDRSHR after STANDBY/STOP exit issue */
	mmio_setbits_32(rcc_base + RCC_DDRITFCFGR, RCC_DDRITFCFGR_DDRSHR);

	/* Perform the common system specific operations */
	switch (stateid) {
	case PWRSTATE_STANDBY:
		VERBOSE("STANDBY exit\n");
		/* Restore the DDR self refresh mode */
		ddr_restore_sr_mode();
		break;
	case PWRSTATE_STOP2:
	case PWRSTATE_LP_STOP2:
	case PWRSTATE_LPLV_STOP2:
		VERBOSE("STOP2 exit\n");
		/* Restore STGEN and generic timer with current clock */
		stm32mp_stgen_config(clk_get_rate(CK_KER_STGEN));

		/* Exit DDR self refresh mode after STOP mode */
		ddr_sr_exit();
		ddr_restore_sr_mode();

		stm32mp_gic_resume();
		stm32mp_gic_cpuif_enable();

		/* Restore register in CA35SS */
		mmio_write_32(A35SSC_BASE + CA35SS_SYSCFG_VBAR_CR, stm32_sec_entrypoint);

		/* Start the secondary core if it was running before STOP */
		if ((core_id == STM32MP_PRIMARY_CPU) &&
		    stm32mp_state_check(STM32MP_SECONDARY_CPU, STATE_START)) {
			/* Reset the secondary core to execute warm boot */
			mmio_write_32(RCC_BASE + RCC_C1P1RSTCSETR, RCC_C1P1RSTCSETR_C1P1PORRST);
		}
		break;
	case PWRSTATE_STOP1:
	case PWRSTATE_LP_STOP1:
	case PWRSTATE_LPLV_STOP1:
		VERBOSE("STOP1 exit\n");
		/* Exit DDR self refresh mode after STOP mode */
		ddr_sr_exit();
		ddr_restore_sr_mode();
		stm32mp2_disable_rcc_wakeup_irq(rcc_base);
		break;
	default:
		ERROR("Invalid state id %x\n", stateid);
		panic();
		break;
	}

	/* Restore SCR */
	write_scr_el3(saved_scr_el3);

	/* Core synchronization to protect DDR access */
	stm32mp_state_set(STM32MP_PRIMARY_CPU, STATE_DDR, true);
	/* Unblock the other core in CPU standby loop */
	sev();
}

static void __dead2 stm32_pwr_domain_pwr_down_wfi(const psci_power_state_t
						  *target_state)
{
	u_register_t mpidr = read_mpidr();
	unsigned int core_id = MPIDR_AFFLVL0_VAL(mpidr);

	/* Core is no more running (stopped or suspended) */
	stm32mp_state_set(core_id, STATE_RUNNING, false);

	/* The first power down on core 0, core 1 is running */
	if ((core_id == STM32MP_PRIMARY_CPU) &&
	    !stm32mp_state_check(STM32MP_PRIMARY_CPU, STATE_START)) {
		uintptr_t sec_entrypoint;

		/* Core 0 can't be turned OFF, emulate it with a WFE loop */
		VERBOSE("BL31: core0 entering wait loop...\n");
		while (!stm32mp_state_check(STM32MP_PRIMARY_CPU, STATE_START)) {
			wfe();
		}

		VERBOSE("BL31: core0 resumed.\n");
		dsbsy();
		sec_entrypoint = mmio_read_32(A35SSC_BASE + CA35SS_SYSCFG_VBAR_CR);
		/* Jump manually to entry point, with mmu disabled. */
		disable_mmu_el3();
		((void(*)(void))sec_entrypoint)();

		/* This shouldn't be reached */
		panic();
	}

	if (psci_is_last_on_cpu_safe() &&
	    stm32_get_stateid(target_state->pwr_domain_state) == PWRSTATE_STANDBY) {
		/* Save the context when all the core are requested to stop */
		stm32_pm_context_save(target_state);
	}

	/* Synchronize instruction flow before auto-reset from WFI */
	isb();
	psci_power_down_wfi();

	/* This shouldn't be reached */
	panic();
}

static void __dead2 stm32_system_off(void)
{
	uintptr_t pwr_base = stm32mp_pwr_base();
	uintptr_t rcc_base = stm32mp_rcc_base();
	u_register_t mpidr = read_mpidr();
	unsigned int core_id = MPIDR_AFFLVL0_VAL(mpidr);
	uintptr_t exti2_base = STM32MP_EXTI2_BASE;
	uint32_t otp_idx = 0U;
	uint32_t otp_value = 0U;

	if (core_id != STM32MP_PRIMARY_CPU) {
		ERROR("PSCI system off request on core %u\n", core_id);
		panic();
	}

	if (!stm32_freeze_other_core(core_id)) {
		WARN("PSCI system off with other core running.\n");

		/* Core is no more running */
		stm32mp_state_set(STM32MP_SECONDARY_CPU, STATE_RUNNING, false);

		/* Program secondary CPU entry points */
		mmio_write_32(A35SSC_BASE + CA35SS_SYSCFG_VBAR_CR,
			      (uintptr_t)&stm32_stop2_entrypoint);

		/* After reset, the core is stopped, waiting in WFI loop */
		stm32mp_state_set(STM32MP_SECONDARY_CPU, STATE_START, false);

		/* Reset the secondary core */
		mmio_write_32(RCC_BASE + RCC_C1P1RSTCSETR, RCC_C1P1RSTCSETR_C1P1PORRST);

		/* Forbid access to DDR */
		stm32mp_state_set(STM32MP_PRIMARY_CPU, STATE_DDR, false);
	}

	/* If CPU2 is not in reset */
	if ((mmio_read_32(pwr_base + PWR_CPU2D2SR) & PWR_CPU2D2SR_CSTATE_MASK) != 0U) {
		WARN("PSCI system off with Cortex M33 running.\n");
		/* Force Hold Boot and reset of CPU2 = Cortex M33 */
		mmio_clrbits_32(rcc_base + RCC_CPUBOOTCR, RCC_CPUBOOTCR_BOOT_CPU2);
		mmio_setbits_32(rcc_base + RCC_C2RSTCSETR, RCC_C2RSTCSETR_C2RST);
	}

	/* If CPU3 is not in reset */
	if ((mmio_read_32(pwr_base + PWR_CPU3D3SR) & PWR_CPU3D3SR_CSTATE_MASK) != 0U) {
		WARN("PSCI system off with Cortex M0 running.\n");
		/* Force reset of CPU3 = Cortex M0+ */
		mmio_setbits_32(rcc_base + RCC_C3CFGR, RCC_C3CFGR_C3RST);
	}

	/* Freeze all watchdog with shadow value of HCONF1 */
	if (stm32_get_otp_index(HCONF1_OTP, &otp_idx, NULL) == 0U) {
		if (stm32_get_otp_value_from_idx(otp_idx, &otp_value) == 0U) {
			otp_value |= HCONF1_OTP_IWDG_FZ_STANDBY_MASK(0);
			otp_value |= HCONF1_OTP_IWDG_FZ_STANDBY_MASK(1);
			otp_value |= HCONF1_OTP_IWDG_FZ_STANDBY_MASK(2);
			otp_value |= HCONF1_OTP_IWDG_FZ_STANDBY_MASK(3);
			stm32_otp_write(otp_value, otp_idx);
		}
	}

	/* Force DDR off */
	ddr_sub_system_clk_off();

	/* Prevent interrupts from spuriously waking up this cpu */
	stm32mp_gic_cpuif_disable();

	/* Request STOP for both cores */
	mmio_write_32(rcc_base + RCC_C1SREQSETR, RCC_C1SREQSETR_STPREQ_MASK);

	/* Request standby2 */
	mmio_write_32(pwr_base + PWR_CPU1CR, PWR_CPU1CR_PDDS_D1 | PWR_CPU1CR_PDDS_D2);
	mmio_write_32(pwr_base + PWR_CPU2CR, PWR_CPU2CR_PDDS_D2);
	mmio_write_32(pwr_base + PWR_D3CR, PWR_D3CR_PDDS_D3);
	stm32mp2_pll1_disable();

	/* Do not maintain RETRAM memory content in Standby or Vbat */
	mmio_write_32(pwr_base + PWR_CR10, PWR_CR10_RETRBSEN_DISABLE);

	/* Clear PM context in BKPSRAM: cold boot at next wake-up */
	stm32_pm_context_clear();

	/* Deactivate all WakeUp except WKUP pins */
	mmio_write_32(exti2_base + EXTI2_C1IMR1, 0U);
	mmio_write_32(exti2_base + EXTI2_C1IMR2, 0U);
	mmio_write_32(exti2_base + EXTI2_C1IMR3, 0U);
	mmio_write_32(exti2_base + EXTI2_C2IMR1, 0U);
	mmio_write_32(exti2_base + EXTI2_C2IMR2, 0U);
	mmio_write_32(exti2_base + EXTI2_C2IMR3, 0U);
	mmio_write_32(exti2_base + EXTI2_C3IMR1, 0U);
	mmio_write_32(exti2_base + EXTI2_C3IMR2, 0U);
	mmio_write_32(exti2_base + EXTI2_C3IMR3, 0U);

	/* Disable STATE_RUNNING state for this core */
	stm32mp_state_set(core_id, STATE_RUNNING, false);

	VERBOSE("BL31: power off\n");

	dsb();
	isb();
	wfi();

	/* This shouldn't be reached */
	panic();
}

static void __dead2 stm32_system_reset(void)
{
	stm32mp_system_reset();
}

/**
 * stm32_validate_power_state() - This function ensures that the power state
 * parameter in request is valid.
 *
 * @power_state		Power state of core
 * @req_state		Requested state
 *
 * @return	Returns status, either success or reason
 */
static int stm32_validate_power_state(unsigned int power_state,
				      psci_power_state_t *req_state)
{
	unsigned int state_id;
	unsigned int i;

	assert(req_state != NULL);

	/*
	 *  Currently we are using a linear search for finding the matching
	 *  entry in the idle power state array. This can be made a binary
	 *  search if the number of entries justify the additional complexity.
	 */
	for (i = 0U; stm32mp_pm_idle_states[i] != 0U; i++) {
		if (power_state == stm32mp_pm_idle_states[i]) {
			break;
		}
	}

	/* Return error if entry not found in the idle state array */
	if (stm32mp_pm_idle_states[i] == 0U) {
		return PSCI_E_INVALID_PARAMS;
	}

	/* Search if board restrict the number of supported modes */
	for (i = 0U; stm32mp_supported_pwr_states[i] != 0U; i++) {
		if (power_state == stm32mp_supported_pwr_states[i]) {
			break;
		}
	}
	if (stm32mp_supported_pwr_states[i] == 0U) {
		ERROR("PSCI power state not supported %x\n", power_state);
		return PSCI_E_INVALID_PARAMS;
	}

	i = 0U;
	state_id = psci_get_pstate_id(power_state);

	/* Parse the State ID and populate the state info parameter */
	while (state_id != 0U) {
		req_state->pwr_domain_state[i++] = state_id & PLAT_LOCAL_PSTATE_MASK;
		state_id >>= PLAT_LOCAL_PSTATE_WIDTH;
	}

	/*
	 * With PSCI v1.0 Extended State-ID, 'last at pwrlvl' is not directly available
	 * and with current toplogy only the CPU level is checked for coordination
	 */
	req_state->last_at_pwrlvl = LVL_CORE;

	return PSCI_E_SUCCESS;
}

static int stm32_validate_ns_entrypoint(uintptr_t entrypoint)
{
	/* The non-secure entry point must be in DDR */
	if (entrypoint < STM32MP_DDR_BASE) {
		return PSCI_E_INVALID_ADDRESS;
	}

	return PSCI_E_SUCCESS;
}

/*
 * The PSCI code uses this API to let the platform participate in state
 * coordination during a power management operation. It compares the platform
 * specific local power states requested by each cpu for a given power domain
 * and returns the coordinated target power state that the domain should
 * enter.
 * This implementation assumes that the power state is RUN for simple
 * CPU STANDBY called by fast path in psci_cpu_suspend(), and assumes that the
 * power state are ordered in increasing depth of the power.
 * As a result, the  coordinated target local power state for a power domain
 * will be the minimum of the requested local power states wich is NOT RUN.
 * And it return a invalid state ()= OFF_STATE + 1) when state is RUN for all
 * the cpu.
 */
plat_local_state_t plat_get_target_pwr_state(unsigned int lvl,
					     const plat_local_state_t *states,
					     unsigned int ncpu)
{
	plat_local_state_t target = PLAT_MAX_OFF_STATE + 1U, temp;
	const plat_local_state_t *st = states;
	unsigned int n = ncpu;

	assert(ncpu > 0U);

	do {
		temp = *st;
		st++;
		if ((temp < target) && (temp != STM32MP_LOCAL_STATE_RUN)) {
			target = temp;
		}
		n--;
	} while (n > 0U);

	return target;
}

/**
 * stm32_get_sys_suspend_power_state() -  Get power state for system suspend
 *
 * @req_state	Requested state
 */
static void stm32_get_sys_suspend_power_state(psci_power_state_t *req_state)
{
	unsigned int state_id;
	unsigned int pwr_state, max_pwr_state = PWRSTATE_STANDBY;
	unsigned int i;
	uintptr_t exti_base = STM32MP_EXTI1_BASE;
	uint32_t c1imr1 = mmio_read_32(exti_base + EXTI1_C1IMR1);
	uint32_t c1imr2 = mmio_read_32(exti_base + EXTI1_C1IMR2);
	uint32_t c1imr3 = mmio_read_32(exti_base + EXTI1_C1IMR3);

	/* Verify the max level supported according to the activated EXTI1 */
	if ((c1imr1 & (EXTI1_C1IMR1_PVD | EXTI1_C1IMR1_PVM)) != 0U) {
		max_pwr_state = PWRSTATE_LPLV_STOP2;
		VERBOSE("%s: max_pwr_state = PWRSTATE_LPLV_STOP2 C1IMR1=%x\n", __func__, c1imr1);
	}

	/* Wake-up pin are connected directly to PWR */
	if (((c1imr1 & ~(EXTI1_C1IMR1_PVD | EXTI1_C1IMR1_PVM)) != 0U) ||
	    ((c1imr2 & ~EXTI1_C1IMR2_WKUP_MASK) != 0U) || (c1imr3 != 0U)) {
		max_pwr_state = PWRSTATE_LP_STOP2;
		VERBOSE("%s: max_pwr_state = PWRSTATE_LP_STOP2, C1IMR1=%x, C1IMR2=%x, C1IMR3=%x\n",
			__func__, c1imr1, c1imr2, c1imr3);
	}

	/* Search the max supported POWERDOWN modes  <= max_pwr_state */
	for (i = ARRAY_SIZE(stm32mp_supported_pwr_states) - 1U; i > 0U; i--) {
		pwr_state = stm32mp_supported_pwr_states[i];
		if ((pwr_state != 0U) && (pwr_state <= max_pwr_state) &&
		    (psci_get_pstate_type(pwr_state) == PSTATE_TYPE_POWERDOWN)) {
			break;
		}
	}
	state_id = psci_get_pstate_id(pwr_state);

	/* Parse the State ID and populate the state info parameter */
	for (i = 0U; i <= PLAT_MAX_PWR_LVL; i++) {
		req_state->pwr_domain_state[i] = state_id & PLAT_LOCAL_PSTATE_MASK;
		state_id >>= PLAT_LOCAL_PSTATE_WIDTH;
	}

	/*
	 * With PSCI v1.0 Extended State-ID, 'last at pwrlvl' is not directly available
	 * and with current toplogy only the CPU level is checked for coordination
	 */
	req_state->last_at_pwrlvl = LVL_CORE;
}

/*******************************************************************************
 * Export the platform handlers. The ARM Standard platform layer will take care
 * of registering the handlers with PSCI.
 ******************************************************************************/
static const plat_psci_ops_t stm32_psci_ops = {
	.cpu_standby = stm32_cpu_standby,
	.pwr_domain_on = stm32_pwr_domain_on,
	.pwr_domain_off = stm32_pwr_domain_off,
	.pwr_domain_validate_suspend = stm32_pwr_domain_validate_suspend,
	.pwr_domain_suspend = stm32_pwr_domain_suspend,
	.pwr_domain_on_finish = stm32_pwr_domain_on_finish,
	.pwr_domain_suspend_finish = stm32_pwr_domain_suspend_finish,
	.pwr_domain_pwr_down_wfi = stm32_pwr_domain_pwr_down_wfi,
	.system_off = stm32_system_off,
	.system_reset = stm32_system_reset,
	.validate_power_state = stm32_validate_power_state,
	.validate_ns_entrypoint = stm32_validate_ns_entrypoint,
	.get_sys_suspend_power_state = stm32_get_sys_suspend_power_state,
};

/*******************************************************************************
 * This function retrieve the generic information from DT.
 * Returns node on success and a negative FDT error code on failure.
 ******************************************************************************/
static int stm32_parse_domain_idle_state(void *fdt)
{
	unsigned int domain_idle_states[PM_IDLE_STATES_SIZE];
	int node = 0;
	int subnode = 0;
	uint32_t power_state;
	unsigned int i = 0U;
	unsigned int j = 0U;
	unsigned int nb_states = 0U;
	int ret;

	/* Search supported domain-idle-state in device tree */
	node = fdt_path_offset(fdt, "/cpus/domain-idle-states");
	if (node < 0) {
		return node;
	}

	memset(domain_idle_states, 0, sizeof(domain_idle_states));
	fdt_for_each_subnode(subnode, fdt, node) {
		ret = fdt_read_uint32(fdt, subnode, "arm,psci-suspend-param", &power_state);
		if (ret != 0) {
			return ret;
		}

		/* Cross check with supported pm idle state in driver */
		for (j = 0U; stm32mp_pm_idle_states[j] != 0U; j++) {
			if (stm32mp_pm_idle_states[j] == power_state) {
				break;
			}
		}
		/* Return error if entry not found and not standby */
		if ((stm32mp_pm_idle_states[j] == 0U) && (power_state != PWRSTATE_STANDBY)) {
			return -EINVAL;
		}

		domain_idle_states[i++] = power_state;

		/* Check array size */
		if (i > ARRAY_SIZE(domain_idle_states)) {
			return -E2BIG;
		}
	}

	memset(stm32mp_supported_pwr_states, 0, sizeof(stm32mp_supported_pwr_states));

	/* The CPU idle state is always supported, not present in domain node */
	stm32mp_supported_pwr_states[nb_states++] = PWRSTATE_RUN;

	/* Add each supported power state with same order than stm32mp_pm_idle_states */
	for (j = 0U; stm32mp_pm_idle_states[j] != 0U; j++) {
		for (i = 0U; domain_idle_states[i] != 0U && i < PM_IDLE_STATES_SIZE; i++) {
			if (domain_idle_states[i] == stm32mp_pm_idle_states[j]) {
				stm32mp_supported_pwr_states[nb_states++] = domain_idle_states[i];
				break;
			}
		}
	}

	/* Add STANDBY at the end of the list if supported */
	for (i = 0U; domain_idle_states[i] != 0U && i < PM_IDLE_STATES_SIZE; i++) {
		if (domain_idle_states[i] == PWRSTATE_STANDBY) {
			stm32mp_supported_pwr_states[nb_states++] = PWRSTATE_STANDBY;
			break;
		}
	}

	VERBOSE("stm32mp_supported_pwr_states[]=\n");
	for (i = 0U; stm32mp_supported_pwr_states[i] != 0U; i++) {
		VERBOSE("%u = %x\n", i, stm32mp_supported_pwr_states[i]);
	}

	return 0;
}

/*******************************************************************************
 * Initialize STM32MP2 for PM support: RCC, PWR
 ******************************************************************************/
struct pm_param {
	uint8_t popl_d1;
	uint8_t podh_d2;
	uint8_t popl_d2;
	uint8_t lplvdly_d2;
	uint8_t lpcfg_d2;
	uint32_t lpstop1dly;
};

static void stm32_read_dt_pm_param(void *fdt, struct pm_param *param)
{
	int node;

	node = fdt_node_offset_by_compatible(fdt, -1, DT_PWR_COMPAT);
	if (node <= 0) {
		panic();
	}

	param->popl_d1 = fdt_read_uint32_default(fdt, node, "st,popl-d1-ms", DEFAULT_POPL_D1);
	param->podh_d2 = fdt_read_uint32_default(fdt, node, "st,podh-d2-ms", DEFAULT_PODH_D2);
	param->popl_d2 = fdt_read_uint32_default(fdt, node, "st,popl-d2-ms", DEFAULT_POPL_D2);
	param->lpcfg_d2 = fdt_read_uint32_default(fdt, node, "st,lpcfg-d2", DEFAULT_LPCFG_D2);
	param->lplvdly_d2 = fdt_read_uint32_default(fdt, node, "st,lplvdly-d2", DEFAULT_LPLVDLY_D2);
	param->lpstop1dly = fdt_read_uint32_default(fdt, node, "st,lpstop1dly-us",
						    DEFAULT_LPSTOP1DLY);
}

static void stm32_pm_init(void *fdt)
{
	uintptr_t pwr_base = stm32mp_pwr_base();
	uintptr_t rcc_base = stm32mp_rcc_base();
	uint32_t lsmcu;
	struct pm_param param;

	stm32mp2_setup_rcc_wakeup_irq(rcc_base);

	/* RCC init: DDR is shared by default */
	mmio_setbits_32(rcc_base + RCC_DDRITFCFGR, RCC_DDRITFCFGR_DDRSHR);

	/* Legacy mode: only CPU1 is allowed to boot, core1 is OFF */
	mmio_setbits_32(rcc_base + RCC_LEGBOOTCR, RCC_LEGBOOTCR_LEGACY_BEN);

	mmio_write_32(rcc_base + RCC_C1SREQCLRR, RCC_C1SREQSETR_STPREQ_MASK);

	/* Maintain BKPSRAM & RETRAM content in Standby */
	mmio_write_32(pwr_base + PWR_CR9, PWR_CR9_BKPRBSEN);
	mmio_write_32(pwr_base + PWR_CR10, PWR_CR10_RETRBSEN_STANDBY);

	/* Prevent RETRAM erase */
	mmio_write_32(RAMCFG_BASE + RAMCFG_RETRAMCR, SRAMHWERDIS);

	/* System standby not set (D3) */
	mmio_write_32(pwr_base + PWR_D3CR, 0U);

	/* Configure delay in PWR */
	stm32_read_dt_pm_param(fdt, &param);

	mmio_write_32(pwr_base + PWR_D1CR,
		      (param.popl_d1 << PWR_D1CR_POPL_D1_SHIFT) & PWR_D1CR_POPL_D1_MASK);

	mmio_write_32(pwr_base + PWR_D2CR,
		      (param.lpcfg_d2 & PWR_D2CR_LPCFG_D2) |
		      ((param.popl_d2  << PWR_D2CR_POPL_D2_SHIFT) & PWR_D2CR_POPL_D2_MASK) |
		      ((param.lplvdly_d2 << PWR_D2CR_LPLVDLY_D2_SHIFT) & PWR_D2CR_LPLVDLY_D2_MASK) |
		      ((param.podh_d2 << PWR_D2CR_PODH_D2_SHIFT) & PWR_D2CR_PODH_D2_MASK));

	/* Compute RCC PWR LP DLY according to parent clock */
	lsmcu = mmio_read_32(rcc_base + RCC_LSMCUDIVR) & RCC_LSMCUDIVR_LSMCUDIV;
	lpstop1_pwrlpdly = PWRLPDLYCR_VAL(param.lpstop1dly, lsmcu);
	/* Wait 2ms for Stop2 */
	stop2_pwrlpdly = PWRLPDLYCR_VAL(2000, lsmcu);
}

/*******************************************************************************
 * Export the platform specific power ops.
 ******************************************************************************/
int plat_setup_psci_ops(uintptr_t sec_entrypoint,
			const plat_psci_ops_t **psci_ops)
{
	int ret = 0;
	void *fdt = NULL;
	uint32_t stop2_entrypoint = (uint32_t)(uintptr_t)&stm32_stop2_entrypoint;
	struct nvmem_cell stop2_entrypoint_cell;
	assert(stop2_entrypoint < UINT32_MAX);

	if (fdt_get_address(&fdt) == 0) {
		panic();
	}

	ret = stm32_parse_domain_idle_state(fdt);
	if (ret != 0) {
		ERROR("invalid domain-idle-states in device tree %d\n", ret);
		panic();
	}

	/* Program secondary CPU entry points. */
	stm32_sec_entrypoint = sec_entrypoint;
	mmio_write_32(A35SSC_BASE + CA35SS_SYSCFG_VBAR_CR, stm32_sec_entrypoint);

	/* Initialize the state per cpu */
	stm32_percpu_data[STM32MP_PRIMARY_CPU].state[STATE_START] = true;
	stm32_percpu_data[STM32MP_PRIMARY_CPU].state[STATE_RUNNING] = true;
	stm32_percpu_data[STM32MP_PRIMARY_CPU].state[STATE_DDR] = true;
	stm32_percpu_data[STM32MP_SECONDARY_CPU].state[STATE_START] = false;
	stm32_percpu_data[STM32MP_SECONDARY_CPU].state[STATE_RUNNING] = false;

	/* Save boot entry point for STOP2 exit */
	ret = stm32_get_stop2_entrypoint_cell(&stop2_entrypoint_cell);
	if (ret != 0) {
		return ret;
	}
	nvmem_cell_write(&stop2_entrypoint_cell, (uint8_t *)&stop2_entrypoint,
			 sizeof(stop2_entrypoint));

	stm32_pm_init(fdt);

	*psci_ops = &stm32_psci_ops;

	return 0;
}
