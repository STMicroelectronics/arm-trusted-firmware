/*
 * Copyright (c) 2025, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <common/debug.h>
#include <drivers/delay_timer.h>
#include <drivers/scmi-msg.h>
#include <drivers/scmi.h>
#include <lib/mmio.h>

#include "scmi_private.h"

#define TIMEOUT_10MS_IN_US		10000U

/* IPCC chanel used for ringbell for SCMI mailbox */
#define IPCC_CHANNEL15			14U
#define IPCC_CHANNEL			IPCC_CHANNEL15
#define IPCC1_C1SCR			(IPCC1_BASE + 0x008U)

/* SCMI message header format bit field */
#define SCMI_HEADER_TOKEN_SHIFT		18U
#define SCMI_HEADER_TOKEN_MASK		GENMASK_32(27, 18)
#define SCMI_HEADER_PROTOCOL_SHIFT	10U
#define SCMI_HEADER_PROTOCOL_MASK	GENMASK_32(17, 10)
#define SCMI_HEADER_MSG_ID_SHIFT	0U
#define SCMI_HEADER_MSG_ID_MASK		GENMASK_32(7, 0)

/* Helper macro to get info from a SCMI message header */
#define SCMI_MSG_GET_TOKEN(_msg)							\
	(((_msg) & SCMI_HEADER_TOKEN_MASK) >> SCMI_HEADER_TOKEN_SHIFT)
#define SCMI_MSG_GET_PROTOCOL(_msg)							\
	(((_msg) & SCMI_HEADER_PROTOCOL_MASK) >> SCMI_HEADER_PROTOCOL_SHIFT)
#define SCMI_MSG_GET_MSG_ID(_msg)							\
	(((_msg) & SCMI_HEADER_MSG_ID_MASK) >> SCMI_HEADER_MSG_ID_SHIFT)

/* SCMI base protocol: mandatory messages IDs for all SCMI protocols */
#define SCMI_MSG_PROTOCOL_VERSION	0x0U
#define SCMI_MSG_PROTOCOL_MSG_ATTR	0x2U

/* SCMI voltage domain protocol message IDs */
#define SCMI_MSG_VOLTAGE_LEVEL_SET	0x7U
#define SCMI_MSG_VOLTAGE_LEVEL_GET	0x8U

/* SCMI system power management protocol message IDs */
#define SCMI_MSG_SYS_PWR_STATE_SET	0x3U
#define SCMI_MSG_SYS_PWR_STATE_GET	0x4U

/* Helper macro on version of a SCMI protocol */
#define SCMI_GET_VER_MAJOR(ver)		(((ver) >> 16) & GENMASK_32(15, 0))
#define SCMI_GET_VER_MINOR(ver)		((ver) & GENMASK_32(15, 0))

#define SCMI_VERSION(maj, min)								\
	((((maj) & GENMASK_32(15, 0)) << 16) | ((min) & GENMASK_32(15, 0)))

/* Expected SCMI protocol version for this driver */
#define SCMI_PROTO_VER_VOLTAGE		SCMI_VERSION(1, 0)
#define SCMI_PROTO_VER_SYS_PWR		SCMI_VERSION(1, 0)

/* Check that the driver's version is same or higher than the reported SCMI version. */
#define is_scmi_version_compatible(drv, scmi)						\
	((SCMI_GET_VER_MAJOR(drv) > SCMI_GET_VER_MAJOR(scmi)) ||			\
	 ((SCMI_GET_VER_MAJOR(drv) == SCMI_GET_VER_MAJOR(scmi)) &&			\
	  (SCMI_GET_VER_MINOR(drv) <= SCMI_GET_VER_MINOR(scmi))))

/* SMT Channel */
#define SCMI_CHANNEL_FLAGS_POLL		0

#define SCMI_CHANNEL_STATUS_ERROR	BIT_32(1)
#define SCMI_CHANNEL_STATUS_FREE	BIT_32(0)

#define SCMI_SHMEM_PAYLOAD_OFFSET	(7 * sizeof(uint32_t))
#define SCMI_MAX_MESSAGE_PAYLOAD_SIZE	((SMT_BUF_SLOT_SIZE - SCMI_SHMEM_PAYLOAD_OFFSET) \
					/ sizeof(uint32_t))

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

unsigned int scmi_msg_token;

bool scmi_sys_pwr_state_set_supported;

/**
 * @brief Ring the SCMI doorbell to notify the remote processor.
 *
 */
static void scmi_ring_doorbell(void)
{
	mmio_write_32(IPCC1_C1SCR, BIT_32(IPCC_CHANNEL + 16U));
}

/**
 * @brief Clear the SCMI response.
 *
 */
void scmi_channel_clear(void)
{
	mmio_write_32(IPCC1_C1SCR, BIT_32(IPCC_CHANNEL));
	shmem->channel_status = SCMI_CHANNEL_STATUS_FREE;
}

/**
 * @brief Is the SCMI channel busy.
 *
 */
bool scmi_channel_busy(void)
{
	return (shmem->channel_status & SCMI_CHANNEL_STATUS_FREE) == 0;
}

/**
 * @brief Is the SCMI channel in an error state.
 *
 */
bool scmi_channel_error(void)
{
	return (shmem->channel_status & SCMI_CHANNEL_STATUS_ERROR) != 0;
}

/**
 * @brief Verify SCMI availlability
 *
 */
static int32_t scmi_channel_check(void)
{
	uint64_t timeout_ref = timeout_init_us(TIMEOUT_10MS_IN_US);

	while (scmi_channel_busy()) {
		if (scmi_channel_error() || timeout_elapsed(timeout_ref)) {
			/* Could not get a free channel */
			scmi_channel_clear();
			ERROR("%s: Couldn't get SCMI free channel\n", __func__);
			return SCMI_ST_TIMEOUT;
		}
		udelay(10);
	}

	return SCMI_SUCCESS;
}

/**
 * @brief Verify SCMI availlability
 *
 */
static int32_t scmi_channel_prepare(uint32_t protocol_id, uint32_t msg_id, uint32_t size)
{
	int32_t ret = scmi_channel_check();

	if (ret != SCMI_SUCCESS) {
		return ret;
	}

	shmem->channel_status = 0U;
	shmem->channel_flags = SCMI_CHANNEL_FLAGS_POLL;
	shmem->length = (1 + size) * sizeof(uint32_t); /* heaader and payload */
	shmem->message_header =
		(((protocol_id) << SCMI_HEADER_PROTOCOL_SHIFT)  & SCMI_HEADER_PROTOCOL_MASK) |
		(((msg_id) << SCMI_HEADER_MSG_ID_SHIFT) & SCMI_HEADER_MSG_ID_MASK) |
		(((++scmi_msg_token) << SCMI_HEADER_TOKEN_SHIFT) & SCMI_HEADER_TOKEN_MASK);

	return SCMI_SUCCESS;
}

/**
 * @brief Check the SCMI answer
 *
 */
static int32_t scmi_rsp_check(uint32_t size)
{
	scmi_channel_clear();

	if ((scmi_msg_token & (SCMI_HEADER_TOKEN_MASK >> SCMI_HEADER_TOKEN_SHIFT)) !=
	     SCMI_MSG_GET_TOKEN(shmem->message_header)) {
		INFO("SCMI 0x%x msg %u: token %x, expected %x\n",
		     SCMI_MSG_GET_PROTOCOL(shmem->message_header),
		     SCMI_MSG_GET_MSG_ID(shmem->message_header),
		     SCMI_MSG_GET_TOKEN(shmem->message_header),
		     scmi_msg_token);
		return SCMI_COMMS_ERROR;
	}
	if (shmem->length == 2 * sizeof(uint32_t)) {
		/* Return the first paylod value = status. For example, NOT SUPPORTED */
		return (int32_t)shmem->payload[0];
	} else if (shmem->length != ((1 + size) * sizeof(uint32_t))) {
		INFO("SCMI 0x%x msg %u: size %u, expected %lu\n",
		     SCMI_MSG_GET_PROTOCOL(shmem->message_header),
		     SCMI_MSG_GET_MSG_ID(shmem->message_header),
		     shmem->length, (size + 1) * sizeof(uint32_t));
		return SCMI_COMMS_ERROR;
	}

	return SCMI_SUCCESS;
}

/**
 * @brief Wait the SCMI answer with expected size
 *
 */
static int32_t scmi_rsp_wait(uint32_t timeout_us, uint32_t size)
{
	uint64_t timeout_ref = timeout_init_us(timeout_us);

	while (scmi_channel_busy() && !scmi_channel_error())
	{
		if (timeout_elapsed(timeout_ref)) {
			return SCMI_ST_TIMEOUT;
		}
		udelay(10);
	}

	if (scmi_channel_error()) {
		shmem->channel_status = 0U;
		return SCMI_COMMS_ERROR;
	}

	return scmi_rsp_check(size);
}

/* Query the protocol message attributes for a SCMI protocol */
static int32_t scmi_proto_version(uint32_t proto_id, uint32_t *version)
{
	int32_t ret = scmi_channel_prepare(proto_id, SCMI_MSG_PROTOCOL_VERSION, 0U);

	if (ret != SCMI_SUCCESS) {
		return ret;
	}

	scmi_ring_doorbell();

	ret = scmi_rsp_wait(TIMEOUT_10MS_IN_US, 2U);
	if (ret != SCMI_SUCCESS) {
		return ret;
	}
	*version = shmem->payload[1];

	return (int32_t)shmem->payload[0]; /* status */
}

static int32_t smci_proto_version_check(uint32_t proto_id, uint32_t expected)
{
	uint32_t version;
	int32_t ret;

	ret = scmi_proto_version(proto_id, &version);
	if (ret == SCMI_SUCCESS) {
		if (!is_scmi_version_compatible(version, expected)) {
			WARN("SCMI protocol 0x%x version 0x%x, expected 0x%x\n",
			     proto_id, version, expected);
			return SCMI_OUT_OF_RANGE;
		}
	} else if (ret == SCMI_NOT_SUPPORTED) {
		/* Not supported is a valid error */
		ret = SCMI_SUCCESS;
	} else {
		ERROR("SCMI protocol 0x%x: init error %d\n", proto_id, ret);
	}

	return ret;
}

/* Query the protocol message attributes for a SCMI protocol */
static int32_t scmi_proto_msg_attr(uint32_t proto_id, uint32_t command_id, uint32_t *attr)
{
	int32_t ret = scmi_channel_prepare(proto_id, SCMI_MSG_PROTOCOL_MSG_ATTR, 1U);

	if (ret != SCMI_SUCCESS) {
		return ret;
	}

	shmem->payload[0] = command_id;

	scmi_ring_doorbell();

	ret = scmi_rsp_wait(TIMEOUT_10MS_IN_US, 2U);
	if (ret != SCMI_SUCCESS) {
		return ret;
	}

	*attr = shmem->payload[1];

	return (int32_t)shmem->payload[0]; /* status */
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
	return scmi_proto_version(SCMI_PROTOCOL_ID_VOLTAGE_DOMAIN, version);
}

/**
 * @brief Send a request to get the voltage level of a domain.
 *
 * @param domain_id The ID of the voltage domain.
 *
 */
int32_t scmi_voltd_level_get_snd(uint32_t domain_id)
{
	int32_t ret = scmi_channel_prepare(SCMI_PROTOCOL_ID_VOLTAGE_DOMAIN,
					   SCMI_MSG_VOLTAGE_LEVEL_GET, 1U);
	if (ret != SCMI_SUCCESS) {
		return ret;
	}

	shmem->payload[0] = domain_id;

	scmi_ring_doorbell();

	return SCMI_SUCCESS;
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
	int32_t ret = scmi_rsp_check(2U);

	if (ret != SCMI_SUCCESS) {
		return ret;
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
int32_t scmi_voltd_level_set_snd(uint32_t domain_id, uint32_t flags, int32_t voltage_level)
{
	int32_t ret = scmi_channel_prepare(SCMI_PROTOCOL_ID_VOLTAGE_DOMAIN,
					   SCMI_MSG_VOLTAGE_LEVEL_SET, 3U);
	if (ret != SCMI_SUCCESS) {
		return ret;
	}

	shmem->payload[0] = domain_id;
	shmem->payload[1] = flags;
	shmem->payload[2] = (uint32_t)voltage_level;

	scmi_ring_doorbell();

	return SCMI_SUCCESS;
}

/**
 * @brief Read the response for the voltage level set request.
 *
 * @return The status of the operation.
 *
 */
int32_t scmi_voltd_level_set_rcv()
{
	int32_t ret = scmi_rsp_check(1U);

	if (ret != SCMI_SUCCESS) {
		return ret;
	}

	return (int32_t)shmem->payload[0]; /* status */
}

/*
 * API to set the SCMI system power state
 */
int32_t scmi_sys_pwr_state_set(uint32_t flags, uint32_t system_state)
{
	int32_t ret;

	if (!scmi_sys_pwr_state_set_supported) {
		return SCMI_NOT_SUPPORTED;
	}

	ret = scmi_channel_prepare(SCMI_PROTOCOL_ID_SYS_POWER, SCMI_MSG_SYS_PWR_STATE_SET, 2U);
	if (ret != SCMI_SUCCESS) {
		return ret;
	}

	shmem->payload[0] = flags;
	shmem->payload[1] = system_state;

	scmi_ring_doorbell();

	ret = scmi_rsp_wait(TIMEOUT_10MS_IN_US, 1U);
	if (ret != SCMI_SUCCESS) {
		return ret;
	}

	return (int32_t)shmem->payload[0]; /* status */
}

/* Check if the message SCMI SYSTEM_POWER_STATE_SET is supported */
static void scmi_check_sys_pwr_state_set(void)
{
	uint32_t attr;
	int32_t ret;

	ret = scmi_proto_msg_attr(SCMI_PROTOCOL_ID_SYS_POWER, SCMI_MSG_SYS_PWR_STATE_SET, &attr);
	if (ret == SCMI_SUCCESS) {
		scmi_sys_pwr_state_set_supported = true;
	} else {
		scmi_sys_pwr_state_set_supported = false;
		INFO("SCMI SYSTEM_POWER_STATE_SET not supported (%d)\n", ret);
	}
}

int32_t scmi_init(void)
{
	int32_t ret;

	scmi_sys_pwr_state_set_supported = false;

	/* Initialize channel */
	scmi_channel_clear();

	/* Check supported protocol and messages */
	ret = smci_proto_version_check(SCMI_PROTOCOL_ID_SYS_POWER, SCMI_PROTO_VER_SYS_PWR);
	if (ret != SCMI_SUCCESS) {
		goto err;
	}
	scmi_check_sys_pwr_state_set();

	ret = smci_proto_version_check(SCMI_PROTOCOL_ID_VOLTAGE_DOMAIN, SCMI_PROTO_VER_VOLTAGE);

err:
	return ret;
}
