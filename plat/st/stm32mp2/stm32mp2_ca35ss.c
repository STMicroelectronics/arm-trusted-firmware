/*
 * Copyright (c) 2026, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <common/debug.h>
#include <drivers/clk.h>
#include <drivers/delay_timer.h>
#include <lib/mmio.h>

#include <platform_def.h>
#include <stm32mp2_private.h>

/* Standardized status and control registers (SSC) access modes */
#define A35SSC_SSC_RW			U(0x0)
#define A35SSC_SSC_WS1			U(0x4)
#define A35SSC_SSC_WC1			U(0x8)
#define A35SSC_SSC_WT1			U(0xC)

/*
 * CA35SS register (base relative)
 * - Standardized Status and Control registers (SSC) access modes (offset=0x0)
 * - SYSCFG registers (offset=0x2000)
 */
#define CA35SS_SSC_LPI_TSGEN_NTS(type)	(U(0x0D0) + A35SSC_SSC_ ## type)
#define CA35SS_SSC_LPI_STGEN_NTS(type)	(U(0x140) + A35SSC_SSC_ ## type)
#define CA35SS_SSC_OPP_REQ(type)	(U(0x1A0) + A35SSC_SSC_ ## type)
#define CA35SS_SSC_MEM_CTRL(type)	(U(0x1C0) + A35SSC_SSC_ ## type)

#define CA35SS_SYSCFG_VBAR_CR		0x2084U

/*
 * CA35SS_SSC_LPI_TSGEN_NTS_ register fields
 */
#define CA35SS_SSC_LPI_TSGEN_CSYSREQ			BIT_32(8)
#define CA35SS_SSC_LPI_TSGEN_CSYSACK			BIT_32(9)

/*
 * CA35SS_SSC_LPI_STGEN_NTS register fields
 */
#define CA35SS_SSC_LPI_STGEN_CSYSREQ			BIT_32(24)
#define CA35SS_SSC_LPI_STGEN_CSYSACK			BIT_32(25)

/*
 * CA35SS_SSC_OPP_REQ register fields
 */
#define CA35SS_SSC_OPP_REQ_REQ				BIT_32(0)
#define CA35SS_SSC_OPP_REQ_ACK				BIT_32(1)

/*
 * CA35SS_SSC_MEM_CTRL register fields
 */
#define CA35SS_SSC_MEM_CTRL_RM_MASK			GENMASK_32(2, 0)
#define CA35SS_SSC_MEM_CTRL_RM_OVERDRIVE		U(3)
#define CA35SS_SSC_MEM_CTRL_RME				BIT_32(3)

#define TIMEOUT_US			U(1000)

void stm32mp_ca35_set_vbar(uintptr_t vbar)
{
	mmio_write_32(A35SSC_BASE + CA35SS_SYSCFG_VBAR_CR, (uint32_t)vbar);
}

/*
 * To guarantee a correct synchronization of the ARM counter with STGEN,
 * the ARM generic timer has to  be isolated before entering in low power
 * mode. Once this is done, the delays or timeouts function based on this
 * timer will never end.
 */
void stm32mp_ca35_lpi_isolate(void)
{
	uint64_t timeout = 0;
	uint32_t counter = UINT32_MAX;

	/* Isolate TSGEN for debug only if associated clock is enabled */
	if (clk_is_enabled(CK_KER_TSDBG)) {
		mmio_write_32(A35SSC_BASE + CA35SS_SSC_LPI_TSGEN_NTS(WC1),
			      CA35SS_SSC_LPI_TSGEN_CSYSREQ);
		timeout = timeout_init_us(TIMEOUT_US);
		while ((mmio_read_32(A35SSC_BASE + CA35SS_SSC_LPI_TSGEN_NTS(WC1))
			& CA35SS_SSC_LPI_TSGEN_CSYSACK) == CA35SS_SSC_LPI_TSGEN_CSYSACK) {
			if (timeout_elapsed(timeout)) {
				panic();
			}
		}
	}

	/* Use write clear registers to clear bits */
	mmio_write_32(A35SSC_BASE + CA35SS_SSC_LPI_STGEN_NTS(WC1),
		      CA35SS_SSC_LPI_STGEN_CSYSREQ);

	while ((mmio_read_32(A35SSC_BASE + CA35SS_SSC_LPI_STGEN_NTS(WC1))
		& CA35SS_SSC_LPI_STGEN_CSYSACK) == CA35SS_SSC_LPI_STGEN_CSYSACK) {
		/* With STGEN isolated, timer is not functional */
		counter--;
		if (counter == 0U) {
			panic();
		}
	}
}

void stm32mp_ca35_lpi_restore(void)
{
	uint64_t timeout = 0;
	uint32_t counter = UINT32_MAX;
	uint64_t reference = read_cntpct_el0();

	/* Use write set registers to set bits */
	mmio_write_32(A35SSC_BASE + CA35SS_SSC_LPI_STGEN_NTS(WS1),
		      CA35SS_SSC_LPI_STGEN_CSYSREQ);

	while ((mmio_read_32(A35SSC_BASE + CA35SS_SSC_LPI_STGEN_NTS(WS1))
		& CA35SS_SSC_LPI_STGEN_CSYSACK) == 0U) {
		/* With STGEN isolated, timer is not functional */
		counter--;
		if (counter == 0U) {
			panic();
		}
	}

	/*
	 * With STGEN isolated, timer is not functional.
	 * Wait for the ARM timer to be incremented at least once
	 * to avoid that next timeout_init_us() reads a bad value.
	 */
	counter = UINT32_MAX;
	while (read_cntpct_el0() == reference) {
		counter--;
		if (counter == 0U) {
			panic();
		}
	}

	if (clk_is_enabled(CK_SYSDBG)) {
		mmio_write_32(A35SSC_BASE + CA35SS_SSC_LPI_TSGEN_NTS(WS1),
			      CA35SS_SSC_LPI_TSGEN_CSYSREQ);
		timeout = timeout_init_us(TIMEOUT_US);
		while ((mmio_read_32(A35SSC_BASE + CA35SS_SSC_LPI_TSGEN_NTS(WS1))
			& CA35SS_SSC_LPI_TSGEN_CSYSACK) == 0U) {
			if (timeout_elapsed(timeout)) {
				panic();
			}
		}
	}
}

/*
 * Configure CA35SS for new OPP with syscfg register SSC_MEM and OPP_REQ
 * the parameter overdrive indicates if the new OPP has an overdrive frequency
 * NB: as STGEN is deactivated during OPP request, use polling without timeout
 */
void stm32mp_ca35_configure_opp(bool overdrive)
{
	uint32_t mem_ctrl = mmio_read_32(A35SSC_BASE + CA35SS_SSC_MEM_CTRL(RW));
	uint32_t counter = UINT32_MAX;

	/*
	 * SSC_MEM control the speed of memories inside Cortex-A35
	 * - RME: 0 default margin setting for nominal Cortex-A35 frequencies
	 * - RME: 1 margin setting from RM[2:0]
	 * - RM: CA35SS_SSC_MEM_CTRL_RM_OVERDRIVE the recommended value for
	 *       Cortex-A35 overdrive frequencies, not used if RME = 0
	 *
	 * This function configure the CA35SS values only if the OPP change
	 * (nominal vs overdrive)
	 */
	if (overdrive) {
		if ((mem_ctrl & CA35SS_SSC_MEM_CTRL_RME) == CA35SS_SSC_MEM_CTRL_RME) {
			return;
		}
		mmio_write_32(A35SSC_BASE + CA35SS_SSC_MEM_CTRL(WC1),
			      CA35SS_SSC_MEM_CTRL_RM_MASK);
		mmio_write_32(A35SSC_BASE + CA35SS_SSC_MEM_CTRL(WS1),
			      CA35SS_SSC_MEM_CTRL_RME | CA35SS_SSC_MEM_CTRL_RM_OVERDRIVE);
	} else {
		if ((mem_ctrl & CA35SS_SSC_MEM_CTRL_RME) == 0U) {
			return;
		}
		mmio_write_32(A35SSC_BASE + CA35SS_SSC_MEM_CTRL(WC1), CA35SS_SSC_MEM_CTRL_RME);
	}

	stm32mp_ca35_lpi_isolate();

	/* Launch the OPP request*/
	mmio_write_32(A35SSC_BASE + CA35SS_SSC_OPP_REQ(WS1), CA35SS_SSC_OPP_REQ_REQ);

	/* Wait for the request to be complete */
	while ((mmio_read_32(A35SSC_BASE + CA35SS_SSC_OPP_REQ(RW))
		& CA35SS_SSC_OPP_REQ_ACK) == 0U) {
		/* After LPI isolate, the ARM generic timer is frozen  */
		counter--;
		if (counter == 0U) {
			panic();
		}
	}

	/* Stop the OPP request */
	mmio_write_32(A35SSC_BASE + CA35SS_SSC_OPP_REQ(WC1), CA35SS_SSC_OPP_REQ_REQ);

	stm32mp_ca35_lpi_restore();
}
