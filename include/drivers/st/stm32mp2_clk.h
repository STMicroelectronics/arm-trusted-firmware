/*
 * Copyright (C) 2024-2026, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef STM32MP2_CLK_H
#define STM32MP2_CLK_H

#include <stdint.h>

enum stm32mp_osc_id {
	_HSI,
	_HSE,
	_CSI,
	_LSI,
	_LSE,
	_I2S_CKIN,
	_SPDIF_SYMB,
	NB_OSC,
	_UNKNOWN_OSC_ID = 0xFF
};

extern const char *stm32mp_osc_node_label[NB_OSC];

enum pll_cfg {
	FBDIV,
	REFDIV,
	POSTDIV1,
	POSTDIV2,
	PLLCFG_NB
};

enum pll_csg {
	DIVVAL,
	SPREAD,
	DOWNSPREAD,
	PLLCSG_NB
};

int stm32mp2_clk_init(void *fdt);
int stm32mp2_pll1_disable(void);
int stm32mp2_pll1_enable(void);

int32_t stm32mp2_pll1_init(void);
int32_t stm32mp2_pll1_check_rate(uint64_t rate);
int32_t stm32mp2_pll1_set_rate(uint64_t rate);
uint64_t stm32mp2_pll1_recalc_rate();

#endif /* STM32MP2_CLK_H */
