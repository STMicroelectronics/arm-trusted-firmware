/*
 * Copyright (c) 2025-2026, STMicroelectronics - All Rights Reserved
 * Author(s): Gabriel Fernandez, <gabriel.fernandez@st.com> for STMicroelectronics.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <lib/utils_def.h>

#include <common/debug.h>
#include <common/fdt_wrappers.h>
#include <drivers/clk.h>
#include <drivers/clk-fixed.h>

#include <platform_def.h>

#ifdef CFG_CLK_FIXED_NB
#define MAX_FIXED_CLOCK_PROVIDERS CFG_CLK_FIXED_NB
#else
#define MAX_FIXED_CLOCK_PROVIDERS 10U
#endif

struct clock_fixed_provider_t {
	unsigned long rate;
	uint16_t phandle;
};

static struct clock_fixed_providers_t {
	struct clock_fixed_provider_t providers[MAX_FIXED_CLOCK_PROVIDERS];
	uint8_t nb;
} clk_fixed_providers;

static int clk_fixed_enable(unsigned long binding_id)
{
	/* Fixed clock is always enabled */
	return 0;
}

static void clk_fixed_disable(unsigned long binding_id)
{
	/* Fixed clocks are always enabled, so no action needed. */
}

static bool clk_fixed_is_enabled(unsigned long binding_id)
{
	/* Fixed clocks are always enabled. */
	return true;
}

static unsigned long clk_fixed_get_rate(unsigned long binding_id)
{
	/* Binding id is the phandle */
	uint16_t phandle = binding_id;
	uint8_t i;

	for (i = 0U; i < clk_fixed_providers.nb; i++) {
		if (clk_fixed_providers.providers[i].phandle == phandle) {
			return clk_fixed_providers.providers[i].rate;
		}
	}

	return 0UL;
}

static int clk_fixed_get_parent(unsigned long binding_id)
{
	return -1;
}

const struct clk_ops fixed_clock_ops = {
	.enable		= clk_fixed_enable,
	.disable	= clk_fixed_disable,
	.is_enabled	= clk_fixed_is_enabled,
	.get_rate	= clk_fixed_get_rate,
	.get_parent	= clk_fixed_get_parent,
};

static void clk_add_fixed_clock(uint32_t phandle, unsigned long frequency)
{
	if (clk_fixed_providers.nb >= MAX_FIXED_CLOCK_PROVIDERS) {
		EARLY_ERROR("No more room for fixed clock providers\n");
		panic();
	}

	clk_fixed_providers.providers[clk_fixed_providers.nb].phandle = phandle;
	clk_fixed_providers.providers[clk_fixed_providers.nb].rate = frequency;
	clk_fixed_providers.nb++;

	clk_add_provider(phandle, &fixed_clock_ops);
}

void clk_fixed_register(void *fdt)
{
	int node = 0;
	int subnode = 0;
	unsigned long frequency = 0UL;

	node = fdt_path_offset(fdt, "/clocks");
	if (node < 0) {
		return;
	}

	fdt_for_each_subnode(subnode, fdt, node) {
		const fdt32_t *cuint = NULL;

		if (!fdt_node_is_enabled(fdt, subnode)) {
			continue;
		}

		if (fdt_node_check_compatible(fdt, subnode, "fixed-clock")) {
			continue;
		}

		cuint = fdt_getprop(fdt, subnode, "clock-frequency", NULL);
		if (cuint == NULL) {
			frequency = 0UL;
		} else {
			frequency = fdt32_to_cpu(*cuint);
		}

		clk_add_fixed_clock(subnode, frequency);
	}
}
