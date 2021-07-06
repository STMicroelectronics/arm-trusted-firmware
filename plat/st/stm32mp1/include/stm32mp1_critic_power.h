/*
 * Copyright (C) 2019-2021, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef STM32MP1_CRITIC_POWER_H
#define STM32MP1_CRITIC_POWER_H

extern void stm32_pwr_down_wfi_wrapper(bool is_cstop, uint32_t mode);
extern void stm32_pwr_back_from_stop2(void);
void stm32_pwr_down_wfi_load(bool is_cstop, uint32_t mode);
void stm32_pwr_call_optee_ep(void);
void stm32_pwr_cstop_critic_exit(void);

#endif /* STM32MP1_CRITIC_POWER_H */
