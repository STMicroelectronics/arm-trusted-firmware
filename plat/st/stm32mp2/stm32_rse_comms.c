/*
 * Copyright (c) 2025, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stm32_rse_comms.h>

psa_status_t rse_platform_stm32_share_key_start(uint32_t key_id)
{
	struct psa_invec in_vec[1];

	in_vec[0].base = &key_id;
	in_vec[0].len = sizeof(key_id);

	return rse_platform_ioctl(STM32_IOCTL_KEYSHARE_START, in_vec, NULL);
}

psa_status_t rse_platform_stm32_share_key_stop(uint32_t key_id)
{
	struct psa_invec in_vec[1];

	in_vec[0].base = &key_id;
	in_vec[0].len = sizeof(key_id);

	return rse_platform_ioctl(STM32_IOCTL_KEYSHARE_STOP, in_vec, NULL);
}
