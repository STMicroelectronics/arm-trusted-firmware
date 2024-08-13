/*
 * Copyright (C) 2024, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <platform_def.h>

/* Magic use to indicated valid mirror = 'B' 'S' 'E' 'C' */
#define OTP_MIRROR_MAGIC		0x42534543U

#define OTP_MAX_SIZE			(STM32MP2_OTP_MAX_ID + 1U)

struct otp_mirror {
	uint32_t magic;
	uint32_t state;
	struct {
		uint32_t value;
		uint32_t status;
	} otp[OTP_MAX_SIZE];
};

uint32_t otp_mirror_read(uint32_t *val, uint32_t otp)
{
	struct otp_mirror *mirror = (struct otp_mirror *)SRAM1_BASE;

	/* Upper OTPs are not mirrored */
	if (otp >= STM32MP2_UPPER_OTP_START) {
		return BSEC_NOT_SUPPORTED;
	}

	/* OTP already mirrored */
	if ((mirror->magic == OTP_MIRROR_MAGIC) && (mirror->state != BSEC_STATE_INVALID)) {
		*val = mirror->otp[otp].value;

		return BSEC_OK;
	}

	return BSEC_ERROR;
}
