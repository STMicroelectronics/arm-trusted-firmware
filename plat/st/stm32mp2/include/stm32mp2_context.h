/*
 * Copyright (c) 2023, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef STM32MP2_CONTEXT_H
#define STM32MP2_CONTEXT_H

#include <stdint.h>

#include <lib/psci/psci.h>

void stm32mp_pm_save_enc_mkey_in_context(uint8_t *data);
void stm32mp_pm_get_enc_mkey_from_context(uint8_t *data);

void stm32_pm_context_init(void);
bool stm32_pm_context_is_valid(void);
void stm32_pm_context_save(const psci_power_state_t *state, const void *data, size_t size);
void stm32_pm_context_restore(void *data, size_t size);
void stm32_pm_context_clear(void);

#endif /* STM32MP2_CONTEXT_H */
