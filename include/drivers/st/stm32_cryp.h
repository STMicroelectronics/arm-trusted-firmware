/*
 * Copyright (c) 2025, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef STM32_CRYP_H
#define STM32_CRYP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DT_CRYP_COMPAT	"st,stm32mp1-cryp"

struct stm32_cryp_platdata {
	uintptr_t base;
	unsigned long clock_id;
	unsigned int reset_id;
};

enum stm32_cryp_algo_mode {
	STM32_CRYP_MODE_TDES_ECB,
	STM32_CRYP_MODE_TDES_CBC,
	STM32_CRYP_MODE_DES_ECB,
	STM32_CRYP_MODE_DES_CBC,
	STM32_CRYP_MODE_AES_ECB,
	STM32_CRYP_MODE_AES_CBC,
	STM32_CRYP_MODE_AES_CTR,
	STM32_CRYP_MODE_AES_GCM,
	STM32_CRYP_MODE_AES_CCM,
};

/*
 * Full CRYP context.
 * Store CRYP internal state to be able to compute any supported algorithm.
 */
struct stm32_cryp_context {
	uintptr_t base;
	uint32_t cr;
	uint32_t assoc_len;
	uint32_t load_len;
	uint32_t key[8]; /* In HW byte order */
	size_t key_size;
	size_t block_u32;
	uint32_t iv[4];  /* In HW byte order */
	uint32_t pm_gcmccm[8];
	union {
		uint32_t pm_gcm[8];
		uint32_t ctr0_ccm[4];
	};
	uint32_t extra[4];
	size_t extra_size;
	bool saes_key_share;
};

int stm32_cryp_driver_init(void);

void stm32_cryp_key_sharing_enable(struct stm32_cryp_context *ctx, bool enable);
int stm32_cryp_init(struct stm32_cryp_context *ctx, bool is_decrypt, enum stm32_cryp_algo_mode mode,
		    const void *key, size_t key_size, const void *iv, size_t iv_size);
int stm32_cryp_update(struct stm32_cryp_context *ctx, bool last_block, uint8_t *data_in,
		      uint8_t *data_out, size_t data_size);
int stm32_cryp_update_assodata(struct stm32_cryp_context *ctx, uint8_t *data, size_t data_size);
int stm32_cryp_update_load(struct stm32_cryp_context *ctx, uint8_t *data_in, uint8_t *data_out,
			   size_t data_size);
int stm32_cryp_final(struct stm32_cryp_context *ctx, uint8_t *tag, size_t tag_size);
#endif
