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

void scmi_channel_clear(void);
bool scmi_channel_busy(void);
bool scmi_channel_error(void);

int32_t scmi_voltd_protocol_version(uint32_t *version);
void scmi_voltd_level_get_snd(uint32_t domain_id);
int32_t scmi_voltd_level_get_rcv(int32_t *voltage_level);
void scmi_voltd_level_set_snd(uint32_t domain_id, uint32_t flags,
                              int32_t voltage_level);
int32_t scmi_voltd_level_set_rcv();

#endif /* STM32MP2_SCMI_PRIVATE */