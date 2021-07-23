/*
 * Copyright (c) 2018-2019, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef STM32MP1_CLK_H
#define STM32MP1_CLK_H

#include <arch_helpers.h>

enum stm32mp_osc_id {
	_HSI,
	_HSE,
	_CSI,
	_LSI,
	_LSE,
	_I2S_CKIN,
	NB_OSC,
	_UNKNOWN_OSC_ID = 0xFF
};

extern const char *stm32mp_osc_node_label[NB_OSC];

#define PLL1_SETTINGS_VALID_ID	U(0x504C4C31) /* "PLL1" */

int stm32mp1_clk_probe(void);
int stm32mp1_clk_init(uint32_t pll1_freq_mhz);

int stm32mp1_clk_compute_all_pll1_settings(uint32_t buck1_voltage);
void stm32mp1_clk_lp_save_opp_pll1_settings(uint8_t *data, size_t size);
void stm32mp1_clk_lp_load_opp_pll1_settings(uint8_t *data, size_t size);

int stm32mp1_clk_get_maxfreq_opp(uint32_t *freq_mhz, uint32_t *voltage_mv);

bool stm32mp1_rcc_is_secure(void);
bool stm32mp1_rcc_is_mckprot(void);

void stm32mp1_clk_force_enable(unsigned long id);
void stm32mp1_clk_force_disable(unsigned long id);

unsigned long stm32mp_clk_timer_get_rate(unsigned long id);

bool stm32mp1_rtc_get_read_twice(void);

/* SMP protection on RCC registers access */
void stm32mp1_clk_rcc_regs_lock(void);
void stm32mp1_clk_rcc_regs_unlock(void);

unsigned long stm32mp1_clk_rcc2id(unsigned int offset, unsigned int bit);

int stm32mp1_round_opp_khz(uint32_t *freq_khz);
int stm32mp1_set_opp_khz(uint32_t freq_khz);

void stm32mp1_clock_suspend(void);
void stm32mp1_clock_resume(void);

void stm32mp1_clock_stopmode_save(void);
int stm32mp1_clock_stopmode_resume(void);

void restore_clock_pm_context(void);
void save_clock_pm_context(void);

/* "Added by BPI to set ID to valid on reset during sleep" */
void set_clk_pll1_settings_to_valid(void);

void stm32mp1_register_clock_parents_secure(unsigned long id);

void stm32mp1_update_earlyboot_clocks_state(void);

void stm32mp1_dump_clocks_state(void);

#endif /* STM32MP1_CLK_H */
