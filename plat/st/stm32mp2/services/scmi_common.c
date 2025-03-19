/*
 * Copyright (c) 2025, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <drivers/delay_timer.h>
#include <drivers/scmi-msg.h>

#include "scmi_private.h"

#define TIMEOUT_10MS_IN_US		10000U

#define IPCC_CHANNEL15			14U
#define IPCC_CHANNEL			IPCC_CHANNEL15
#define IPCC1_C1SCR			(IPCC1_BASE + 0x008U)

#define SCMI_HEADER_PROTOCOL_SHIFT	10U
#define SCMI_CHANNEL_FLAGS		0

#define SCMI_PROTO_VOLTD		0x17U
#define SCMI_MSG_PROTOCOL_VERSION	0x0U
#define SCMI_MSG_VOLTAGE_LEVEL_SET	0x7U
#define SCMI_MSG_VOLTAGE_LEVEL_GET	0x8U

#define SCMI_HEADER_VOLTD_PROTO_VERSION	((SCMI_PROTO_VOLTD << SCMI_HEADER_PROTOCOL_SHIFT) \
					| SCMI_MSG_PROTOCOL_VERSION)
#define SCMI_HEADER_VOLTD_LEVEL_SET	((SCMI_PROTO_VOLTD << SCMI_HEADER_PROTOCOL_SHIFT) \
					| SCMI_MSG_VOLTAGE_LEVEL_SET)
#define SCMI_HEADER_VOLTD_LEVEL_GET	((SCMI_PROTO_VOLTD << SCMI_HEADER_PROTOCOL_SHIFT) \
					| SCMI_MSG_VOLTAGE_LEVEL_GET)

#define SCMI_CHANNEL_BUSY(ch_status)	(((ch_status) & BIT_32(0)) == 0)
#define SCMI_CHANNEL_ERROR(ch_status)	(((ch_status) & BIT_32(1)) == BIT_32(1))

struct scmi_shmem_layout {
	uint32_t reserved0;
	uint32_t channel_status;
	uint32_t reserved2;
	uint32_t reserved3;
	uint32_t channel_flags;
	uint32_t length;
	uint32_t message_header;
	uint32_t payload[SCMI_MAX_MESSAGE_PAYLOAD_SIZE];
};

static struct scmi_shmem_layout *shmem = (struct scmi_shmem_layout *)STM32MP_SCMI_SEC_SHMEM_BASE;

/**
 * @brief Ring the SCMI doorbell to notify the remote processor.
 *
 */
void scmi_ring_doorbell(void)
{
	*(volatile uint32_t *)IPCC1_C1SCR = BIT_32(IPCC_CHANNEL + 16U);
}

/**
 * @brief Clear the SCMI response.
 *
 */
void scmi_channel_clear(void)
{
	*(volatile uint32_t *)IPCC1_C1SCR = BIT_32(IPCC_CHANNEL);
        shmem->channel_status = 1U;
}

/**
 * @brief Is the SCMI channel busy.
 *
 */
bool scmi_channel_busy(void)
{
        return SCMI_CHANNEL_BUSY(shmem->channel_status);
}

/**
 * @brief Is the SCMI channel in an error state.
 *
 */
bool scmi_channel_error(void)
{
        return SCMI_CHANNEL_ERROR(shmem->channel_status);
}

/**
 * @brief Get the SCMI voltage domain protocol version.
 *
 * @param The protocol version.
 * @return The status of the operation.
 *
 */
int32_t scmi_voltd_protocol_version(uint32_t *version)
{
	uint64_t timeout_ref;

	shmem->channel_status = 0U;
	shmem->channel_flags = SCMI_CHANNEL_FLAGS;
	shmem->length = sizeof(uint32_t);
	shmem->message_header = SCMI_HEADER_VOLTD_PROTO_VERSION;

	scmi_ring_doorbell();

	timeout_ref = timeout_init_us(TIMEOUT_10MS_IN_US);
	while (SCMI_CHANNEL_BUSY(shmem->channel_status)
	       && !SCMI_CHANNEL_ERROR(shmem->channel_status))
	{
		if (timeout_elapsed(timeout_ref)) {
			return SCMI_ST_TIMEOUT;
		}
		udelay(10);
	}

	if (SCMI_CHANNEL_ERROR(shmem->channel_status)) {
		shmem->channel_status = 0U;
		return SCMI_COMMS_ERROR;
	}

	scmi_channel_clear();

        if (shmem->length != (3U * sizeof(uint32_t))) {
                return SCMI_COMMS_ERROR;
        }

	*version = shmem->payload[1];
	return (int32_t)shmem->payload[0]; /* status */
}

/**
 * @brief Send a request to get the voltage level of a domain.
 *
 * @param domain_id The ID of the voltage domain.
 *
 */
void scmi_voltd_level_get_snd(uint32_t domain_id)
{
	shmem->channel_status = 0U;
	shmem->channel_flags = SCMI_CHANNEL_FLAGS;
	shmem->length = 2U * sizeof(uint32_t);
	shmem->message_header = SCMI_HEADER_VOLTD_LEVEL_GET;

	shmem->payload[0] = domain_id;
	scmi_ring_doorbell();
}

/**
 * @brief Read the response for the voltage level get request.
 *
 * @param voltage_level Pointer to store the voltage level.
 * @return The status of the operation.
 *
 */
int32_t scmi_voltd_level_get_rcv(int32_t *voltage_level)
{
	scmi_channel_clear();

        if (shmem->length != (3U * sizeof(uint32_t))) {
                return SCMI_COMMS_ERROR;
        }

	*voltage_level = shmem->payload[1];
	return (int32_t)shmem->payload[0]; /* status */
}

/**
 * @brief Send a request to set the voltage level of a domain.
 *
 * @param domain_id The ID of the voltage domain.
 * @param flags Flags for the operation.
 * @param voltage_level The voltage level to set.
 *
 */
void scmi_voltd_level_set_snd(uint32_t domain_id, uint32_t flags,
                              int32_t voltage_level)
{
	shmem->channel_status = 0U;
	shmem->channel_flags = SCMI_CHANNEL_FLAGS;
	shmem->length = 4U * sizeof(uint32_t);
	shmem->message_header = SCMI_HEADER_VOLTD_LEVEL_SET;

	shmem->payload[0] = domain_id;
	shmem->payload[1] = flags;
	shmem->payload[2] = (uint32_t)voltage_level;

	scmi_ring_doorbell();
}

/**
 * @brief Read the response for the voltage level set request.
 *
 * @return The status of the operation.
 *
 */
int32_t scmi_voltd_level_set_rcv()
{
	scmi_channel_clear();

        if (shmem->length != (2U * sizeof(uint32_t))) {
                return SCMI_COMMS_ERROR;
        }

	return (int32_t)shmem->payload[0]; /* status */
}
