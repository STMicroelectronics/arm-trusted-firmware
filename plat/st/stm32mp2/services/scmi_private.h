/*
 * Copyright (c) 2025, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef STM32MP2_SCMI_PRIVATE
#define STM32MP2_SCMI_PRIVATE

#include <drivers/scmi.h>
#include <platform_def.h>

#define SCMI_ST_TIMEOUT                 (-128)

/*
 * Bit-fields of the `flags` parameter of system power protocol in
 * SYSTEM_POWER_STATE_SET message with scmi_sys_pwr_state_set().
 */
#define SCMI_SYS_PWR_GRACEFUL_REQ		BIT_32(0)
#define SCMI_SYS_PWR_FORCEFUL_REQ		0U

/*
 * `system_state` parameter of system power domain protocol
 * SYSTEM_POWER_STATE_SET message with scmi_sys_pwr_state_set().
 */
#define SCMI_SYS_PWR_SHUTDOWN			0x0
#define SCMI_SYS_PWR_COLD_RESET			0x1
#define SCMI_SYS_PWR_WARM_RESET			0x2
#define SCMI_SYS_PWR_POWER_UP			0x3
#define SCMI_SYS_PWR_SUSPEND			0x4

int32_t scmi_init(void);
void scmi_channel_clear(void);
bool scmi_channel_busy(void);
bool scmi_channel_error(void);

int32_t scmi_voltd_protocol_version(uint32_t *version);
int32_t scmi_voltd_level_get_snd(uint32_t domain_id);
int32_t scmi_voltd_level_get_rcv(int32_t *voltage_level);
int32_t scmi_voltd_level_set_snd(uint32_t domain_id, uint32_t flags, int32_t voltage_level);
int32_t scmi_voltd_level_set_rcv();

int32_t scmi_sys_pwr_state_set(uint32_t flags, uint32_t system_state);

#endif /* STM32MP2_SCMI_PRIVATE */
