/*
 * Copyright (c) 2025, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef STM32_CRYPTO_COMMON_H
#define STM32_CRYPTO_COMMON_H

#include <lib/utils_def.h>

/*
 * Crypto algorithm common macro used in stm32_saes and stm32_cryp driver
 */
#define UINT8_BIT			8U
#define AES_BLOCK_SIZE_BIT		128U
#define AES_BLOCK_SIZE			(AES_BLOCK_SIZE_BIT / UINT8_BIT)
#define AES_BLOCK_NB_U32		(AES_BLOCK_SIZE / sizeof(uint32_t))
#define DES_BLOCK_SIZE_BIT		U(64)
#define DES_BLOCK_SIZE			(DES_BLOCK_SIZE_BIT / UINT8_BIT)
#define DES_BLOCK_NB_U32		(DES_BLOCK_SIZE / sizeof(uint32_t))
#define AES_KEYSIZE_128			U(16)
#define AES_KEYSIZE_192			U(24)
#define AES_KEYSIZE_256			U(32)
#define AES_IVSIZE			U(16)

#endif /* STM32_CRYPTO_COMMON_H */