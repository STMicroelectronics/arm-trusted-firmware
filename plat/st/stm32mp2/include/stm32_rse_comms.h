/*
 * Copyright (c) 2025, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef STM32_RSE_COMMS_H
#define STM32_RSE_COMMS_H

#include <lib/psa/rse_platform_api.h>

/* STM32 RSE IOCTL Specific service */
#define STM32_IOCTL_KEYSHARE_START	2
#define STM32_IOCTL_KEYSHARE_STOP	3

psa_status_t rse_platform_stm32_share_key_start(uint32_t key_id);
psa_status_t rse_platform_stm32_share_key_stop(uint32_t key_id);

#endif
