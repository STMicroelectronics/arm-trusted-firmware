/*
 * Copyright (c) 2025, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <errno.h>

#include <common/debug.h>
#include <common/fdt_wrappers.h>
#include <drivers/generic_delay_timer.h>
#include <drivers/st/stm32mp2_clk.h>
#include <drivers/st/stm32mp_clkfunc.h>
#include <lib/mmio.h>
#include <lib/spinlock.h>
#include <plat/common/platform.h>
#include <platform_def.h>

#include "ca35ss_clk_svc.h"
#include <stm32mp2_smc.h>
#include <stm32mp_common.h>
#include <stm32mp_svc_setup.h>

#define OPP_MAX_NB			4

#define HW_NO_OVERDRIVE			BIT(0)
#define HW_SUPPORTS_OVERDRIVE		BIT(1)

#define PART_NUMBER_SUPPORT_OVERDRIVE	BIT(31) /* stm32mp2xxD or stm32mp2xxF */

enum stm32_ca35ss_clk_svc_function {
	CA35SS_CLK_SVC_NO_FUNCTION,
	CA35SS_CLK_SVC_SET_RATE,
	CA35SS_CLK_SVC_SET_RATE_STATUS,
	CA35SS_CLK_SVC_RECALC_RATE,
	CA35SS_CLK_SVC_ROUND_RATE,
	CA35SS_CLK_SVC_NB_FUNCTION,
};

/* State machine */
enum stm32_ca35ss_clk_svc_state {
	CA35SS_CLK_SVC_STATE_NO_STATE,
	CA35SS_CLK_SVC_STATE_BASE,
	CA35SS_CLK_SVC_STATE_CHG,
	CA35SS_CLK_SVC_STATE_CHG_PLL1_DONE,
	CA35SS_CLK_SVC_STATE_NB_STATE,
};

struct ca35ss_state
{
	int32_t state;
	int32_t target_opp_idx;
};

static struct ca35ss_state state;

static void ca35ss_reinit_state(void)
{
	state.state = CA35SS_CLK_SVC_STATE_BASE;
	state.target_opp_idx = -1;
}

/* Device tree parsing */
struct ca35ss_opp
{
	uint64_t hz[OPP_MAX_NB];
	uint32_t microvolt[OPP_MAX_NB];
	uint32_t supported_hw[OPP_MAX_NB];
	int32_t opp_nb;
};

static struct ca35ss_opp opp;

static uint32_t get_opp_hw_filter(void) {
	uint32_t opp_hw_filter = 0;
	uint32_t part_number = stm32mp_get_part_number();

	if (part_number & PART_NUMBER_SUPPORT_OVERDRIVE) {
		opp_hw_filter = HW_SUPPORTS_OVERDRIVE;
	} else {
		opp_hw_filter = HW_NO_OVERDRIVE;
	}

	return opp_hw_filter;
}

static int32_t dt_find_opp_node(void *fdt, int32_t *opp_nb)
{
	uint32_t opp_phandle = 0;
	int32_t node_cpu = 0;
	int32_t node_opp = 0;
	int32_t err = 0;

	node_cpu = fdt_node_offset_by_compatible(fdt, -1, DT_CPU_COMPAT);
	if (node_cpu < 0) {
		return node_cpu;
	}

	err = fdt_read_uint32(fdt, node_cpu, "operating-points-v2",
			      &opp_phandle);
	if (err != 0) {
		if (err == -FDT_ERR_NOTFOUND) {
			/* OPP not enabled in the device tree */
			*opp_nb = -1;
			return 0;
		}
		return err;
	}

	node_opp = fdt_node_offset_by_phandle(fdt, opp_phandle);
	if (node_opp < 0) {
		return node_opp;
	}

	err = fdt_node_check_compatible(fdt, node_opp, "operating-points-v2");
	if (err != 0) {
		if (err == 1) {
			return -FDT_ERR_BADVALUE;
		}
		return err;
	}

	return node_opp;
}

static int32_t dt_read_opp(int32_t *opp_nb, uint64_t opp_hz[OPP_MAX_NB],
		       uint32_t opp_microvolt[OPP_MAX_NB],
		       uint32_t opp_supported_hw[OPP_MAX_NB])
{
	int32_t subnode_opp = 0;
	int32_t node_opp = 0;
	void *fdt = NULL;
	int32_t err = 0;

	/* Find operating-points-v2 node in the device tree */
	if (fdt_get_address(&fdt) == 0) {
		return 1;
	}

	node_opp = dt_find_opp_node(fdt, opp_nb);
	if (node_opp < 0) {
		return node_opp;
	}
	if (*opp_nb < 0) {
		return 0;
	}

	/* Iterate over OPP subnodes */
	subnode_opp = fdt_first_subnode(fdt, node_opp);
	for (int32_t opp_idx = 0; opp_idx < OPP_MAX_NB; opp_idx++) {

		/* Is this an actual node? Otherwise you read all opp */
		if (subnode_opp < 0) {
			*opp_nb = opp_idx;
			break;
		}

		/* Read opp attributes */
		err = fdt_read_uint64(fdt, subnode_opp, "opp-hz",
				      &opp_hz[opp_idx]);
		if (err != 0) {
			return err;
		}

		err = fdt_read_uint32(fdt, subnode_opp, "opp-microvolt",
				      &opp_microvolt[opp_idx]);
		if (err != 0) {
			return err;
		}

		err = fdt_read_uint32(fdt, subnode_opp, "opp-supported-hw",
				      &opp_supported_hw[opp_idx]);
		if (err != 0) {
			return err;
		}

		/* Iterate to next subnode */
		subnode_opp = fdt_next_subnode(fdt, subnode_opp);
	}

	if (*opp_nb == 0) {
		return -FDT_ERR_BADVALUE;
	}

	return 0;
}

/* Helpers / Utils */
static bool clock_scale_before_volt_scale(void/* TODO */)
{
	/* TODO: implement this function */
	return 0;
}

static uint32_t vddcpu_set_mvolt(int32_t opp_idx)
{
	if ((opp_idx < 0) || (opp.opp_nb < opp_idx)) {
		return STM32_SMC_FAILED;
	}

	/* TODO: implement this function
	ERROR("vddcpu_set_mvolt failed with status %d", 0); */
	return STM32_SMC_FAILED;
}

static uint32_t vddcpu_set_mvolt_get_status(void)
{
	/* TODO: implement this function
	ERROR("vddcpu_set_mvolt_get_status failed with status %d", 0); */
	return STM32_SMC_FAILED;
}

static uint32_t get_closest_opp_idx(uint64_t target_rate)
{
	int32_t opp_idx, opp_idx_iter;
	uint64_t rate_delta, rate_delta_iter;

	opp_idx = OPP_MAX_NB;
	rate_delta = __UINT64_MAX__;
	for (opp_idx_iter = 0; opp_idx_iter < opp.opp_nb; opp_idx_iter++) {
		if (opp.hz[opp_idx_iter] == target_rate) {
			/* Perfect case, no need to loop further */
			opp_idx = opp_idx_iter;
			break;
		}
		/* No perfect case on this turn, find smallest delta */
		if (opp.hz[opp_idx_iter] > target_rate) {
			rate_delta_iter = opp.hz[opp_idx_iter] - target_rate;
		} else {
			rate_delta_iter = target_rate - opp.hz[opp_idx_iter];
		}
		if (rate_delta_iter < rate_delta) {
			rate_delta = rate_delta_iter;
			opp_idx = opp_idx_iter;
		}
	}

	return opp_idx;
}

/* SMC setup */
uint32_t ca35ss_clk_svc_setup(void)
{
	uint32_t status;
	int32_t opp_idx;
	int32_t err = 0;
	uint32_t opp_hw_filter = get_opp_hw_filter();

	/* Read the DT to find available configurations */
	opp.opp_nb = 0;
	state.state = CA35SS_CLK_SVC_STATE_NO_STATE;
	err = dt_read_opp(&opp.opp_nb, opp.hz, opp.microvolt, opp.supported_hw);
	if ((err != 0) || (opp.opp_nb < 0)) {
		if (opp.opp_nb < 0) {
			/* 
			 * OPP is disabled in the DT. This is not a fail, the
			 * handlers are simply disabled by not initializing the
			 * state machine (state.state = NO_STATE).
			 * 
			 */
			status = STM32_SMC_OK;
			goto exit_label;
		}
		ERROR("ca35ss_clk_svc_setup: couldn't parse DT (%d)\n", err);
		status = STM32_SMC_FAILED;
		goto exit_label;
	}

	/* Verify OPP supported hw */
	for (opp_idx = 0; opp_idx < opp.opp_nb; opp_idx++) {
		if (!(opp.supported_hw[opp_idx] | opp_hw_filter)) {
			ERROR("ca35ss_clk_svc_setup: opp-supported-hw (0x%x) doesn't match hw\n",
			      opp.supported_hw[opp_idx]);
			status = STM32_SMC_FAILED;
			goto exit_label;
		}
	}

	/* Init stm32mp2 clock driver for PLL1 */
	err = stm32mp2_pll1_init();
	if (err != 0) {
		ERROR("ca35ss_clk_svc_setup: couldn't init STM32MP2 PLL1 (%d)\n", err);
		status = STM32_SMC_FAILED;
		goto exit_label;
	}

	/* Init state machine */
	ca35ss_reinit_state();

	status = STM32_SMC_OK;

exit_label:
	return status;
}

/* SMC handlers */
static uint32_t ca35ss_clk_svc_handler_get_rate(uint32_t *rate)
{
	uint64_t calc_rate = stm32mp2_pll1_recalc_rate();
	uint32_t status;

	if (calc_rate > UINT32_MAX) {
		/* This should not happen since the VCO max rate is 3200 MHz */
		*rate = 0;
		status = STM32_SMC_FAILED;
	} else {
		*rate = (uint32_t)calc_rate;
		status = STM32_SMC_OK;
	}

	return status;
}

static uint32_t ca35ss_clk_svc_handler_round_rate(uint64_t rate, uint32_t *rounded_rate)
{
	uint32_t status = STM32_SMC_OK;
	int32_t opp_idx = get_closest_opp_idx(rate);

	if (opp_idx >= opp.opp_nb) {
		status = STM32_SMC_INVALID_PARAMS;
	}

	if (opp.hz[opp_idx] > UINT32_MAX) {
		*rounded_rate = 0;
		status = STM32_SMC_FAILED;
	} else {
		*rounded_rate = (uint32_t) opp.hz[opp_idx];
	}

	return status;
}

uint32_t ca35ss_clk_svc_handler_set_rate(uint64_t target_rate)
{
	uint32_t status = STM32_SMC_FAILED;
	int32_t err;
	int32_t opp_idx;
	uint64_t current_rate;

	/* This method can only be called in certain states */
	switch (state.state)
	{
	case CA35SS_CLK_SVC_STATE_BASE:
		/* LEGAL */
		break;

	default:
		/* ILLEGAL */
		status = STM32_SMC_NO_PERM;
		goto exit_label;
	}

	/* Verify the asked target_rate is not the current rate */
	current_rate = stm32mp2_pll1_recalc_rate();
	if (current_rate == target_rate) {
		status = STM32_SMC_OK;
		goto exit_label;
	}

	/* Find cfg for a given target_rate */
	opp_idx = get_closest_opp_idx(target_rate);
	if (opp_idx >= opp.opp_nb) {
		status = STM32_SMC_INVALID_PARAMS;
		goto exit_label;
	}

	/* State: base -> changing */
	state.target_opp_idx = opp_idx;
	state.state = CA35SS_CLK_SVC_STATE_CHG;

	/* Change PLL1 rate if to be done before changing vddcpu */
	if (clock_scale_before_volt_scale(/* TODO*/)) {
		err = stm32mp2_pll1_set_rate(opp.hz[opp_idx]);
		if (err == 0) {
			state.state = CA35SS_CLK_SVC_STATE_CHG_PLL1_DONE;
		} else {
			/* Something went wrong then back to base state */
			status = STM32_SMC_FAILED;
			goto exit_label_reinit_state;
		}
	}

	/* Start asynchronous scaling of vcpu */
	status = vddcpu_set_mvolt(opp_idx);

	/* If not on-going then back to base state */
	if (status == STM32_SMC_ON_GOING) {
		goto exit_label;
	}

exit_label_reinit_state:
	ca35ss_reinit_state();
exit_label:
	return status;
}

static uint32_t ca35ss_clk_svc_handler_set_rate_status(void)
{
	uint32_t status = STM32_SMC_FAILED;
	uint64_t target_rate = opp.hz[state.target_opp_idx];

	/* This method can only be called in certain states */
	switch (state.state)
	{
	case CA35SS_CLK_SVC_STATE_CHG:
	case CA35SS_CLK_SVC_STATE_CHG_PLL1_DONE:
		/* LEGAL */
		break;

	default:
		/* ILLEGAL */
		status = STM32_SMC_NO_PERM;
		goto exit_label;
	}

	/* Get status for asynchronous scaling of vcpu */
	status = vddcpu_set_mvolt_get_status();

	switch (status)
	{
	case STM32_SMC_ON_GOING:
		goto exit_label;

	case STM32_SMC_OK:
		/* Change PLL1 rate if not done yet */
		if (state.state != CA35SS_CLK_SVC_STATE_CHG_PLL1_DONE) {
			status = stm32mp2_pll1_set_rate(target_rate);
		}
		goto exit_label_reinit_state;

	default:
		/* Something went wrong */
		goto exit_label_reinit_state;
	}

exit_label_reinit_state:
	ca35ss_reinit_state();
exit_label:
	return status;
}

uint32_t ca35ss_clk_svc_handler(u_register_t fid, u_register_t arg1,
			       u_register_t arg2, u_register_t arg3,
			       uint32_t *ret2, bool *ret2_enabled)
{
	static spinlock_t slock;
	uint32_t status = STM32_SMC_FAILED;

	spin_lock(&slock);

	if (state.state == CA35SS_CLK_SVC_STATE_NO_STATE) {
		status = STM32_SMC_NOT_SUPPORTED;
	} else {
		switch (fid) {
		case CA35SS_CLK_SVC_RECALC_RATE:
			status = ca35ss_clk_svc_handler_get_rate(ret2);
			*ret2_enabled = true;
			break;
		case CA35SS_CLK_SVC_SET_RATE:
			status = ca35ss_clk_svc_handler_set_rate((uint64_t)
								 arg1);
			break;
		case CA35SS_CLK_SVC_SET_RATE_STATUS:
			status = ca35ss_clk_svc_handler_set_rate_status();
			break;
		case CA35SS_CLK_SVC_ROUND_RATE:
			status = ca35ss_clk_svc_handler_round_rate((uint64_t)
								   arg1, ret2);
			*ret2_enabled = true;
			break;
		default:
			status = STM32_SMC_NOT_SUPPORTED;
			break;
		}
	}

	spin_unlock(&slock);

	return status;
}
