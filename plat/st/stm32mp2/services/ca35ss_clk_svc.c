/*
 * Copyright (c) 2025, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <errno.h>

#include <common/debug.h>
#include <common/fdt_wrappers.h>
#include <drivers/delay_timer.h>
#include <drivers/generic_delay_timer.h>
#include <drivers/st/stm32mp2_clk.h>
#include <drivers/st/stm32mp_clkfunc.h>
#include <lib/mmio.h>
#include <lib/spinlock.h>
#include <plat/common/platform.h>
#include <platform_def.h>

#include "ca35ss_clk_svc.h"
#include "scmi_private.h"
#include <stm32mp2_smc.h>
#include <stm32mp_common.h>
#include <stm32mp_svc_setup.h>

#define OPP_MAX_NB			4

#define HW_NO_OVERDRIVE			BIT_32(0)
#define HW_SUPPORTS_OVERDRIVE		BIT_32(1)

#define PART_NUMBER_SUPPORT_OVERDRIVE	BIT_32(31) /* stm32mp2xxD or stm32mp2xxF */

#define TIMEOUT_10MS_IN_US		10000U

#if STM32MP23 || STM32MP25
#define VOLTD_SCMI_STPMIC2_BUCK1	10U
#define VOLTD_SCMI_STPMIC2_BUCK		VOLTD_SCMI_STPMIC2_BUCK1
#elif STM32MP21
#define VOLTD_SCMI_STPMIC2_BUCK3	7U
#define VOLTD_SCMI_STPMIC2_BUCK		VOLTD_SCMI_STPMIC2_BUCK3
#endif

enum stm32_ca35ss_clk_svc_function {
	CA35SS_CLK_SVC_NO_FUNCTION,
	CA35SS_CLK_SVC_SET_RATE,
	CA35SS_CLK_SVC_SET_RATE_STATUS,
	CA35SS_CLK_SVC_RECALC_RATE,
	CA35SS_CLK_SVC_ROUND_RATE,
	CA35SS_CLK_SVC_NB_FUNCTION,
};

enum stm32_ca35ss_clk_svc_state {
	CA35SS_CLK_SVC_STATE_NO_STATE,
	CA35SS_CLK_SVC_STATE_BASE,
	CA35SS_CLK_SVC_STATE_GET_VOLT_POLL,
	CA35SS_CLK_SVC_STATE_SET_VOLT_POLL,
	CA35SS_CLK_SVC_STATE_NB_STATE,
};

struct ca35ss_state
{
	int32_t state;
	int32_t target_opp_idx;
	bool pll1_done;
};

struct ca35ss_opp
{
	uint64_t hz[OPP_MAX_NB];
	uint32_t uvolt[OPP_MAX_NB];
	uint32_t supported_hw[OPP_MAX_NB];
	int32_t opp_nb;
};

static struct ca35ss_opp opp;
static struct ca35ss_state state;

/*
 * There is no IRQ in TF-A BL31 and since TF-A BL31 is not preemptible, we chose
 * not to poll from BL31. Therefore, we poll from Linux via an SMC and we use a
 * state machine in TF-A BL31 to know what step of the DVFS we are at.
 * The following sequence diagram shows the implementation with the associated
 * states.
 *
 * ,-----.          ,----.          ,--------.          ,----.
 * |Linux|          |TF-A|          |Sh. Mem.|          |TF-M|    STATE
 * `--+--'          `-+--'          `---+----'          `-+--'
 *    |  set_rate()   |                 |                 |       BASE
 *    | ------------> |                 |                 |       BASE
 *    |               |     get_volt    |                 |       GET_VOLT_POLL
 *    |               | --------------> |                 |       GET_VOLT_POLL
 *    |               |              Doorbell             |       GET_VOLT_POLL
 *    |               | --------------------------------> |       GET_VOLT_POLL
 *    |   ON_GOING    |                 |                 |       GET_VOLT_POLL
 *    | < - - - - - - |                 |                 |       GET_VOLT_POLL
 *                      [POLLING UNTIL CHANNEL IS FREE]
 *    |    poll()     |                 |                 |       GET_VOLT_POLL
 *    | ------------> |                 |                 |       GET_VOLT_POLL
 *    |               |   Read voltage  |                 |       GET_VOLT_POLL
 *    |               | --------------> |                 |       GET_VOLT_POLL
 *    |               |       volt      |                 |       GET_VOLT_POLL
 *    |               | < - - - - - - - |                 |       GET_VOLT_POLL
 *    |               |     set_volt    |                 |       SET_VOLT_POLL
 *    |               | --------------> |                 |       SET_VOLT_POLL
 *    |               |              Doorbell             |       SET_VOLT_POLL
 *    |               | --------------------------------> |       SET_VOLT_POLL
 *    |   ON_GOING    |                 |                 |       SET_VOLT_POLL
 *    |<- - - - - - - |                 |                 |       SET_VOLT_POLL
 *                      [POLLING UNTIL CHANNEL IS FREE]
 *    |    poll()     |                 |                 |       SET_VOLT_POLL
 *    | ------------> |                 |                 |       SET_VOLT_POLL
 *    |      OK       |                 |                 |       BASE
 *    | < - - - - - - |                 |                 |       BASE
 *
 * Hence, from Linux the view is as simple as a set_rate() followed by a status
 * polling and the whole voltage/rate synchronization is done in TF-A.
 *
 */

static void ca35ss_set_state_base(void)
{
	state.state = CA35SS_CLK_SVC_STATE_BASE;
	state.target_opp_idx = -1;
	state.pll1_done = false;
}

/* Device tree parsing functions */
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
			   uint32_t opp_uvolt[OPP_MAX_NB],
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
				      &opp_uvolt[opp_idx]);
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

/* SCMI helper functions */
/**
 * @brief Find the operating point index with the closest rate to target_rate.
 *
 * @param uint64_t target_rate The target rate.
 * @return uint32_t The index of the closest OPP.
 *
 */
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

/**
 * @brief Get the voltage level for the CPU domain. This function starts the
 * operation: use the vddcpu_get_uvolt_poll() function to poll the result.
 * @warning The caller expects the channel is ready for transfer unless what
 * the function returns STM32_SMC_FAILED.
 *
 * @param uvolt Pointer to store the voltage level in microvolts.
 * @return uint32_t Status of the operation.
 *
 */
static uint32_t vddcpu_get_uvolt(void)
{
	if (scmi_channel_error()) {
		scmi_channel_clear();
		return STM32_SMC_FAILED;
	}

	if (scmi_channel_busy()) {
		return STM32_SMC_FAILED;
	}

	scmi_voltd_level_get_snd(VOLTD_SCMI_STPMIC2_BUCK);
	return STM32_SMC_ON_GOING;
}

/**
 * @brief Poll for the result after the operation was started using the
 * vddcpu_get_uvolt() function.
 *
 * @param uvolt Pointer to store the voltage level in microvolts.
 * @return uint32_t Status of the operation.
 *
 */
static uint32_t vddcpu_get_uvolt_poll(int32_t *uvolt)
{
	int32_t scmi_status;

	if (scmi_channel_error()) {
		scmi_channel_clear();
		return STM32_SMC_FAILED;
	}

	if (scmi_channel_busy()) {
		return STM32_SMC_ON_GOING;
	}

	scmi_status = scmi_voltd_level_get_rcv(uvolt);

	if ((scmi_status != SCMI_SUCCESS) || (*uvolt < 0)) {
		return STM32_SMC_FAILED;
	} else {
		return STM32_SMC_OK;
	}
}

/**
 * @brief Set the voltage level for the CPU domain. This function starts the
 * operation: use the vddcpu_set_uvolt_poll() function to poll the result.
 * @warning The caller expects the channel is ready for transfer unless what
 * the function returns STM32_SMC_FAILED.
 *
 * @param opp_idx Index of the operating performance point.
 * @return uint32_t Status of the operation.
 *
 */
static uint32_t vddcpu_set_uvolt(int32_t opp_idx)
{
	if (scmi_channel_error()) {
		scmi_channel_clear();
		return STM32_SMC_FAILED;
	}

	if ((opp_idx < 0) || (opp.opp_nb < opp_idx) || (scmi_channel_busy())) {
		return STM32_SMC_FAILED;
	}

	scmi_voltd_level_set_snd(VOLTD_SCMI_STPMIC2_BUCK, 0,
				 opp.uvolt[opp_idx]);
	return STM32_SMC_ON_GOING;
}

/**
 * @brief Poll for the result after the operation was started using the
 * vddcpu_set_uvolt() function.
 *
 * @return uint32_t Status of the operation.
 *
 */
static uint32_t vddcpu_set_uvolt_poll(void)
{
	if (scmi_channel_error()) {
		scmi_channel_clear();
		return STM32_SMC_FAILED;
	}

	if (scmi_channel_busy()) {
		return STM32_SMC_ON_GOING;
	}

	if (scmi_voltd_level_set_rcv() == SCMI_SUCCESS) {
		return STM32_SMC_OK;
	} else {
		return STM32_SMC_FAILED;
	}
}

/* SMC handler and setup functions */
uint32_t ca35ss_clk_svc_setup(void)
{
	int32_t opp_idx;
	int32_t err = 0;
	uint32_t voltd_version;
	uint64_t timeout_ref;
	uint32_t opp_hw_filter = get_opp_hw_filter();

	/* Read the DT to find available configurations */
	opp.opp_nb = 0;
	state.state = CA35SS_CLK_SVC_STATE_NO_STATE;
	err = dt_read_opp(&opp.opp_nb, opp.hz, opp.uvolt, opp.supported_hw);
	if ((err != 0) || (opp.opp_nb < 0)) {
		if (opp.opp_nb < 0) {
			/*
			 * OPP is disabled in the DT. This is not a fail, the
			 * handlers are simply disabled by not initializing the
			 * state machine (state.state is NO_STATE).
			 *
			 */
			return STM32_SMC_OK;
		}
		ERROR("%s: couldn't parse DT (%d)\n", __func__, err);
		return STM32_SMC_FAILED;
	}

	/* Verify OPP supported hw */
	for (opp_idx = 0; opp_idx < opp.opp_nb; opp_idx++) {
		if (!(opp.supported_hw[opp_idx] | opp_hw_filter)) {
			ERROR("%s: opp-supported-hw (0x%x) doesn't match hw\n",
			      __func__, opp.supported_hw[opp_idx]);
			return STM32_SMC_FAILED;
		}
	}

	/* Init stm32mp2 clock driver for PLL1 */
	err = stm32mp2_pll1_init();
	if (err != 0) {
		ERROR("%s: couldn't init STM32MP2 PLL1 (%d)\n", __func__, err);
		return STM32_SMC_FAILED;
	}

	/* Verify SCMI availlability */
	timeout_ref = timeout_init_us(TIMEOUT_10MS_IN_US);
	while (scmi_channel_busy())
	{
		if (scmi_channel_error()
		    || timeout_elapsed(timeout_ref)) {
			/* Could not get a free channel */
			scmi_channel_clear();
			ERROR("%s: Couldn't get SCMI free channel\n", __func__);
			return STM32_SMC_FAILED;
		}
		udelay(10);
	}
	if (scmi_voltd_protocol_version(&voltd_version) != SCMI_SUCCESS) {
		ERROR("%s: couldn't get SCMI voltd protocol\n", __func__);
		return STM32_SMC_FAILED;
	}

	/* Init state machine */
	ca35ss_set_state_base();

	return STM32_SMC_OK;
}

static uint32_t ca35ss_clk_svc_handler_get_rate(uint32_t *rate)
{
	uint64_t calc_rate = stm32mp2_pll1_recalc_rate();

	if (calc_rate > UINT32_MAX) {
		/* This should not happen since the VCO max rate is 3200 MHz */
		return STM32_SMC_FAILED;
	}

	*rate = (uint32_t)calc_rate;
	return STM32_SMC_OK;
}

static uint32_t ca35ss_clk_svc_handler_round_rate(uint64_t rate,
						  uint32_t *rounded_rate)
{
	int32_t opp_idx = get_closest_opp_idx(rate);

	if (opp_idx >= opp.opp_nb) {
		return STM32_SMC_INVALID_PARAMS;
	}

	if (opp.hz[opp_idx] > UINT32_MAX) {
		return STM32_SMC_FAILED;
	}

	*rounded_rate = (uint32_t)opp.hz[opp_idx];

	return STM32_SMC_OK;
}

static uint32_t ca35ss_clk_svc_handler_set_rate(uint64_t target_rate)
{
	uint32_t status = STM32_SMC_FAILED;
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
		return STM32_SMC_NO_PERM;
	}

	/* Verify the asked target_rate is not the current rate */
	current_rate = stm32mp2_pll1_recalc_rate();
	if (current_rate == target_rate) {
		return STM32_SMC_OK;
	}

	/* Find cfg for a given target_rate */
	opp_idx = get_closest_opp_idx(target_rate);
	if (opp_idx >= opp.opp_nb) {
		return STM32_SMC_INVALID_PARAMS;
	}

	/* Initiate the operation of getting of current vddcpu */
	status = vddcpu_get_uvolt();
	if (status == STM32_SMC_ON_GOING) {
		/* State: base -> get voltage polling */
		state.state = CA35SS_CLK_SVC_STATE_GET_VOLT_POLL;
		state.target_opp_idx = opp_idx;
	}

	return status;
}

static uint32_t ca35ss_clk_svc_handler_poll_get_volt()
{
	uint32_t status;
	int32_t err;
	int32_t current_uvolt;
	int32_t opp_idx = state.target_opp_idx;

	/* Poll the status of the operation of getting of current vddcpu */
	status = vddcpu_get_uvolt_poll(&current_uvolt);
	if (status != STM32_SMC_OK) {
		if (status != STM32_SMC_ON_GOING) {
			/* Back to base state */
			ca35ss_set_state_base();
		}
		return status;
	}

	/* Next step */
	if (current_uvolt >= (int32_t)opp.uvolt[opp_idx]) {
		/* If dicreasing voltage, then frequency scaling comes first */
		err = stm32mp2_pll1_set_rate(opp.hz[opp_idx]);
		if (err != 0) {
			/* Back to base state */
			ca35ss_set_state_base();
			return STM32_SMC_FAILED;
		}

		/* pll1 has been scaled */
		state.pll1_done = true;
	}

	/* Initiate the operation of scaling vddcpu */
	status = vddcpu_set_uvolt(opp_idx);
	if (status != STM32_SMC_ON_GOING) {
		/* Back to base state */
		ca35ss_set_state_base();
	} else {
		/* State: get voltage polling -> set voltage polling */
		state.state = CA35SS_CLK_SVC_STATE_SET_VOLT_POLL;
	}

	return status;
}

static uint32_t ca35ss_clk_svc_handler_poll_set_volt(void)
{
	uint32_t status = STM32_SMC_FAILED;
	uint64_t target_rate = opp.hz[state.target_opp_idx];

	/* Poll the status of the operation of scaling vddcpu */
	status = vddcpu_set_uvolt_poll();

	if (status == STM32_SMC_ON_GOING) {
		return STM32_SMC_ON_GOING;
	}

	if ((status == STM32_SMC_OK) && (!state.pll1_done)) {
		/* Change PLL1 rate if not done yet */
		status = stm32mp2_pll1_set_rate(target_rate);
	}

	/* Back to base state */
	ca35ss_set_state_base();

	return status;
}

static uint32_t ca35ss_clk_svc_handler_set_rate_status(void)
{
	uint32_t status;

	/* Polling SMC: actual action depends on the state */
	switch (state.state)
	{
	case CA35SS_CLK_SVC_STATE_GET_VOLT_POLL:
		status = ca35ss_clk_svc_handler_poll_get_volt();
		break;

	case CA35SS_CLK_SVC_STATE_SET_VOLT_POLL:
		status = ca35ss_clk_svc_handler_poll_set_volt();
		break;

	default:
		/* ILLEGAL */
		status = STM32_SMC_NO_PERM;
	}

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
