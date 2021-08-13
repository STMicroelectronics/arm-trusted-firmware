/*
 * Copyright (c) 2024-2026, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <errno.h>

#include <arch_helpers.h>
#include <common/debug.h>
#include <drivers/arm/gic_common.h>
#include <drivers/arm/gicv2.h>
#include <drivers/st/stm32mp_clkfunc.h>
#include <drivers/st/stm32mp_reset.h>
#include <lib/mmio.h>
#include <lib/psci/psci.h>
#include <plat/common/platform.h>

#include <platform_def.h>
#include <stm32mp2_private.h>

static uintptr_t stm32_sec_entrypoint;

/*******************************************************************************
 * STM32MP2 handler called when a CPU is about to enter standby.
 * Called by core 1 to enter in wfi.
 ******************************************************************************/
static void stm32_cpu_standby(plat_local_state_t cpu_state)
{
	uint32_t interrupt = GIC_SPURIOUS_INTERRUPT;

	assert(cpu_state == STM32MP_LOCAL_STATE_RET);

	/*
	 * Enter standby state.
	 * Synchronize on memory accesses and instruction flow before the WFI
	 * instruction.
	 */
	dsb();
	isb();
	while (interrupt == GIC_SPURIOUS_INTERRUPT) {
		wfi();

		/* Acknowledge IT */
		interrupt = gicv2_acknowledge_interrupt();
		/* If Interrupt == 1022 it will be acknowledged by non secure */
		if ((interrupt != PENDING_G1_INTID) &&
		    (interrupt != GIC_SPURIOUS_INTERRUPT)) {
			gicv2_end_of_interrupt(interrupt);
		}
	}
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

	if (core_id == STM32MP_PRIMARY_CPU) {
		/* Cortex-A35 core0 can't be turned OFF, emulate it with a WFE loop */
		VERBOSE("BL31: Releasing core0 from wait loop...\n");
		dsb();
		isb();
		sev();
	} else {
#if !STM32MP21
		/* Reset the secondary core */
		mmio_write_32(RCC_BASE + RCC_C1P1RSTCSETR, RCC_C1P1RSTCSETR_C1P1PORRST);
#endif /* !STM32MP21 */
	}

	return PSCI_E_SUCCESS;
}

static void stm32_pwr_domain_off(const psci_power_state_t *target_state)
{
	/* Prevent interrupts from spuriously waking up this cpu */
	stm32mp_gic_cpuif_disable();

}

static void stm32_pwr_domain_suspend(const psci_power_state_t *target_state)
{
	/* Nothing to do, power domain is not disabled */
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

	if (core_id == STM32MP_PRIMARY_CPU) {
		dsb();
		isb();
	} else {
		/* Restore generic timer after reset */
		stm32mp_stgen_restore_rate();
#if !STM32MP21
		/* clear flag after core 1 power on */
		mmio_setbits_32(rcc_base + RCC_C1HWRSTSCLRR, RCC_C1HWRSTSCLRR_C1P1RSTF);
#endif /* !STM32MP21 */
	}

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
	/* Nothing to do, power domain is not disabled */
}

static void stm32_pwr_domain_pwr_down_wfi(const psci_power_state_t *target_state)
{
	u_register_t mpidr = read_mpidr();
	unsigned int core_id = MPIDR_AFFLVL0_VAL(mpidr);

	if (core_id == STM32MP_PRIMARY_CPU) {
		ERROR("stm32mp2 Power Down WFI: operation not handled.\n");
		panic();
	}
}

static void __dead2 stm32_system_off(void)
{
	ERROR("stm32mp2 System Off: operation not handled.\n");
	panic();
}

static void __dead2 stm32_system_reset(void)
{
	stm32mp_system_reset();
}

static int stm32_validate_power_state(unsigned int power_state,
				      psci_power_state_t *req_state)
{
	return PSCI_E_INVALID_PARAMS;
}

static int stm32_validate_ns_entrypoint(uintptr_t entrypoint)
{
	/* The non-secure entry point must be in DDR */
	if (entrypoint < STM32MP_DDR_BASE) {
		return PSCI_E_INVALID_ADDRESS;
	}

	return PSCI_E_SUCCESS;
}

static void stm32_get_sys_suspend_power_state(psci_power_state_t *req_state)
{
}

/*******************************************************************************
 * Export the platform handlers. The ARM Standard platform layer will take care
 * of registering the handlers with PSCI.
 ******************************************************************************/
static const plat_psci_ops_t stm32_psci_ops = {
	.cpu_standby = stm32_cpu_standby,
	.pwr_domain_on = stm32_pwr_domain_on,
	.pwr_domain_off = stm32_pwr_domain_off,
	.pwr_domain_suspend = stm32_pwr_domain_suspend,
	.pwr_domain_on_finish = stm32_pwr_domain_on_finish,
	.pwr_domain_suspend_finish = stm32_pwr_domain_suspend_finish,
	.pwr_domain_pwr_down = stm32_pwr_domain_pwr_down_wfi,
	.system_off = stm32_system_off,
	.system_reset = stm32_system_reset,
	.validate_power_state = stm32_validate_power_state,
	.validate_ns_entrypoint = stm32_validate_ns_entrypoint,
	.get_sys_suspend_power_state = stm32_get_sys_suspend_power_state,
};

/*******************************************************************************
 * Export the platform specific power ops.
 ******************************************************************************/
int plat_setup_psci_ops(uintptr_t sec_entrypoint,
			const plat_psci_ops_t **psci_ops)
{
	/* Program secondary CPU entry points. */
	stm32_sec_entrypoint = sec_entrypoint;
	stm32mp_ca35_set_vbar(stm32_sec_entrypoint);

	*psci_ops = &stm32_psci_ops;

	return 0;
}
