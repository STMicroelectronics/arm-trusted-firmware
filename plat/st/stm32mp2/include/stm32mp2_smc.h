/*
 * Copyright (c) 2024, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef STM32MP2_SMC_H
#define STM32MP2_SMC_H

#define STM32_COMMON_SIP_NUM_CALLS			2U

/*
 * STM32_SIP_SMC_STGEN_SET_RATE call API
 * This service is opened to secure world only.
 *
 * Argument a0: (input) SMCC ID
 *		(output) status return code
 * Argument a1: (input) Frequency to set (given by sender)
 */
#define STM32_SIP_SMC_STGEN_SET_RATE                    0x82000000

/*
 * STM32_SIP_CA35SS_CLK call API
 * This service is opened to secure world only.
 *
 * Argument a0: (input) SMCC ID
 *        (output) status return code
 * Argument a1: (input) CA35SS clock Function ID in one of the following:
 *            1 for CA35SS_CLK_SVC_ROUND_RATE
 *            2 for CA35SS_CLK_SVC_ROUND_RATE_STATUS
 *            3 for CA35SS_CLK_SVC_RECALC_RATE
 *        (output) if status return code is STM32_SMC_OK, the current rate
 * Argument a2: (input) if a1 == CA35SS_CLK_SVC_ROUND_RATE: the target rate
 *                      else: unused
 */
#define STM32_SIP_CA35SS_CLK                            0x82000001

#endif /* STM32MP2_SMC_H */
