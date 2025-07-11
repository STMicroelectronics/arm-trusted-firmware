/*
 * Copyright (c) 2026, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <lib/mmio.h>

#include <platform_def.h>

#define CA35SS_SYSCFG_VBAR_CR		0x2084U

void stm32mp_ca35_set_vbar(uintptr_t vbar)
{
	mmio_write_32(A35SSC_BASE + CA35SS_SYSCFG_VBAR_CR, (uint32_t)vbar);
}
