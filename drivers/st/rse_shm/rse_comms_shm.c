/*
 * Copyright (c) 2025, Arm Limited and Contributors. All rights reserved.
 * Copyright (c) 2025, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include <common/debug.h>
#include <drivers/arm/rse_comms.h>
#include <drivers/delay_timer.h>
#include <drivers/st/rse_shm.h>
#include <lib/mmio.h>
#include <lib/utils_def.h>

#include <platform_def.h>
/*
 *  In size from struct stm32_rse_shmem bit 31 from is reserved
 *  for sending without mailbox ack. When bit is set, the request
 *  is complete when bit is cleared.
 */
#define FLAG_SIZE_NO_MBOX_ACK BIT_32(31)
#ifndef RSE_COMMS_TIMEOUT_US
#error "RSE_COMMS_TIMEOUT_US not defined by platform_defs.h"
#endif
#ifndef RSE_COMMS_IPCC_CHAN
#error "RSE_COMMS_IPCC_CHAN not defined by platform_defs.h"
#endif

struct stm32_rse_shmem {
	uint32_t size;
	uint8_t payload[];
};

static struct stm32_rse_shmem *shmem;
static size_t shmem_size;

size_t rse_mbx_get_max_message_size(void)
{
	return shmem_size;
}

#define IPCC_C1SCR_CHS(x) BIT_32((x) - 1) << 16
#define IPCC_C1SCR 0x8U

int rse_mbx_send_data(const uint8_t *send_buffer, size_t size)
{
	(void)memcpy(shmem->payload, send_buffer, size);
	mmio_write_32((uintptr_t)(&shmem->size), size | FLAG_SIZE_NO_MBOX_ACK);
	/* raise interrupt to remote m33 */
	mmio_write_32(RSE_COMMS_IPCC_BASE + IPCC_C1SCR, IPCC_C1SCR_CHS(RSE_COMMS_IPCC_CHAN));

	return 0;
}

int rse_mbx_receive_data(uint8_t *receive_buffer, size_t *size)
{
	uint64_t timeout = timeout_init_us(RSE_COMMS_TIMEOUT_US);

	while (mmio_read_32((uintptr_t)(&shmem->size)) & FLAG_SIZE_NO_MBOX_ACK) {
		if (timeout_elapsed(timeout)) {
			ERROR("%s: timeout\n", __func__);
			return -ETIMEDOUT;
		}
	}
	*size = shmem->size;
	(void)memcpy((void *)receive_buffer, shmem->payload, *size);

	return 0;
}

int rse_mbx_init(const void *init_data)
{
	shmem = (struct stm32_rse_shmem *)(((struct rse_shmem *)init_data)->base);
	shmem_size = ((struct rse_shmem *)init_data)->len;

	return 0;
}
