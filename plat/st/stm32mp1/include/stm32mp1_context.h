/*
 * Copyright (c) 2017-2021, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef STM32MP1_CONTEXT_H
#define STM32MP1_CONTEXT_H

#include <stdint.h>

uint32_t stm32_get_zdata_from_context(void);
void stm32_restore_ddr_training_area(void);

#endif /* STM32MP1_CONTEXT_H */
