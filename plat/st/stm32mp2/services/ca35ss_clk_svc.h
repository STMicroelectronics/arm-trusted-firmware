/*
 * Copyright (c) 2025, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CA35SS_CLK_SVC_H
#define CA35SS_CLK_SVC_H

#if STM32MP_SIP_CA33SS_CLK
uint32_t ca35ss_clk_svc_setup(void);
uint32_t ca35ss_clk_svc_handler(u_register_t x1, u_register_t x2,
				u_register_t x3, u_register_t x4,
				uint32_t *ret2, bool *ret2_enabled);
#else
static inline uint32_t ca35ss_clk_svc_setup(void)
{
    return 0U;
}

static inline uint32_t ca35ss_clk_svc_handler(u_register_t x1, u_register_t x2,
                          		      u_register_t x3, u_register_t x4,
                          		      uint32_t *ret2,
					      bool *ret2_enabled)
{
    return STM32_SMC_NOT_SUPPORTED;
}
#endif /* STM32MP_SIP_CA33SS_CLK */

#endif /* CA35SS_CLK_SVC_H */
