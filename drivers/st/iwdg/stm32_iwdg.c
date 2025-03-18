/*
 * Copyright (c) 2017-2025, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <errno.h>
#include <string.h>

#include <libfdt.h>

#include <platform_def.h>

#include <arch_helpers.h>
#include <common/debug.h>
#include <drivers/arm/gicv2.h>
#include <drivers/clk.h>
#include <drivers/delay_timer.h>
#include <drivers/st/stm32_iwdg.h>
#include <drivers/st/stm32mp_clkfunc.h>
#include <lib/mmio.h>
#include <lib/utils.h>
#include <plat/common/platform.h>

/* IWDG registers offsets */
#define IWDG_KR_OFFSET		0x00U
#define IWDG_PR_OFFSET		0x04U
#define IWDG_RLR_OFFSET		0x08U

/* Registers values */
#define IWDG_KR_EN_WRITE_KEY	0x5555U
#define IWDG_KR_RELOAD_KEY	0xAAAAU
#define IWDG_KR_ENABLE_KEY	0xCCCCU

#define IWDG_PRESCALER_MIN	2U
#define IWDG_PRESCALER_MAX	10U
#define IWDG_RLR_BITS		12U

struct stm32_iwdg_instance {
	uintptr_t base;
	unsigned long clk_lsi;
	unsigned long clk_pclk;
	unsigned long timeout;
};

static struct stm32_iwdg_instance stm32_iwdg[IWDG_MAX_INSTANCE];

static unsigned int ilog2(unsigned int val)
{
	unsigned int res = 0U;

	while (val > 1U) {
		val >>= 1U;
		res++;
	}

	return res;
}

static int fdt_get_clk_by_name(const void *dtb, int node, const char *name,
			       unsigned long *clk)
{
	const fdt32_t *cuint;
	int index, len;

	index = fdt_stringlist_search(dtb, node, "clock-names", name);
	if (index < 0) {
		return index;
	}

	cuint = fdt_getprop(dtb, node, "clocks", &len);
	if (cuint == NULL) {
		return -FDT_ERR_NOTFOUND;
	}

	if (len < (2 * (index + 1) * (int)sizeof(uint32_t))) {
		return -FDT_ERR_NOTFOUND;
	}

	*clk = fdt32_to_cpu(cuint[(2 * index) + 1]);

	return 0;
}

static int stm32_iwdg_get_dt_node(struct dt_node_info *info, int offset)
{
	int node;

	node = dt_get_node(info, offset, DT_IWDG_COMPAT);
	if (node < 0) {
		if (offset == -1) {
			VERBOSE("%s: No IDWG found\n", __func__);
		}
		return -FDT_ERR_NOTFOUND;
	}

	return node;
}

void stm32_iwdg_refresh(void)
{
	uint8_t i;

	for (i = 0U; i < IWDG_MAX_INSTANCE; i++) {
		struct stm32_iwdg_instance *iwdg = &stm32_iwdg[i];

		/* 0x00000000 is not a valid address for IWDG peripherals */
		if (iwdg->base != 0U) {
			clk_enable(iwdg->clk_pclk);

			mmio_write_32(iwdg->base + IWDG_KR_OFFSET,
				      IWDG_KR_RELOAD_KEY);

			clk_disable(iwdg->clk_pclk);
		}
	}
}

static void stm32_iwdg_start(struct stm32_iwdg_instance *iwdg)
{
	unsigned long lsi_rate;
	unsigned long long reload;
	uint32_t rlr, pr;

	/* 0x00000000 is not a valid address for IWDG peripherals */
	if (iwdg->base == 0U) {
		panic();
	}

	lsi_rate = clk_get_rate(iwdg->clk_lsi);
	if (lsi_rate == 0U) {
		panic();
	}

	reload = iwdg->timeout * (unsigned long long)lsi_rate;
	if ((reload >> (IWDG_RLR_BITS + IWDG_PRESCALER_MAX)) != 0ULL) {
		/* use max timeout */
		pr = IWDG_PRESCALER_MAX - IWDG_PRESCALER_MIN;
		rlr = GENMASK_32(IWDG_RLR_BITS - 1U, 0U);
	} else if ((reload >> IWDG_PRESCALER_MIN) <= 2ULL) {
		/* Be safe and expect any counter to be above 2 */
		panic();
	} else {
		unsigned int bits = ilog2((unsigned int)reload) + 1U;

		if (bits < (IWDG_RLR_BITS + IWDG_PRESCALER_MIN)) {
			pr = 0U;
		} else {
			pr = bits - IWDG_RLR_BITS - IWDG_PRESCALER_MIN;
		}

		rlr = (uint32_t)(reload >> (pr + IWDG_PRESCALER_MIN)) - 1U;
	}

	clk_enable(iwdg->clk_pclk);

	mmio_write_32(iwdg->base + IWDG_KR_OFFSET, IWDG_KR_EN_WRITE_KEY);
	mmio_write_32(iwdg->base + IWDG_PR_OFFSET, pr);
	mmio_write_32(iwdg->base + IWDG_RLR_OFFSET, rlr);
	mmio_write_32(iwdg->base + IWDG_KR_OFFSET, IWDG_KR_ENABLE_KEY);

	clk_disable(iwdg->clk_pclk);
}

int stm32_iwdg_init(void)
{
	int node = -1;
	struct dt_node_info dt_info;
	void *fdt;
	uint32_t __unused count = 0;

	if (fdt_get_address(&fdt) == 0) {
		panic();
	}

	for (node = stm32_iwdg_get_dt_node(&dt_info, node);
	     node != -FDT_ERR_NOTFOUND;
	     node = stm32_iwdg_get_dt_node(&dt_info, node)) {
		struct stm32_iwdg_instance *iwdg;
		const fdt32_t *cuint;
		uint32_t hw_init;
		uint32_t idx;
		int ret;

		count++;

		idx = stm32_iwdg_get_instance(dt_info.base);
		iwdg = &stm32_iwdg[idx];
		iwdg->base = dt_info.base;

		/* Read OTP to know if IWDG is started by hardware or not */
		hw_init = stm32_iwdg_get_otp_config(idx);

		if ((hw_init & IWDG_HW_ENABLED) != 0) {
			if (dt_info.status == DT_DISABLED) {
				ERROR("OTP enabled but iwdg%u DT-disabled\n",
				      idx + 1U);
				panic();
			}
		}

		if (dt_info.status == DT_DISABLED) {
			zeromem((void *)iwdg,
				sizeof(struct stm32_iwdg_instance));
			continue;
		}

		ret = fdt_get_clk_by_name(fdt, node, "pclk", &iwdg->clk_pclk);
		if (ret != 0) {
			return ret;
		}

		ret = fdt_get_clk_by_name(fdt, node, "lsi", &iwdg->clk_lsi);
		if (ret != 0) {
			return ret;
		}

		cuint = fdt_getprop(fdt, node, "timeout-sec", NULL);
		if (cuint == NULL) {
			ERROR("iwdg%u: missing timeout-sec property",
			      idx + 1U);
			panic();
		}
		iwdg->timeout = fdt32_to_cpu(*cuint);

		VERBOSE("IWDG%u found, %ssecure\n", idx + 1U,
			((dt_info.status & DT_NON_SECURE) != 0) ?
			"non-" : "");

		if ((dt_info.status & DT_NON_SECURE) != 0) {
			stm32mp_register_non_secure_periph_iomem(iwdg->base);
		} else {
			stm32mp_register_secure_periph_iomem(iwdg->base);
		}

		clk_enable(iwdg->clk_lsi);
		stm32_iwdg_start(iwdg);
	}

	VERBOSE("%u IWDG instance%s found\n", count, (count > 1U) ? "s" : "");

	return 0;
}
