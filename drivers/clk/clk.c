/*
 * Copyright (c) 2021-2025, STMicroelectronics - All Rights Reserved
 * Author(s): Ludovic Barre, <ludovic.barre@st.com> for STMicroelectronics.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <errno.h>
#include <stdbool.h>

#include <drivers/clk.h>

/* Maximum number of clock providers */
#ifdef CFG_CLK_PROVIDER_NB
#define MAX_CLOCK_PROVIDERS CFG_CLK_PROVIDER_NB
#else
#define MAX_CLOCK_PROVIDERS 15U
#endif

struct clock_provider_t {
	const struct clk_ops *ops;
	uint16_t phandle;
};

static struct clock_providers_t {
	struct clock_provider_t providers[MAX_CLOCK_PROVIDERS];
	uint8_t nb;
	uint8_t def_provider; /* Index of the default provider */
} clk_providers;

static const struct clk_ops *get_ops(unsigned long id)
{
	uint16_t phandle = (id & CLK_PHANDLE_MASK) >> CLK_PHANDLE_SHIFT;
	const struct clk_ops *ops = NULL;
	uint8_t i;
	bool found = false;

	/* If no phandle is provided, use the default ops */
	if (phandle != 0U) {
		/* Check if phandle is registered in the clock providers list */
		for (i = 0U; i < clk_providers.nb; i++) {
			if (clk_providers.providers[i].phandle == phandle) {
				ops = clk_providers.providers[i].ops;
				found = true;
				break;
			}
		}
	}

	if (!found) {
		ops = clk_providers.providers[clk_providers.def_provider].ops;
	}

	/* If no ops found, panic */
	if (ops == NULL) {
		ERROR("Clock provider not found for phandle %u\n", phandle);
		panic();
	}

	return ops;
}


int clk_enable(unsigned long id)
{
	const struct clk_ops *ops = get_ops(id);

	return ops->enable(id & CLK_BINDING_ID_MASK);
}

void clk_disable(unsigned long id)
{
	const struct clk_ops *ops = get_ops(id);

	ops->disable(id & CLK_BINDING_ID_MASK);
}

unsigned long clk_get_rate(unsigned long id)
{
	const struct clk_ops *ops = get_ops(id);

	return ops->get_rate(id & CLK_BINDING_ID_MASK);
}

int clk_set_rate(unsigned long id, unsigned long rate, unsigned long *orate)
{
	const struct clk_ops *ops = get_ops(id);
	unsigned long lrate;

	if (orate != NULL) {
		return ops->set_rate(id & CLK_BINDING_ID_MASK, rate, orate);
	}

	/* In case the caller is not interested in the output rate */
	return ops->set_rate(id & CLK_BINDING_ID_MASK, rate, &lrate);
}

int clk_get_parent(unsigned long id)
{
	const struct clk_ops *ops = get_ops(id);

	return ops->get_parent(id & CLK_BINDING_ID_MASK);
}

int clk_set_parent(unsigned long id, unsigned long parent_id)
{
	const struct clk_ops *ops = get_ops(id);

	return ops->set_parent(id & CLK_BINDING_ID_MASK, parent_id);
}

bool clk_is_enabled(unsigned long id)
{
	const struct clk_ops *ops = get_ops(id);

	return ops->is_enabled(id & CLK_BINDING_ID_MASK);
}

void clk_add_provider(uint32_t phandle, const struct clk_ops *ops_ptr)
{
	assert((ops_ptr != NULL) &&
	       (ops_ptr->enable != NULL) &&
	       (ops_ptr->disable != NULL) &&
	       (ops_ptr->get_rate != NULL) &&
	       (ops_ptr->get_parent != NULL) &&
	       (ops_ptr->is_enabled != NULL));

	if (clk_providers.nb >= MAX_CLOCK_PROVIDERS) {
		ERROR("No more room for clock providers");
		panic();
	}

	clk_providers.providers[clk_providers.nb].phandle = phandle;
	clk_providers.providers[clk_providers.nb].ops = ops_ptr;
	clk_providers.nb++;
}

void clk_add_default_provider(uint32_t phandle, const struct clk_ops *ops_ptr)
{
	clk_add_provider(phandle, ops_ptr);

	clk_providers.def_provider = clk_providers.nb - 1;
}

/*
 * Initialize the clk. The fields in the provided clk
 * ops pointer must be valid.
 */
void clk_register(const struct clk_ops *ops_ptr)
{
	clk_add_default_provider(0, ops_ptr);
}
