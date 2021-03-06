/*
 * Copyright (c) 2017-2020, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef STM32MP1_SHARED_RESOURCES_H
#define STM32MP1_SHARED_RESOURCES_H

#include <drivers/st/etzpc.h>

#include <stm32mp_shared_resources.h>

#define STM32MP1_SHRES_GPIOZ(i)		(STM32MP1_SHRES_GPIOZ_0 + (i))

enum stm32mp_shres {
	STM32MP1_SHRES_CRYP1,
	STM32MP1_SHRES_GPIOZ_0,
	STM32MP1_SHRES_GPIOZ_1,
	STM32MP1_SHRES_GPIOZ_2,
	STM32MP1_SHRES_GPIOZ_3,
	STM32MP1_SHRES_GPIOZ_4,
	STM32MP1_SHRES_GPIOZ_5,
	STM32MP1_SHRES_GPIOZ_6,
	STM32MP1_SHRES_GPIOZ_7,
	STM32MP1_SHRES_HASH1,
	STM32MP1_SHRES_I2C4,
	STM32MP1_SHRES_I2C6,
	STM32MP1_SHRES_IWDG1,
	STM32MP1_SHRES_MCU,
	STM32MP1_SHRES_MDMA,
	STM32MP1_SHRES_PLL3,
	STM32MP1_SHRES_RNG1,
	STM32MP1_SHRES_RTC,
	STM32MP1_SHRES_SPI6,
	STM32MP1_SHRES_USART1,

	STM32MP1_SHRES_COUNT
};

#ifdef STM32MP_SHARED_RESOURCES
/*
 * Register a (non-)secure peripheral based on the ETZPC DECPROT configuration
 */
void stm32mp1_register_etzpc_decprot(unsigned int id,
				     enum etzpc_decprot_attributes attr);
#else
static inline
void stm32mp1_register_etzpc_decprot(unsigned int id,
				     enum etzpc_decprot_attributes attr)
{
}
#endif

#endif /* STM32MP1_SHARED_RESOURCES_H */
