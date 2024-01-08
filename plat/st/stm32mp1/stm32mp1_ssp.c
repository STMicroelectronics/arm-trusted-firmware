/*
 * Copyright (c) 2017-2024, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <endian.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <arch_helpers.h>
#include <common/debug.h>
#include <drivers/clk.h>
#include <drivers/generic_delay_timer.h>
#include <drivers/st/bsec.h>
#include <drivers/st/regulator.h>
#include <drivers/st/stm32_hash.h>
#include <drivers/st/stm32_iwdg.h>
#include <drivers/st/stm32_uart.h>
#include <drivers/st/stm32mp_pmic.h>
#include <drivers/st/stm32mp_reset.h>
#include <drivers/st/stpmic1.h>
#include <drivers/usb_device.h>
#include <lib/mmio.h>
#include <lib/utils.h>
#include <lib/utils_def.h>
#include <lib/xlat_tables/xlat_tables_v2.h>
#include <plat/common/platform.h>

#include <platform_def.h>
#include <stm32cubeprogrammer.h>
#include <usb_dfu.h>

#define CERT_CHIP_ID_LEN		U(3)
#define CERT_SECURITY_COUNTER_LEN	U(2)
#define CERT_SECURITY_COUNTER_SHIFT	CERT_CHIP_ID_LEN
#define CERT_RFU_LEN			U(1)
#define CERT_RFU_SHIFT			(CERT_SECURITY_COUNTER_LEN + \
					 CERT_SECURITY_COUNTER_SHIFT)
#define CERT_PRODUCT_KEY_LEN		U(2)
#define CERT_PRODUCT_KEY_SHIFT		(CERT_RFU_LEN + CERT_RFU_SHIFT)
#define CERT_PRODUCT_ID_SIZE		(CERT_PRODUCT_KEY_LEN + \
					 CERT_PRODUCT_KEY_SHIFT)
#define CERT_SIGNATURE_LEN		CHIP_CERTIFICATE_MAX_SIZE
#define CERT_SIGNATURE_SHIFT		(CERT_PRODUCT_ID_SIZE + \
					 BOOT_API_SSP_PUBK_KEY_SIZE_BYTES)
#define CERTIFICATE_SIZE		(CERT_PRODUCT_ID_SIZE + \
					 BOOT_API_SSP_PUBK_KEY_SIZE_BYTES + \
					 CERT_SIGNATURE_LEN) /* 136 bytes */
#define BLOB_FILE_MAX_ADDR		BL2_RW_LIMIT

/* Local status for SSP processing sequences */
typedef enum {
	SSP_NONE,
	SSP_GET_CERT,
	SSP_FLASH_OEM,
	SSP_DONE,
	SSP_ERROR
} ssp_result_e;

struct otp_val {
	uint32_t idx;
	uint32_t nb;
};

static struct otp_val otp_ssp;
static struct otp_val otp_rma;
static struct otp_val otp_pubkey;

/* Platform empty definition required */
void bl2_platform_setup(void) {}
struct bl_params *plat_get_next_bl_params(void) { return NULL; }
void plat_flush_next_bl_params(void) {}
struct bl_load_info *plat_get_bl_image_load_info(void) { return NULL; }
int plat_get_image_source(unsigned int image_id, uintptr_t *dev_handle,
			  uintptr_t *image_spec)
{
	return 0;
}

/*
 * Initialized OTP index from device tree.
 */
static int initialize_otp(void)
{
	uint32_t len;

	/* OTP SSP */
	if (stm32_get_otp_index(SSP_OTP, &otp_ssp.idx, NULL) != 0) {
		VERBOSE("%s: get index error\n", __func__);
		return -EINVAL;
	}

	/* OTP public key */
	if (stm32_get_otp_index(PKH_OTP, &otp_pubkey.idx, &len) != 0) {
		VERBOSE("%s: get index error\n", __func__);
		return -EINVAL;
	}

	if (len != (CHAR_BIT * BOOT_API_SHA256_DIGEST_SIZE_IN_BYTES)) {
		VERBOSE("%s: length Error\n", __func__);
		return -EINVAL;
	}

	otp_pubkey.nb = len / __WORD_BIT;

	/* OTP RMA */
	if (stm32_get_otp_index(RMA_OTP, &otp_rma.idx, NULL) != 0) {
		VERBOSE("%s: get index error\n", __func__);
		return -EINVAL;
	}

	return 0;
}

/*
 * Compute HASH from public key and burn it in OTP.
 */
static int ssp_pub_key_prog(boot_api_context_t *boot_context)
{
#if STM32MP13
	uint32_t *value = (uint32_t *)
		boot_context->p_ssp_config->p_blob_payload->oem_rpkth;
#endif
#if STM32MP15
	uint8_t key_hash[BOOT_API_SHA256_DIGEST_SIZE_IN_BYTES] __aligned(4);
	uint8_t *pubk = (uint8_t *)
		boot_context->p_ssp_config->p_blob_payload->oem_ecdsa_pubk;
	uint32_t *value = (uint32_t *)key_hash;
#endif
	uint32_t i;

#if STM32MP15
	if (stm32_hash_register() != 0) {
		return -EINVAL;
	}

	stm32_hash_init(HASH_SHA256);

	if (stm32_hash_final_update(pubk, BOOT_API_SSP_PUBK_KEY_SIZE_BYTES,
				    key_hash) != 0) {
		ERROR("Hash of payload failed\n");
		return -EINVAL;
	}
#endif

	for (i = otp_pubkey.idx; i < (otp_pubkey.idx + otp_pubkey.nb); i++) {
		if (bsec_program_otp(bswap32(*value), i) != BSEC_OK) {
			return -EINVAL;
		}

		if (bsec_permanent_lock_otp(i) != BSEC_OK) {
			ERROR("Error locking OTP %u\n", i);
			panic();
		}

		value++;
	}

	return 0;
}

/*
 * Burn OTP to close device.
 */
static int ssp_close_device(void)
{
	uint32_t otp;
	uint32_t value;

	if (stm32_get_otp_index(CFG0_OTP, &otp, NULL) != 0) {
		return -EINVAL;
	}

	if (bsec_read_otp(&value, otp) != BSEC_OK) {
		return -EINVAL;
	}

#if STM32MP13
	if ((value & ~CFG0_OPEN_DEVICE) != 0U) {
		ERROR("Device already closed\n");
		return -EINVAL;
	}
#endif
#if STM32MP15
	if ((value & CFG0_CLOSED_DEVICE) != 0U) {
		ERROR("Device already closed\n");
		return -EINVAL;
	}
#endif

	value |= CFG0_CLOSED_DEVICE;
	if (bsec_program_otp(value, otp) != BSEC_OK) {
		return -EINVAL;
	}

	return 0;
}

/*
 * OTP initial check to detect previous values.
 */
static int ssp_secrets_check(boot_api_context_t *boot_ctx)
{
	uint32_t i;
	uint32_t check_val;
	uint32_t otp_bytes = boot_ctx->p_ssp_config->p_blob_payload->oem_secret_size_bytes;
	uint32_t otp_decrypted;
	uint32_t *val;

	if (otp_bytes == 0U) {
		return -EINVAL;
	}

	for (i = otp_pubkey.idx; i < (otp_pubkey.idx + otp_pubkey.nb); i++) {
		if (stm32_get_otp_value_from_idx(i, &check_val) != 0) {
			return -EINVAL;
		}

		if (check_val != 0U) {
			ERROR("OTP %u value already programmed\n", i);
			return -EINVAL;
		}
	}

	otp_decrypted = round_up(otp_bytes, sizeof(uint32_t)) / sizeof(uint32_t);

	/* OTP decrypted include RMA password */
	if (otp_decrypted > (2U + SSP_OTP_SECRET_END - SSP_OTP_SECRET_BASE)) {
		return -EINVAL;
	}

	/* Check RMA password */
	if (stm32_get_otp_value_from_idx(otp_rma.idx, &check_val) != 0) {
		return -EINVAL;
	}

	if (check_val != 0U) {
		ERROR("OTP %s value already programmed\n", RMA_OTP);
		return -EINVAL;
	}

	val = (uint32_t *)boot_ctx->p_ssp_config->p_ssp_oem_secrets_decrypted;

	/* Check all OTP available */
	for (i = SSP_OTP_SECRET_BASE; i < SSP_OTP_SECRET_BASE + otp_decrypted - 1U; i++) {
		val++;

		if (*val == 0U) {
			continue;
		}

		if (stm32_get_otp_value_from_idx(i, &check_val) != 0) {
			return -EINVAL;
		}

		if (check_val != 0U) {
			ERROR("OTP %u value already programmed\n", i);
			return -EINVAL;
		}
	}

	return 0;
}

/*
 * Burn OTP with the decrypted secret received.
 */
static int ssp_secrets_flash(boot_api_context_t *boot_ctx)
{
	uint32_t i;
	uint32_t *val;
	uint32_t otp_val;
	uint32_t otp_bytes =
		boot_ctx->p_ssp_config->p_blob_payload->oem_secret_size_bytes;
	uint32_t otp_decrypted;
	uint32_t otp_mask = 0U;

	if (otp_bytes == 0U) {
		return -EINVAL;
	}

	if (otp_bytes % sizeof(uint32_t) != 0U) {
		otp_mask = GENMASK_32(((otp_bytes % sizeof(uint32_t)) *
				       sizeof(uint32_t)) - 1, 0);
	}

	val = (uint32_t *)boot_ctx->p_ssp_config->p_ssp_oem_secrets_decrypted;

	otp_decrypted = round_up(otp_bytes, sizeof(uint32_t)) / sizeof(uint32_t);

	/* Burn RMA password */
	if (bsec_program_otp((*val & RMA_OTP_MASK), otp_rma.idx) !=  BSEC_OK) {
		WARN("RMA programing failed\n");
		return -EINVAL;
	}

	if (bsec_shadow_read_otp(&otp_val, otp_rma.idx) != BSEC_OK) {
		return -EINVAL;
	}

	if ((otp_val & RMA_OTP_MASK) != (*val & RMA_OTP_MASK)) {
		WARN("RMA programming failed\n");
		return -EINVAL;
	}

	if (bsec_permanent_lock_otp(otp_rma.idx) != BSEC_OK) {
		WARN("Error locking RMA OTP\n");
		return -EINVAL;
	}

	val++;
	otp_decrypted--;
	for (i = SSP_OTP_SECRET_BASE; i < (SSP_OTP_SECRET_BASE + otp_decrypted - 1U); i++) {
		if (*val == 0U) {
			val++;
			continue;
		}

		if (bsec_program_otp(*val, i) != BSEC_OK) {
			WARN("Error writing OTP %u\n", i);
			return -EINVAL;
		}

		if (bsec_permanent_lock_otp(i) != BSEC_OK) {
			WARN("Error locking OTP %u\n", i);
			return -EINVAL;
		}

		val++;
	}

	if (*val == 0U) {
		return 0;
	}

	/* Mask the last OTP value if needed */
	if (otp_mask != 0U) {
		*val &= otp_mask;
	}

	if (bsec_program_otp(*val, i) != BSEC_OK) {
		WARN("Error writing OTP %u\n", i);
		return -EINVAL;
	}

	if (bsec_permanent_lock_otp(i) != BSEC_OK) {
		WARN("Error locking OTP %u\n", i);
		return -EINVAL;
	}

	return 0;
}

/*
 * Finish SSP processing by fusing OTP SSP success.
 */
static int ssp_finish_process(void)
{
	uint32_t val;

	if (stm32_get_otp_value_from_idx(otp_ssp.idx, &val) != 0) {
		return -EINVAL;
	}

	if ((val & SSP_OTP_SUCCESS) != 0U) {
		WARN("Error while configuring OTP\n");
		return -EINVAL;
	}

	val |= SSP_OTP_SUCCESS;
	if (bsec_program_otp(val, otp_ssp.idx) != BSEC_OK) {
		return -EINVAL;
	}

	VERBOSE("Write OTP Success\n");

	return 0;
}

/*
 * Transform integer to string.
 */
static void itoa(uint32_t num, char *str, int nb)
{
	if (num == 0U) {
		while (nb-- != 0U) {
			str[nb] = '0';
		}

		return;
	}

	while (num != 0U) {
		int rem = num % 16;

		str[--nb] = (rem > 9) ? (rem - 10) + 'A' : rem + '0';
		num /= 16;
	}

	while (nb != 0) {
		str[--nb] = '0';
	}
}

/*
 * Return chip product ID.
 */
static int ssp_get_product_id(char *msg)
{
	uint32_t otp;
	uint32_t otp_idx;
	uint32_t chip_id = stm32mp_get_chip_dev_id();

	if (stm32_get_otp_index(CFG2_OTP, &otp_idx, NULL) != 0) {
		VERBOSE("Get index error\n");
		return -EINVAL;
	}

	if (stm32_get_otp_value_from_idx(otp_idx, &otp) != 0) {
		return -EINVAL;
	}

	itoa(chip_id, msg, CERT_CHIP_ID_LEN);
	itoa((otp & OTP_CFG2_SEC_COUNTER_MASK) >> OTP_CFG2_SEC_COUNTER_SHIFT,
	     msg + CERT_SECURITY_COUNTER_SHIFT,
	     CERT_SECURITY_COUNTER_LEN);

	itoa(0, msg + CERT_RFU_SHIFT, CERT_RFU_LEN);
	itoa((otp & OTP_CFG2_ST_KEY_MASK) >> OTP_CFG2_ST_KEY_SHIFT,
	     msg + CERT_PRODUCT_KEY_SHIFT,
	     CERT_PRODUCT_KEY_LEN);

	return 0;
}

/*
 * Construct SSP certificate.
 */
static int prepare_certificate(uint8_t *cert, const uint8_t *pubkey)
{
	uint32_t i;
	uint32_t j;
	uint32_t otp;
	uint32_t otp_idx;
	uint32_t otp_len;

	/* Prepare the ROM Security constant */
	if (ssp_get_product_id((char *)cert) != 0) {
		return -EINVAL;
	}

	/* Prepare public key and certificate for flashloader */
	/* Read Public Key from boot_context */
	memcpy(cert + CERT_PRODUCT_ID_SIZE, pubkey, BOOT_API_SSP_PUBK_KEY_SIZE_BYTES);

	if (stm32_get_otp_index(CHIP_CERTIFICATE_OTP,
				&otp_idx, &otp_len) != 0) {
		VERBOSE("Get index error\n");
		return -EINVAL;
	}

	if (otp_len != (CHAR_BIT * CHIP_CERTIFICATE_MAX_SIZE)) {
		VERBOSE("Length error\n");
		return -EINVAL;
	}

	otp_len /= __WORD_BIT;

	/* Read Certificat from OTP */
	for (i = otp_idx, j = 0U; i < (otp_idx + otp_len); i++, j++) {
		uint32_t otp_s;

		if (stm32_get_otp_value_from_idx(i, &otp) != 0) {
			return -EINVAL;
		}

		otp_s = bswap32(otp);
		memcpy(&cert[CERT_SIGNATURE_SHIFT + (sizeof(uint32_t) * j)],
		       &otp_s, sizeof(uint32_t));
	}

	return 0;
}

/*
 * Clean OTP value that might be still in shadow registers
 */
static void cleanup_otp_value(void)
{
	unsigned int i;

	/* RMA clear and lock */
	if (bsec_write_otp(0U, otp_rma.idx) != BSEC_OK) {
		return;
	}

	if (bsec_set_sr_lock(otp_rma.idx) != BSEC_OK) {
		return;
	}

	for (i = SSP_OTP_SECRET_BASE; i < SSP_OTP_SECRET_END + 1U; i++) {
		if (bsec_write_otp(0U, i) != BSEC_OK) {
			return;
		}
	}
}

/*
 * Clean external data and bootrom context secret values.
 */
static void ssp_cleanup(boot_api_context_t *boot_context)
{
	boot_api_ssp_config_t *ssp_config = boot_context->p_ssp_config;

	/* Cleanup boot_context */
	if (ssp_config->p_ssp_oem_secrets_decrypted != NULL) {
		zeromem(ssp_config->p_ssp_oem_secrets_decrypted,
			BOOT_API_SSP_OEM_SECRETS_MAX_SIZE_BYTES);
#ifndef DCACHE_OFF
		flush_dcache_range((uintptr_t)ssp_config->p_ssp_oem_secrets_decrypted,
				   BOOT_API_SSP_OEM_SECRETS_MAX_SIZE_BYTES);
#endif
		ssp_config->p_ssp_oem_secrets_decrypted = NULL;
	}

	if (ssp_config->p_chip_pubk != NULL) {
		zeromem(ssp_config->p_chip_pubk,
			BOOT_API_SSP_PUBK_KEY_SIZE_BYTES);
#ifndef DCACHE_OFF
		flush_dcache_range((uintptr_t)ssp_config->p_chip_pubk,
				   BOOT_API_SSP_PUBK_KEY_SIZE_BYTES);
#endif
		ssp_config->p_chip_pubk = NULL;
	}

	if (ssp_config->p_blob_license != NULL) {
		zeromem(ssp_config->p_blob_license,
			sizeof(boot_api_ssp_blob_license_t));
#ifndef DCACHE_OFF
		flush_dcache_range((uintptr_t)ssp_config->p_blob_license,
				   sizeof(boot_api_ssp_blob_license_t));
#endif
		ssp_config->p_blob_license = NULL;
	}

	if (ssp_config->p_blob_payload != NULL) {
		zeromem(ssp_config->p_blob_payload,
			sizeof(boot_api_ssp_blob_payload_t));
#ifndef DCACHE_OFF
		flush_dcache_range((uintptr_t)ssp_config->p_blob_payload,
			   sizeof(boot_api_ssp_blob_payload_t));
#endif
		ssp_config->p_blob_payload = NULL;
	}

	ssp_config->ssp_cmd = 0U;

#ifndef DCACHE_OFF
	flush_dcache_range((uintptr_t)boot_context->p_ssp_config,
			   sizeof(boot_api_ssp_config_t));
#endif
}

/*
 * Send certificate to the programmer and retrieve the associated
 * encrypted file.
 */
static int ssp_download_phase(boot_api_context_t *boot_ctx)
{
	uint8_t *blob_file;
#if STM32MP_UART_PROGRAMMER
	uintptr_t uart_base;
#endif
#if STM32MP_USB_PROGRAMMER
	struct usb_handle *pdev;
#endif
	int result = 0;
	uint8_t cert[CERTIFICATE_SIZE];

#if STM32MP13
	blob_file = (uint8_t *)boot_ctx->p_ssp_config->p_blob_license;
#endif
#if STM32MP15
	blob_file = (uint8_t *)page_align(BLOB_FILE_MAX_ADDR -
					  sizeof(boot_api_ssp_blob_license_t) -
					  sizeof(boot_api_ssp_blob_payload_t),
					  DOWN);
#endif

	if (prepare_certificate(cert, boot_ctx->p_ssp_config->p_chip_pubk) != 0) {
		return -EINVAL;
	}

	switch (boot_ctx->boot_interface_selected) {
#if STM32MP_USB_PROGRAMMER
	case BOOT_API_CTX_BOOT_INTERFACE_SEL_SERIAL_USB:

		/* init USB on platform */
		pdev = usb_dfu_plat_init();

		result = stm32cubeprog_usb_ssp(pdev, (uintptr_t)cert,
					       sizeof(cert), (uintptr_t)blob_file,
					       sizeof(boot_api_ssp_blob_license_t) +
					       sizeof(boot_api_ssp_blob_payload_t));
		if (result != 0) {
			return -EINVAL;
		}

		break;
#endif

#if STM32MP_UART_PROGRAMMER
	case BOOT_API_CTX_BOOT_INTERFACE_SEL_SERIAL_UART:
		uart_base = get_uart_address(boot_ctx->boot_interface_instance);

		if (uart_base == 0U) {
			return -EINVAL;
		}

		result = stm32cubeprog_uart_ssp(uart_base, (uintptr_t)cert, sizeof(cert),
						(uintptr_t)blob_file,
						sizeof(boot_api_ssp_blob_license_t) +
						sizeof(boot_api_ssp_blob_payload_t));
		if (result != 0) {
			return -EINVAL;
		}
		break;
#endif
	default:
		return -EINVAL;
	}

	boot_ctx->p_ssp_config->p_blob_license =
		(boot_api_ssp_blob_license_t *)blob_file;

	/* Payload is concatened with license file */
	boot_ctx->p_ssp_config->p_blob_payload =
		(boot_api_ssp_blob_payload_t *)(blob_file +
		sizeof(boot_api_ssp_blob_license_t));

#ifndef DCACHE_OFF
	flush_dcache_range((uintptr_t)blob_file,
			   sizeof(boot_api_ssp_blob_license_t) +
			   sizeof(boot_api_ssp_blob_payload_t));
#endif

	/* Set return address for decrypted_secrets */
	boot_ctx->p_ssp_config->p_ssp_oem_secrets_decrypted =
		boot_ctx->p_ssp_config->p_blob_payload->oem_encrypted_secrets;

	return result;
}

/*
 * Burn decrypted secrets into OTP, clean memory and close the device.
 */
static int ssp_secret_programming(boot_api_context_t *boot_context)
{
	int result;

	result = ssp_secrets_check(boot_context);
	if (result != 0) {
		ERROR("SSP ERROR checking OTP\n");
		goto clean;
	}

	result = ssp_pub_key_prog(boot_context);
	if (result != 0) {
		ERROR("SSP ERROR writing HASH key\n");
		goto clean;
	}

	result = ssp_close_device();
	if (result != 0) {
		ERROR("SSP close device failed\n");
		goto clean;
	}

	result = ssp_secrets_flash(boot_context);
	if (result != 0) {
		ERROR("SSP Secret flash failed\n");
	}

clean:
	cleanup_otp_value();

	ssp_cleanup(boot_context);

	if (result != 0) {
		return result;
	}

	return ssp_finish_process();
}

/*
 * Enable the SSP processing.
 */
static int ssp_enable_processing(boot_api_context_t *boot_context)
{
	uint32_t val;
	int result;
#if STM32MP_USB_PROGRAMMER
	struct usb_handle *pdev;
#endif
#if STM32MP_UART_PROGRAMMER
	uintptr_t uart_base;
#endif

	if (stm32_get_otp_value_from_idx(otp_ssp.idx, &val) != 0) {
		return -EINVAL;
	}

	if (((val & SSP_OTP_MASK) == SSP_OTP_MASK) ||
	    ((val & SSP_OTP_MASK) == SSP_OTP_SUCCESS)) {
		return -EINVAL;
	}

	if ((val & SSP_OTP_MASK) == 0U) {
		if (bsec_program_otp(SSP_OTP_REQ, otp_ssp.idx) != BSEC_OK) {
			return -EINVAL;
		}
	}

	switch (boot_context->boot_interface_selected) {
#if STM32MP_UART_PROGRAMMER
	case BOOT_API_CTX_BOOT_INTERFACE_SEL_SERIAL_UART:
		uart_base = get_uart_address(boot_context->boot_interface_instance);
		if (uart_base == 0U) {
			return -EINVAL;
		}

		result = stm32cubeprog_uart_ssp(uart_base, (uintptr_t)-1, 0,
						(uintptr_t)NULL, 0);
		if (result != 0) {
			return -EINVAL;
		}
		break;
#endif
#if STM32MP_USB_PROGRAMMER
	case BOOT_API_CTX_BOOT_INTERFACE_SEL_SERIAL_USB:
		/* init USB on platform */
		pdev = usb_dfu_plat_init();

		result = stm32cubeprog_usb_ssp(pdev, (uintptr_t)-1, 0,
					       (uintptr_t)NULL, 0);
		if (result != 0) {
			return -EINVAL;
		}

		break;
#endif
	default:
		return -EINVAL;
	}

	boot_context->p_ssp_config->ssp_cmd =
		BOOT_API_CTX_SSP_CMD_CALC_CHIP_PUBK;

	return 0;
}

/*
 * Retrieve the current status of the SSP from bootrom context and OTP value.
 */
static ssp_result_e ssp_check_status(boot_api_context_t *boot_context)
{
	uint32_t otp;

	if (initialize_otp() < 0) {
		return SSP_ERROR;
	}

	if (stm32_get_otp_value_from_idx(otp_ssp.idx, &otp) != 0) {
		return SSP_ERROR;
	}

	if ((otp & SSP_OTP_REQ) == 0U) {
		return SSP_NONE;
	}

	if ((otp & SSP_OTP_SUCCESS) != 0U) {
		return SSP_DONE;
	}

	VERBOSE("Start Get ssp_cmd : %x\n",
		boot_context->p_ssp_config->ssp_cmd);

	switch (boot_context->p_ssp_config->ssp_cmd) {
	case BOOT_API_CTX_SSP_CMD_CALC_CHIP_PUBK_ACK:
		INFO("Detected start SSP Phase 2\n");
		return SSP_GET_CERT;
	case BOOT_API_CTX_SSP_CMD_PROV_SECRET_ACK:
		INFO("Detected start SSP Phase 3\n");
		return SSP_FLASH_OEM;
	default:
		return SSP_NONE;
	}
}

/*
 * Start the SSP processing.
 */
static void ssp_start(boot_api_context_t *boot_context)
{
	int result;
	uint8_t ssp_phase = ssp_check_status(boot_context);

	switch (ssp_phase) {
	case SSP_GET_CERT:
		result = ssp_download_phase(boot_context);
		if (result != 0) {
			/*
			 * Download Phase failed, clean, reset
			 */
			ssp_cleanup(boot_context);

			ERROR("SSP_Error: Resetting target\n");
		} else {
			/* Process completed, go to Phase 3 */
			boot_context->p_ssp_config->ssp_cmd =
				BOOT_API_CTX_SSP_CMD_PROV_SECRET;
		}

		break;

	case SSP_FLASH_OEM:
		result = ssp_secret_programming(boot_context);
		if (result != 0) {
			ERROR("Error during provisionning\n");
		} else {
			NOTICE("Provisioning completed\n");
		}

		break;

	case SSP_ERROR:
		/*
		 * Error during bootrom SSP processing
		 */
		result = -EINVAL;
		ERROR("SSP_Error: Resetting target\n");
		break;

	case SSP_NONE:
	default:
		result = ssp_enable_processing(boot_context);
		if (result != 0) {
			ERROR("Start SSP Failed (%i)\n", result);
		}
	}

	if ((result != 0) || (ssp_phase == SSP_FLASH_OEM)) {
		goto out;
	}

	/*
	 * Keep VDDCORE && VDD enabled if PMIC used to generate
	 * the required MPSYSRST.
	 */
	if (dt_pmic_status() > 0) {
		struct rdev *regul;

		regul = dt_get_sysram_regulator();
		if (regul == NULL) {
			panic();
		}

		if (regulator_set_flag(regul, REGUL_MASK_RESET) < 0) {
			WARN("Failed to write cpu-supply reset mask\n");
		}

		regul = dt_get_vdd_regulator();
		if (regul == NULL) {
			goto out;
		}

		if (regulator_set_flag(regul, REGUL_MASK_RESET) < 0) {
			WARN("Failed to write vdd-supply reset mask\n");
		}
	} else {
		static const char debug_msg[] = {
			"SSP next step will be only guarantee if the VDD\n"
			"domain is maintained during system reset\n"
		};

		NOTICE("%s", debug_msg);
	}

out:
#ifndef DCACHE_OFF
	if (boot_context->p_ssp_config != NULL) {
		flush_dcache_range((uintptr_t)boot_context->p_ssp_config,
				   sizeof(boot_api_ssp_config_t));
	}
#endif

	stm32mp_system_reset();
}

void bl2_el3_early_platform_setup(u_register_t arg0,
				  u_register_t arg1 __unused,
				  u_register_t arg2 __unused,
				  u_register_t arg3 __unused)
{
	stm32mp_save_boot_ctx_address(arg0);
}

void bl2_el3_plat_arch_setup(void)
{
#if DEBUG
	const char *board_model;
#endif
	uintptr_t pwr_base;
	uintptr_t rcc_base;

	boot_api_context_t *boot_context =
		(boot_api_context_t *)stm32mp_get_boot_ctx_address();

	if (bsec_probe() != 0) {
		panic();
	}

	mmap_add_region(BL_CODE_BASE, BL_CODE_BASE,
			BL_CODE_END - BL_CODE_BASE,
			MT_CODE | MT_SECURE);

#if SEPARATE_CODE_AND_RODATA
	mmap_add_region(BL_RO_DATA_BASE, BL_RO_DATA_BASE,
			BL_RO_DATA_END - BL_RO_DATA_BASE,
			MT_RO_DATA | MT_SECURE);
#endif

	/* Prevent corruption of preloaded Device Tree */
	mmap_add_region(DTB_BASE, DTB_BASE,
			DTB_LIMIT - DTB_BASE,
			MT_RO_DATA | MT_SECURE);

	configure_mmu();

	if (dt_open_and_check(STM32MP_DTB_BASE) < 0) {
		panic();
	}

	pwr_base = stm32mp_pwr_base();
	rcc_base = stm32mp_rcc_base();

	/*
	 * Disable the backup domain write protection.
	 * The protection is enable at each reset by hardware
	 * and must be disabled by software.
	 */
	mmio_setbits_32(pwr_base + PWR_CR1, PWR_CR1_DBP);

	while ((mmio_read_32(pwr_base + PWR_CR1) & PWR_CR1_DBP) == 0U) {
		;
	}

	/* Reset backup domain on cold boot cases */
	if ((mmio_read_32(rcc_base + RCC_BDCR) & RCC_BDCR_RTCSRC_MASK) == 0U) {
		mmio_setbits_32(rcc_base + RCC_BDCR, RCC_BDCR_VSWRST);

		while ((mmio_read_32(rcc_base + RCC_BDCR) & RCC_BDCR_VSWRST) ==
		       0U) {
			;
		}

		mmio_clrbits_32(rcc_base + RCC_BDCR, RCC_BDCR_VSWRST);
	}

	/* Set minimum reset pulse duration to 31ms for discrete power supplied boards */
	if (dt_pmic_status() <= 0) {
		mmio_clrsetbits_32(rcc_base + RCC_RDLSICR, RCC_RDLSICR_MRD_MASK,
				   31U << RCC_RDLSICR_MRD_SHIFT);
	}

	generic_delay_timer_init();

#if STM32MP_UART_PROGRAMMER
	/* Disable programmer UART before changing clock tree */
	if (boot_context->boot_interface_selected ==
	    BOOT_API_CTX_BOOT_INTERFACE_SEL_SERIAL_UART) {
		uintptr_t uart_prog_addr =
			get_uart_address(boot_context->boot_interface_instance);

		stm32_uart_stop(uart_prog_addr);
	}
#endif

	if (stm32mp1_clk_probe() < 0) {
		panic();
	}

	if (dt_pmic_status() > 0) {
		initialize_pmic();
	}

#if DEBUG
	if (stm32mp_uart_console_setup() != 0) {
		goto skip_console_init;
	}

	stm32mp_print_cpuinfo();

	board_model = dt_get_board_model();
	if (board_model != NULL) {
		NOTICE("Model: %s\n", board_model);
	}

	stm32mp_print_boardinfo();

	if ((boot_context->p_ssp_config == NULL) ||
	    (boot_context->p_ssp_config->ssp_cmd !=
	     BOOT_API_CTX_SSP_CMD_PROV_SECRET_ACK)) {
		if (!stm32mp_is_auth_supported()) {
			ERROR("Chip doesn't support SSP\n");
			panic();
		}
	}

skip_console_init:
#endif
	if (stm32mp_check_closed_device() == STM32MP_CHIP_SEC_CLOSED) {
		/* Closed chip required authentication */
		ERROR("SSP not supported on closed chip\n");
		panic();
	}

	if (stm32_iwdg_init() < 0) {
		panic();
	}

	stm32_iwdg_refresh();

	if (dt_pmic_status() > 0) {
		initialize_pmic_i2c();
		print_pmic_info_and_debug();
	}

	ssp_start(boot_context);

	/* This must not be reached */
	panic();
}
