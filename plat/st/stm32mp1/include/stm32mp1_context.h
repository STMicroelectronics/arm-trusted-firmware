/*
 * Copyright (c) 2017-2022, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef STM32MP1_CONTEXT_H
#define STM32MP1_CONTEXT_H

#include <stdbool.h>
#include <stdint.h>

#if STM32MP_SUPPORT_PM
void stm32_clean_context(void);
void stm32_context_save_bl2_param(void);
uint32_t stm32_get_zdata_from_context(void);
bool stm32_pm_context_is_valid(void);
void stm32_restore_ddr_training_area(void);
uint32_t stm32_pm_get_optee_ep(void);
#if STM32MP13
void stm32mp1_pm_save_mce_mkey_in_context(uint8_t *data);
void stm32mp1_pm_get_mce_mkey_from_context(uint8_t *data);
#endif
#else /* STM32MP_SUPPORT_PM */
static inline uint32_t stm32_get_zdata_from_context(void) {return 0U;}
#endif /* STM32MP_SUPPORT_PM */

#endif /* STM32MP1_CONTEXT_H */
