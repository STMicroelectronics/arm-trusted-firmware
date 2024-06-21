/*
 * Copyright (c) 2017-2020, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef STM32MP1_DDR_HELPERS_H
#define STM32MP1_DDR_HELPERS_H

#include <stdint.h>

enum stm32mp1_ddr_sr_mode {
	DDR_SR_MODE_INVALID = 0,
	DDR_SSR_MODE,
	DDR_HSR_MODE,
	DDR_ASR_MODE,
};

void ddr_enable_clock(bool ddrphycen);
int ddr_sw_self_refresh_exit(void);
uint32_t ddr_get_io_calibration_val(void);
int ddr_standby_sr_entry(void);
enum stm32mp1_ddr_sr_mode ddr_read_sr_mode(void);
void ddr_set_sr_mode(enum stm32mp1_ddr_sr_mode mode);

#endif /* STM32MP1_DDR_HELPERS_H */
