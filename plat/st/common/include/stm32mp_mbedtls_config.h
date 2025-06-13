/*
 * Copyright (c) 2025, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <default_mbedtls_config.h>

/* MPI / BIGNUM options */
#undef MBEDTLS_MPI_WINDOW_SIZE
#define MBEDTLS_MPI_WINDOW_SIZE			2

#undef TF_MBEDTLS_HEAP_SIZE
#if STM32MP_CRYPTO_USE_SW
/* Mbed TLS heap size set in order to be able to use ecdsa */
#define TF_MBEDTLS_HEAP_SIZE           U(13312)
#else
/*
 * Mbed TLS heap size is small as we only use the asn1
 * parsing functions
 * digest, signature and crypto algorithm are done by
 * other library.
 */
#define TF_MBEDTLS_HEAP_SIZE            U(5120)
#endif
