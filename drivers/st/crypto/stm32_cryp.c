/*
 * Copyright (c) 2025, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <endian.h>
#include <errno.h>
#include <stdint.h>

#include <drivers/clk.h>
#include <drivers/delay_timer.h>
#include <drivers/st/stm32_cryp.h>
#include <drivers/st/stm32mp_reset.h>
#include <lib/mmio.h>
#include <lib/utils_def.h>
#include <libfdt.h>

#include <platform_def.h>

#include "stm32_crypto_common.h"

/* CRYP control register */
#define _CRYP_CR			0x0U
/* CRYP status register */
#define _CRYP_SR			0x04U
/* CRYP data input register */
#define _CRYP_DIN			0x08U
/* CRYP data output register */
#define _CRYP_DOUT			0x0CU
/* CRYP DMA control register */
#define _CRYP_DMACR			0x10U
/* CRYP interrupt mask set/clear register */
#define _CRYP_IMSCR			0x14U
/* CRYP raw interrupt status register */
#define _CRYP_RISR			0x18U
/* CRYP masked interrupt status register */
#define _CRYP_MISR			0x1CU
/* CRYP key registers */
#define _CRYP_K0LR			0x20U
#define _CRYP_K0RR			0x24U
#define _CRYP_K1LR			0x28U
#define _CRYP_K1RR			0x2CU
#define _CRYP_K2LR			0x30U
#define _CRYP_K2RR			0x34U
#define _CRYP_K3LR			0x38U
#define _CRYP_K3RR			0x3CU
/* CRYP initialization vector registers */
#define _CRYP_IV0LR			0x40U
#define _CRYP_IV0RR			0x44U
#define _CRYP_IV1LR			0x48U
#define _CRYP_IV1RR			0x4CU
/* CRYP context swap GCM-CCM registers */
#define _CRYP_CSGCMCCM0R		0x50U
#define _CRYP_CSGCMCCM1R		0x54U
#define _CRYP_CSGCMCCM2R		0x58U
#define _CRYP_CSGCMCCM3R		0x5CU
#define _CRYP_CSGCMCCM4R		0x60U
#define _CRYP_CSGCMCCM5R		0x64U
#define _CRYP_CSGCMCCM6R		0x68U
#define _CRYP_CSGCMCCM7R		0x6CU
/* CRYP context swap GCM registers */
#define _CRYP_CSGCM0R			0x70U
#define _CRYP_CSGCM1R			0x74U
#define _CRYP_CSGCM2R			0x78U
#define _CRYP_CSGCM3R			0x7CU
#define _CRYP_CSGCM4R			0x80U
#define _CRYP_CSGCM5R			0x84U
#define _CRYP_CSGCM6R			0x88U
#define _CRYP_CSGCM7R			0x8CU
/* CRYP hardware configuration register */
#define _CRYP_HWCFGR			0x3F0U
/* CRYP HW version register */
#define _CRYP_VERR			0x3F4U
/* CRYP identification */
#define _CRYP_IPIDR			0x3F8U
/* CRYP HW magic ID */
#define _CRYP_MID			0x3FCU

#define CRYP_TIMEOUT_US			1000000U
#define TIMEOUT_US_1MS			1000U
#define CRYP_RESET_DELAY		20U

/* CRYP control register fields */
#define _CRYP_CR_RESET_VALUE		0x0U
#define _CRYP_CR_KMOD_MSK		GENMASK_32(25, 24)
#define _CRYP_CR_KMOD_OFF		24U
#define _CRYP_CR_KMOD_NORMAL		0x0U
#define _CRYP_CR_KMOD_SHARED		0x2U
#define _CRYP_CR_NPBLB_MSK		GENMASK_32(23, 20)
#define _CRYP_CR_NPBLB_OFF		20U
#define _CRYP_CR_GCM_CCMPH_MSK		GENMASK_32(17, 16)
#define _CRYP_CR_GCM_CCMPH_OFF		16U
#define _CRYP_CR_GCM_CCMPH_INIT		0U
#define _CRYP_CR_GCM_CCMPH_HEADER	1U
#define _CRYP_CR_GCM_CCMPH_PAYLOAD	2U
#define _CRYP_CR_GCM_CCMPH_FINAL	3U
#define _CRYP_CR_CRYPEN			BIT(15)
#define _CRYP_CR_FFLUSH			BIT(14)
#define _CRYP_CR_KEYSIZE_MSK		GENMASK_32(9, 8)
#define _CRYP_CR_KEYSIZE_OFF		8U
#define _CRYP_CR_KSIZE_128		0U
#define _CRYP_CR_KSIZE_192		1U
#define _CRYP_CR_KSIZE_256		2U
#define _CRYP_CR_DATATYPE_MSK		GENMASK_32(7, 6)
#define _CRYP_CR_DATATYPE_OFF		6U
#define _CRYP_CR_DATATYPE_NONE		0U
#define _CRYP_CR_DATATYPE_HALF_WORD	1U
#define _CRYP_CR_DATATYPE_BYTE		2U
#define _CRYP_CR_DATATYPE_BIT		3U
#define _CRYP_CR_ALGOMODE_MSK		(BIT(19) | GENMASK_32(5, 3))
#define _CRYP_CR_ALGOMODE_OFF		3U
#define _CRYP_CR_ALGOMODE_TDES_ECB	0x0U
#define _CRYP_CR_ALGOMODE_TDES_CBC	0x1U
#define _CRYP_CR_ALGOMODE_DES_ECB	0x2U
#define _CRYP_CR_ALGOMODE_DES_CBC	0x3U
#define _CRYP_CR_ALGOMODE_AES_ECB	0x4U
#define _CRYP_CR_ALGOMODE_AES_CBC	0x5U
#define _CRYP_CR_ALGOMODE_AES_CTR	0x6U
#define _CRYP_CR_ALGOMODE_AES		0x7U
#define _CRYP_CR_ALGOMODE_AES_GCM	BIT(16)
#define _CRYP_CR_ALGOMODE_AES_CCM	(BIT(16) | BIT(0))
#define _CRYP_CR_ALGODIR		BIT(2)
#define _CRYP_CR_ALGODIR_ENC		0U
#define _CRYP_CR_ALGODIR_DEC		BIT(2)

/* CRYP status register fields */
#define _CRYP_SR_KEY_VALID		BIT(7)
#define _CRYP_SR_KERF			BIT(6)
#define _CRYP_SR_BUSY			BIT(4)
#define _CRYP_SR_OFFU			BIT(3)
#define _CRYP_SR_OFNE			BIT(2)
#define _CRYP_SR_IFNF			BIT(1)
#define _CRYP_SR_IFEM			BIT(0)

/* CRYP DMA control register fields */
#define _CRYP_DMACR_DOEN		BIT(1)
#define _CRYP_DMACR_DIEN		BIT(0)

/* CRYP interrupt fields */
#define _CRYP_I_OUT			BIT(1)
#define _CRYP_I_IN			BIT(0)

/* CRYP hardware configuration register fields */
#define _CRYP_HWCFGR_CFG1_MSK		GENMASK_32(3, 0)
#define _CRYP_HWCFGR_CFG1_OFF		0U
#define _CRYP_HWCFGR_CFG2_MSK		GENMASK_32(7, 4)
#define _CRYP_HWCFGR_CFG2_OFF		4U
#define _CRYP_HWCFGR_CFG3_MSK		GENMASK_32(11, 8)
#define _CRYP_HWCFGR_CFG3_OFF		8U
#define _CRYP_HWCFGR_CFG4_MSK		GENMASK_32(15, 12)
#define _CRYP_HWCFGR_CFG4_OFF		12U

/* CRYP HW version register */
#define _CRYP_VERR_MSK			GENMASK_32(7, 0)
#define _CRYP_VERR_OFF			0U

/*
 * Macro to manage bit manipulation when we work on a local variable
 * before writing only once to the hardware register.
 */
#define CLRBITS(v, bits)		((v) &= ~(bits))
#define SETBITS(v, bits)		((v) |= (bits))

#define IS_ALGOMODE(cr, mod) \
	(((cr) & _CRYP_CR_ALGOMODE_MSK) == (_CRYP_CR_ALGOMODE_##mod << \
					  _CRYP_CR_ALGOMODE_OFF))

#define SET_ALGOMODE(mod, cr) \
	clrsetbits(&(cr), _CRYP_CR_ALGOMODE_MSK, (_CRYP_CR_ALGOMODE_##mod << \
						  _CRYP_CR_ALGOMODE_OFF))

#define GET_ALGOMODE(cr) \
	(((cr) & _CRYP_CR_ALGOMODE_MSK) >> _CRYP_CR_ALGOMODE_OFF)

static struct stm32_cryp_platdata cryp_pdata;

static int stm32_cryp_parse_fdt(struct stm32_cryp_platdata *pdata)
{
	int node;
	struct dt_node_info info;
	void *fdt;

	if (fdt_get_address(&fdt) == 0) {
		return -FDT_ERR_NOTFOUND;
	}

	node = dt_get_node(&info, -1, DT_CRYP_COMPAT);
	if (node < 0) {
		ERROR("No CRYP entry in DT\n");
		return -FDT_ERR_NOTFOUND;
	}

	if (info.status == DT_DISABLED) {
		return -FDT_ERR_NOTFOUND;
	}

	if ((info.base == 0U) || (info.clock < 0) || (info.reset < 0)) {
		return -FDT_ERR_BADVALUE;
	}

	pdata->base = (uintptr_t)info.base;
	pdata->clock_id = (unsigned long)info.clock;
	pdata->reset_id = (unsigned int)info.reset;

	return 0;
}

static void clrsetbits(uint32_t *v, uint32_t mask, uint32_t bits)
{
	*v = (*v & ~mask) | bits;
}

static bool algo_mode_needs_iv(uint32_t cr)
{
	return !IS_ALGOMODE(cr, TDES_ECB) && !IS_ALGOMODE(cr, DES_ECB) &&
	       !IS_ALGOMODE(cr, AES_ECB);
}

static bool algo_mode_is_ecb_cbc(uint32_t cr)
{
	return GET_ALGOMODE(cr) < _CRYP_CR_ALGOMODE_AES_CTR;
}

static bool algo_mode_is_aes(uint32_t cr)
{
	return ((cr & _CRYP_CR_ALGOMODE_MSK) >> _CRYP_CR_ALGOMODE_OFF) >=
	       _CRYP_CR_ALGOMODE_AES_ECB;
}

static bool is_decrypt(uint32_t cr)
{
	return (cr & _CRYP_CR_ALGODIR) == _CRYP_CR_ALGODIR_DEC;
}

static bool is_encrypt(uint32_t cr)
{
	return !is_decrypt(cr);
}

static bool does_need_npblb(uint32_t cr)
{
	return (IS_ALGOMODE(cr, AES_GCM) && is_encrypt(cr)) ||
	       (IS_ALGOMODE(cr, AES_CCM) && is_decrypt(cr));
}

static int wait_sr_bits(uintptr_t base, uint32_t bits)
{
	uint64_t timeout = timeout_init_us(CRYP_TIMEOUT_US);

	while ((mmio_read_32(base + _CRYP_SR) & bits) != bits) {
		if (timeout_elapsed(timeout)) {
			ERROR("%s: timeout\n", __func__);
			return -ETIMEDOUT;
		}
	}

	return 0;
}

static int wait_end_busy(uintptr_t base)
{
	uint64_t timeout = timeout_init_us(CRYP_TIMEOUT_US);

	while ((mmio_read_32(base + _CRYP_SR) & _CRYP_SR_BUSY) == _CRYP_SR_BUSY) {
		if (timeout_elapsed(timeout)) {
			ERROR("%s: timeout\n", __func__);
			return -ETIMEDOUT;
		}
	}

	return 0;
}

static int wait_end_enable(uintptr_t base)
{
	uint64_t timeout = timeout_init_us(CRYP_TIMEOUT_US);

	while ((mmio_read_32(base + _CRYP_CR) & _CRYP_CR_CRYPEN) == _CRYP_CR_CRYPEN) {
		if (timeout_elapsed(timeout)) {
			ERROR("%s: timeout\n", __func__);
			return -ETIMEDOUT;
		}
	}

	return 0;
}

static int write_block(struct stm32_cryp_context *ctx, uint32_t *data)
{
	int res = 0;
	unsigned int i = 0;

	assert((uintptr_t)data % __alignof__(uint32_t) == 0);

	res = wait_sr_bits(ctx->base, _CRYP_SR_IFNF);
	if (res != 0) {
		return res;
	}

	for (i = 0; i < ctx->block_u32; i++) {
	/* No need to htobe() as we configure the HW to swap bytes */
		mmio_write_32(ctx->base + _CRYP_DIN, data[i]);
	}

	return 0;
}

static int read_block(struct stm32_cryp_context *ctx, uint32_t *data)
{
	int res = 0;
	unsigned int i = 0;

	assert((uintptr_t)data % __alignof__(uint32_t) == 0);

	res = wait_sr_bits(ctx->base, _CRYP_SR_OFNE);
	if (res != 0) {
		return res;
	}

	for (i = 0; i < ctx->block_u32; i++) {
		/* No need to htobe() as we configure the HW to swap bytes */
		data[i] = mmio_read_32(ctx->base + _CRYP_DOUT);
	}

	return 0;
}

static void cryp_end(struct stm32_cryp_context *ctx, int prev_error)
{
	if (prev_error) {
		if (stm32mp_reset_assert(cryp_pdata.reset_id, TIMEOUT_US_1MS) != 0) {
			panic();
		}

		if (stm32mp_reset_assert(cryp_pdata.reset_id, TIMEOUT_US_1MS) != 0) {
			panic();
		}
	}

	/* Disable the CRYP peripheral */
	mmio_clrbits_32(ctx->base + _CRYP_CR, _CRYP_CR_CRYPEN);
}

static void cryp_write_iv(struct stm32_cryp_context *ctx)
{
	if (algo_mode_needs_iv(ctx->cr)) {
		unsigned int i = 0;

		/* Restore the _CRYP_IVRx */
		for (i = 0; i < ctx->block_u32; i++) {
			mmio_write_32(ctx->base + _CRYP_IV0LR + i * sizeof(uint32_t), ctx->iv[i]);
		}
	}
}

static void cryp_save_suspend(struct stm32_cryp_context *ctx)
{
	unsigned int i = 0;

	if (IS_ALGOMODE(ctx->cr, AES_GCM) || IS_ALGOMODE(ctx->cr, AES_CCM)) {
		for (i = 0; i < ARRAY_SIZE(ctx->pm_gcmccm); i++) {
			ctx->pm_gcmccm[i] = mmio_read_32(ctx->base + _CRYP_CSGCMCCM0R +
			i * sizeof(uint32_t));
		}
	}

	if (IS_ALGOMODE(ctx->cr, AES_GCM)) {
		for (i = 0; i < ARRAY_SIZE(ctx->pm_gcm); i++) {
			ctx->pm_gcm[i] = mmio_read_32(ctx->base + _CRYP_CSGCM0R +
			i * sizeof(uint32_t));
		}
	}
}

static void cryp_restore_suspend(struct stm32_cryp_context *ctx)
{
	unsigned int i = 0;

	if (IS_ALGOMODE(ctx->cr, AES_GCM) || IS_ALGOMODE(ctx->cr, AES_CCM)) {
		for (i = 0; i < ARRAY_SIZE(ctx->pm_gcmccm); i++) {
			mmio_write_32(ctx->base + _CRYP_CSGCMCCM0R + i * sizeof(uint32_t),
				      ctx->pm_gcmccm[i]);
		}
	}

	if (IS_ALGOMODE(ctx->cr, AES_GCM)) {
		for (i = 0; i < ARRAY_SIZE(ctx->pm_gcm); i++) {
			mmio_write_32(ctx->base + _CRYP_CSGCM0R + i * sizeof(uint32_t),
				      ctx->pm_gcm[i]);
		}
	}
}

/* Key sharing mode can be enabled with stm32_cryp_key_sharing_enable */
static int cryp_share_key_start(struct stm32_cryp_context *ctx)
{
	int res = 0;
	uint32_t sr;

	/* Enable Sharing beetwheen SAES and CRYP, SAES must be set first. */
	mmio_clrsetbits_32(ctx->base + _CRYP_CR, _CRYP_CR_KMOD_MSK,
			   _CRYP_CR_KMOD_SHARED << _CRYP_CR_KMOD_OFF);

	res = wait_end_busy(ctx->base);
	if (res != 0) {
		return res;
	}

	/*
	 * If KEYVALID is not set or KERF flag is set when BUSY bit is
	 * cleared in CRYP_SR register, it means that the KEYSIZE value
	 * is incorrect or an unexpected event occurred during the
	 * transfer (such as DPA error, tamper event or KEYVALID in
	 * SAES_SR cleared before the end of the transfer).
	 */
	sr = mmio_read_32(ctx->base + _CRYP_SR);
	if (((sr & _CRYP_SR_KEY_VALID) != _CRYP_SR_KEY_VALID) ||
	    ((sr & _CRYP_SR_KERF) != 0U)) {
		ERROR("%s: Key sharing failed, check SAES state.\n", __func__);
		return -EACCES;
	}

	/*
	 * The key sharing sequence is complete, the CRYP is initialized
	 * with a valid, shared key. The application can then process
	 * data in normal key mode, by writing KMOD[1:0] with 0x0.
	 */
	mmio_clrsetbits_32(ctx->base + _CRYP_CR, _CRYP_CR_KMOD_MSK,
			   _CRYP_CR_KMOD_NORMAL << _CRYP_CR_KMOD_OFF);
	return 0;
}

static int cryp_write_key(struct stm32_cryp_context *ctx)
{
	uintptr_t reg = 0;
	int i = 0;
	uint32_t algo = GET_ALGOMODE(ctx->cr);

	/* Key sharing bypass write process */
	if (ctx->saes_key_share) {
		if (algo == _CRYP_CR_ALGOMODE_DES_ECB || algo == _CRYP_CR_ALGOMODE_DES_CBC) {
			return -ENOTSUP;
		}

		return cryp_share_key_start(ctx);
	}

	if (algo == _CRYP_CR_ALGOMODE_DES_ECB || algo == _CRYP_CR_ALGOMODE_DES_CBC) {
		reg = ctx->base + _CRYP_K1RR;
	} else {
		reg = ctx->base + _CRYP_K3RR;
	}

	for (i = ctx->key_size / sizeof(uint32_t) - 1; i >= 0; i--, reg -= sizeof(uint32_t)) {
		mmio_write_32(reg, ctx->key[i]);
	}

	return 0;
}

static int cryp_prepare_key(struct stm32_cryp_context *ctx)
{
	int res = 0;

	/*
	 * For AES ECB/CBC decryption, key preparation mode must be selected
	 * to populate the key.
	 */
	if (is_decrypt(ctx->cr) && (IS_ALGOMODE(ctx->cr, AES_ECB) ||
				    IS_ALGOMODE(ctx->cr, AES_CBC))) {
		/* Select Algomode "prepare key" */
		mmio_clrsetbits_32(ctx->base + _CRYP_CR, _CRYP_CR_ALGOMODE_MSK,
				   _CRYP_CR_ALGOMODE_AES << _CRYP_CR_ALGOMODE_OFF);

		res = cryp_write_key(ctx);
		if (res) {
			return res;
		}

		/* Enable CRYP */
		mmio_write_32(ctx->base + _CRYP_CR, _CRYP_CR_CRYPEN);

		res = wait_end_busy(ctx->base);
		if (res) {
			return res;
		}

		/* Reset 'real' algomode */
		mmio_clrsetbits_32(ctx->base + _CRYP_CR, _CRYP_CR_ALGOMODE_MSK,
				   ctx->cr & _CRYP_CR_ALGOMODE_MSK);
	} else {
		res = cryp_write_key(ctx);
		if (res) {
			return res;
		}
	}

	return 0;
}

static int save_context(struct stm32_cryp_context *ctx)
{
	/* Device should not be in a processing phase */
	if ((mmio_read_32(ctx->base + _CRYP_SR) & _CRYP_SR_BUSY) == _CRYP_SR_BUSY) {
		return -EBUSY;
	}

	/* Disable the CRYP peripheral */
	mmio_clrbits_32(ctx->base + _CRYP_CR, _CRYP_CR_CRYPEN);

	/* Save CR */
	ctx->cr = mmio_read_32(ctx->base + _CRYP_CR);

	cryp_save_suspend(ctx);

	/* If algo mode needs to save current IV */
	if (algo_mode_needs_iv(ctx->cr)) {
		unsigned int i = 0;

		/* Save IV */
		for (i = 0; i < ctx->block_u32; i++)
			ctx->iv[i] = mmio_read_32(ctx->base + _CRYP_IV0LR + i *
						  sizeof(uint32_t));
	}

	return 0;
}

/* To resume the processing of a message */
static int restore_context(struct stm32_cryp_context *ctx)
{
	int res = 0;

	/* Peripheral should be disabled */
	if ((mmio_read_32(ctx->base + _CRYP_CR) & _CRYP_CR_CRYPEN) == _CRYP_CR_CRYPEN) {
		ERROR("%s: CRYP Device is still enabled\n", __func__);
		return -EBUSY;
	}

	/* Restore the _CRYP_CR */
	mmio_write_32(ctx->base + _CRYP_CR, ctx->cr);

	/* Write key and, in case of AES_CBC or AES_ECB decrypt, prepare it */
	res = cryp_prepare_key(ctx);
	if (res) {
		return res;
	}

	cryp_restore_suspend(ctx);

	cryp_write_iv(ctx);

	/* Flush internal fifo */
	mmio_setbits_32(ctx->base + _CRYP_CR, _CRYP_CR_FFLUSH);

	/* Enable the CRYP peripheral */
	mmio_setbits_32(ctx->base + _CRYP_CR, _CRYP_CR_CRYPEN);

	return 0;
}

/*
 * Translate a byte index in an array of BE uint32_t into the index of same
 * byte in the corresponding LE uint32_t array.
 */
static size_t be_index(size_t index)
{
	return (index & ~0x3) + 3 - (index & 0x3);
}

static int ccm_first_context(struct stm32_cryp_context *ctx)
{
	int res = 0;
	uint32_t b0[AES_BLOCK_NB_U32] = { 0 };
	uint8_t *iv = (uint8_t *)ctx->iv;
	size_t l = 0;
	size_t i = 15;

	/* IP should be disabled */
	if ((mmio_read_32(ctx->base + _CRYP_CR) & _CRYP_CR_CRYPEN) == _CRYP_CR_CRYPEN) {
		ERROR("%s: CRYP Device is still enabled\n", __func__);
		return -EBUSY;
	}

	/* Write the _CRYP_CR */
	mmio_write_32(ctx->base + _CRYP_CR, ctx->cr);

	/* Write key */
	res = cryp_prepare_key(ctx);
	if (res) {
		return res;
	}

	/* Save full IV that will be b0 */
	memcpy(b0, iv, sizeof(b0));

	/*
	 * Update IV to become CTR0/1 before setting it.
	 * IV is saved as LE uint32_t[4] as expected by hardware,
	 * but CCM RFC defines bytes to update in a BE array.
	 */
	/* Set flag bits to 0 (5 higher bits), keep 3 low bits */
	iv[be_index(0)] &= 0x7;
	/* Get size of length field (can be from 2 to 8) */
	l = iv[be_index(0)] + 1;
	/* Set Q to 0 */
	for (i = 15; i >= 15 - l + 1; i--)
		iv[be_index(i)] = 0;
	/* Save CTR0 */
	memcpy(ctx->ctr0_ccm, iv, sizeof(b0));
	/* Increment Q */
	iv[be_index(15)] |= 0x1;

	cryp_write_iv(ctx);

	/* Enable the CRYP peripheral */
	mmio_setbits_32(ctx->base + _CRYP_CR, _CRYP_CR_CRYPEN);

	res = write_block(ctx, b0); /* write align block */

	return res;
}

static int do_from_init_to_phase(struct stm32_cryp_context *ctx, uint32_t new_phase)
{
	int res = 0;

	/*
	 * We didn't run the init phase yet
	 * CCM need a specific restore_context phase for the init phase
	 */
	if (IS_ALGOMODE(ctx->cr, AES_CCM)) {
		res = ccm_first_context(ctx);
	} else {
		res = restore_context(ctx);
	}

	if (res) {
		return res;
	}

	res = wait_end_enable(ctx->base);
	if (res) {
		return res;
	}

	/* Move to 'new_phase' */
	mmio_clrsetbits_32(ctx->base + _CRYP_CR, _CRYP_CR_GCM_CCMPH_MSK,
			   new_phase << _CRYP_CR_GCM_CCMPH_OFF);

	/* Enable the CRYP peripheral (init disabled it) */
	mmio_setbits_32(ctx->base + _CRYP_CR, _CRYP_CR_CRYPEN);

	return 0;
}

static int do_from_header_to_phase(struct stm32_cryp_context *ctx, uint32_t new_phase)
{
	int res = 0;

	res = restore_context(ctx);
	if (res) {
		return res;
	}

	if (ctx->extra_size) {
		/* Manage unaligned header data before moving to next phase */
		memset((uint8_t *)ctx->extra + ctx->extra_size, 0,
		       ctx->block_u32 * sizeof(uint32_t) - ctx->extra_size);

		res = write_block(ctx, ctx->extra); /* write align block */
		if (res) {
			return res;
		}

		ctx->assoc_len += (ctx->extra_size) * UINT8_BIT;
		ctx->extra_size = 0;
	}

	/* Move to 'new_phase' */
	mmio_clrsetbits_32(ctx->base + _CRYP_CR, _CRYP_CR_GCM_CCMPH_MSK,
			   new_phase << _CRYP_CR_GCM_CCMPH_OFF);

	return 0;
}

/**
 * @brief Initialize CRYP driver.
 * @param None.
 * @retval 0 if OK; negative value else.
 */
int stm32_cryp_driver_init(void)
{
	int err;

	if (cryp_pdata.base != 0U) {
		/* Driver is already initialized */
		return 0;
	}

	err = stm32_cryp_parse_fdt(&cryp_pdata);
	if (err != 0) {
		return err;
	}

	clk_enable(cryp_pdata.clock_id);

	if (stm32mp_reset_assert(cryp_pdata.reset_id, TIMEOUT_US_1MS) != 0) {
		panic();
	}

	udelay(CRYP_RESET_DELAY);

	if (stm32mp_reset_deassert(cryp_pdata.reset_id, TIMEOUT_US_1MS) != 0) {
		panic();
	}

	return 0;
}

/**
 * @brief Update cryp context to enable Key sharing with SAES. This only update
 *        the ctx structure.
 * @param ctx: CRYP process context
 * @param enable: true to enable key sharing, false to disable.
 * @note this function doesn't access to hardware but stores in ctx the values.
 * When keysharing is used, Avoid changing KEYSIZE or ALGOMODE[3:0] to a DES or
 * TDES algorithm when the key registers have been initialized with a shared
 * key. This causes key data to be lost and KERF error to be raised.
 */
void stm32_cryp_key_sharing_enable(struct stm32_cryp_context *ctx, bool enable)
{
	if (enable) {
		ctx->saes_key_share = true;
	} else {
		ctx->saes_key_share = false;
	}
}

/**
 * @brief Start a AES computation. This only update the ctx structure
 * @param ctx: CRYP process context
 * @param is_dec: true if decryption, false if encryption
 * @param algo: define the algo mode
 * @param key: pointer to key
 * @param key_size: key size
 * @param iv: pointer to initialization vector (unused if algo is ECB)
 * @param iv_size: iv size
 * @note this function doesn't access to hardware but stores in ctx the values
 *
 * @retval 0 on success.
 */
int stm32_cryp_init(struct stm32_cryp_context *ctx, bool is_dec, enum stm32_cryp_algo_mode algo,
		    const void *key, size_t key_size, const void *iv, size_t iv_size)
{
	unsigned int i = 0;
	const uint32_t *iv_u32 = NULL;
	const uint32_t *key_u32 = NULL;

	ctx->assoc_len = 0;
	ctx->load_len = 0;
	ctx->extra_size = 0;

	ctx->base = cryp_pdata.base;
	ctx->cr = _CRYP_CR_RESET_VALUE;

	/* We want buffer to be u32 aligned */
	assert((uintptr_t)key % __alignof__(uint32_t) == 0);
	assert((uintptr_t)iv % __alignof__(uint32_t) == 0);

	key_u32 = key;
	iv_u32 = iv;

	if (is_dec) {
		SETBITS(ctx->cr, _CRYP_CR_ALGODIR);
	} else {
		CLRBITS(ctx->cr, _CRYP_CR_ALGODIR);
	}

	/* Save algo mode */
	switch (algo) {
	case STM32_CRYP_MODE_TDES_ECB:
		SET_ALGOMODE(TDES_ECB, ctx->cr);
		break;
	case STM32_CRYP_MODE_TDES_CBC:
		SET_ALGOMODE(TDES_CBC, ctx->cr);
		break;
	case STM32_CRYP_MODE_DES_ECB:
		SET_ALGOMODE(DES_ECB, ctx->cr);
		break;
	case STM32_CRYP_MODE_DES_CBC:
		SET_ALGOMODE(DES_CBC, ctx->cr);
		break;
	case STM32_CRYP_MODE_AES_ECB:
		SET_ALGOMODE(AES_ECB, ctx->cr);
		break;
	case STM32_CRYP_MODE_AES_CBC:
		SET_ALGOMODE(AES_CBC, ctx->cr);
		break;
	case STM32_CRYP_MODE_AES_CTR:
		SET_ALGOMODE(AES_CTR, ctx->cr);
		break;
	case STM32_CRYP_MODE_AES_GCM:
		SET_ALGOMODE(AES_GCM, ctx->cr);
		break;
	case STM32_CRYP_MODE_AES_CCM:
		SET_ALGOMODE(AES_CCM, ctx->cr);
		break;
	default:
		return -EINVAL;
	}

	/*
	 * We will use HW Byte swap (_CRYP_CR_DATATYPE_BYTE) for data.
	 * So we won't need to
	 * htobe32(data) before write to DIN register
	 * nor
	 * be32toh after reading from DOUT register.
	 */
	clrsetbits(&ctx->cr, _CRYP_CR_DATATYPE_MSK,
		   _CRYP_CR_DATATYPE_BYTE << _CRYP_CR_DATATYPE_OFF);

	/*
	 * Configure keysize for AES algorithms
	 * And save block size
	 */
	if (algo_mode_is_aes(ctx->cr)) {
		switch (key_size) {
		case AES_KEYSIZE_128:
			clrsetbits(&ctx->cr, _CRYP_CR_KEYSIZE_MSK,
				   _CRYP_CR_KSIZE_128 << _CRYP_CR_KEYSIZE_OFF);
			break;
		case AES_KEYSIZE_192:
			clrsetbits(&ctx->cr, _CRYP_CR_KEYSIZE_MSK,
				   _CRYP_CR_KSIZE_192 << _CRYP_CR_KEYSIZE_OFF);
			break;
		case AES_KEYSIZE_256:
			clrsetbits(&ctx->cr, _CRYP_CR_KEYSIZE_MSK,
				   _CRYP_CR_KSIZE_256 << _CRYP_CR_KEYSIZE_OFF);
			break;
		default:
			return -EINVAL;
		}

		/* And set block size */
		ctx->block_u32 = AES_BLOCK_NB_U32;
	} else {
		/* And set DES/TDES block size */
		ctx->block_u32 = DES_BLOCK_NB_U32;
	}

	/* Save key in HW order */
	ctx->key_size = key_size;

	/* key pointer can be null in case of key sharing */
	if (key_u32 != NULL) {
		for (i = 0; i < key_size / sizeof(uint32_t); i++) {
			ctx->key[i] = htobe32(key_u32[i]);
		}
	}

	/* Save IV */
	if (algo_mode_needs_iv(ctx->cr)) {
		if (!iv || iv_size != ctx->block_u32 * sizeof(uint32_t)) {
			return -EINVAL;
		}

		/*
		 * We save IV in the byte order expected by the
		 * IV registers
		 */
		for (i = 0; i < ctx->block_u32; i++)
			ctx->iv[i] = htobe32(iv_u32[i]);
	}

	/* Reset suspend registers */
	memset(ctx->pm_gcmccm, 0, sizeof(ctx->pm_gcmccm));
	memset(ctx->pm_gcm, 0, sizeof(ctx->pm_gcm));

	return 0;
}

/**
 * @brief Update (or start) a AES authenticate process of
 *	  associated data (CCM or GCM).
 * @param ctx: CRYP process context
 * @param data: pointer to associated data
 * @param data_size: data size
 * @retval 0 if OK.
 */
int stm32_cryp_update_assodata(struct stm32_cryp_context *ctx,
			       uint8_t *data, size_t data_size)
{
	int res = 0;
	unsigned int i = 0;
	uint32_t previous_phase = 0;

	/* If no associated data, nothing to do */
	if (!data || !data_size) {
		return 0;
	}

	previous_phase = (ctx->cr & _CRYP_CR_GCM_CCMPH_MSK) >>
			 _CRYP_CR_GCM_CCMPH_OFF;

	switch (previous_phase) {
	case _CRYP_CR_GCM_CCMPH_INIT:
		res = do_from_init_to_phase(ctx, _CRYP_CR_GCM_CCMPH_HEADER);
		break;
	case _CRYP_CR_GCM_CCMPH_HEADER:
		/*
		 * Function update_assodata was already called.
		 * We only need to restore the context.
		 */
		res = restore_context(ctx);
		break;
	default:
		res = -EINVAL;
		break;
	}

	if (res != 0) {
		goto out;
	}

	/* Manage if remaining data from a previous update_assodata call */
	if (ctx->extra_size &&
	    (ctx->extra_size + data_size >= ctx->block_u32 * sizeof(uint32_t))) {
		uint32_t block[AES_BLOCK_NB_U32] = { 0 };

		memcpy(block, ctx->extra, ctx->extra_size);
		memcpy((uint8_t *)block + ctx->extra_size, data,
		       ctx->block_u32 * sizeof(uint32_t) - ctx->extra_size);

		res = write_block(ctx, block); /* write align block */
		if (res != 0) {
			goto out;
		}

		i += ctx->block_u32 * sizeof(uint32_t) - ctx->extra_size;
		ctx->extra_size = 0;
		ctx->assoc_len += ctx->block_u32 * sizeof(uint32_t) * UINT8_BIT;
	}

	while (data_size - i >= ctx->block_u32 * sizeof(uint32_t)) {
		res = write_block(ctx, (uint32_t*)(data + i));
		if (res != 0) {
			goto out;
		}

		/* Process next block */
		i += ctx->block_u32 * sizeof(uint32_t);
		ctx->assoc_len += ctx->block_u32 * sizeof(uint32_t) * UINT8_BIT;
	}

	/*
	 * Manage last block if not a block size multiple:
	 * Save remaining data to manage them later (potentially with new
	 * associated data).
	 */
	if (i < data_size) {
		memcpy((uint8_t *)ctx->extra + ctx->extra_size, data + i,
		       data_size - i);
		ctx->extra_size += data_size - i;
	}

	res = save_context(ctx);
out:
	if (res != 0) {
		cryp_end(ctx, res);
	}

	return res;
}

/**
 * @brief Update (or start) a AES authenticate and de/encrypt with
 *	  payload data (CCM or GCM).
 * @param ctx: CRYP process context
 * @param data_in: pointer to payload
 * @param data_out: pointer where to save de/encrypted payload
 * @param data_size: payload size
 *
 * @retval 0 if OK.
 */
int stm32_cryp_update_load(struct stm32_cryp_context *ctx,
			   uint8_t *data_in, uint8_t *data_out,
			   size_t data_size)
{
	int res = 0;
	unsigned int i = 0;
	uint32_t previous_phase = 0;

	if (!data_in || !data_size) {
		return 0;
	}

	previous_phase = (ctx->cr & _CRYP_CR_GCM_CCMPH_MSK) >>
			  _CRYP_CR_GCM_CCMPH_OFF;

	switch (previous_phase) {
	case _CRYP_CR_GCM_CCMPH_INIT:
		res = do_from_init_to_phase(ctx, _CRYP_CR_GCM_CCMPH_PAYLOAD);
		break;
	case _CRYP_CR_GCM_CCMPH_HEADER:
		res = do_from_header_to_phase(ctx, _CRYP_CR_GCM_CCMPH_PAYLOAD);
		break;
	case _CRYP_CR_GCM_CCMPH_PAYLOAD:
		/* new update_load call, we only need to restore context */
		res = restore_context(ctx);
		break;
	default:
		res = -EINVAL;
		break;
	}

	if (res != 0) {
		goto out;
	}

	/* Manage if incomplete block from a previous update_load call */
	if (ctx->extra_size &&
	    (ctx->extra_size + data_size >= ctx->block_u32 * sizeof(uint32_t))) {
		uint32_t block_out[AES_BLOCK_NB_U32] = { 0 };

		memcpy((uint8_t *)ctx->extra + ctx->extra_size, data_in + i,
		       ctx->block_u32 * sizeof(uint32_t) - ctx->extra_size);

		res = write_block(ctx, ctx->extra); /* write align block */
		if (res != 0) {
			goto out;
		}

		res = read_block(ctx, block_out); /* read align block */
		if (res != 0) {
			goto out;
		}

		memcpy(data_out + i, (uint8_t *)block_out + ctx->extra_size,
		       ctx->block_u32 * sizeof(uint32_t) - ctx->extra_size);

		i += ctx->block_u32 * sizeof(uint32_t) - ctx->extra_size;
		ctx->extra_size = 0;

		ctx->load_len += ctx->block_u32 * sizeof(uint32_t) * UINT8_BIT;
	}

	while (data_size - i >= ctx->block_u32 * sizeof(uint32_t)) {
		res = write_block(ctx, (uint32_t*)(data_in + i));
		if (res != 0) {
			goto out;
		}

		res = read_block(ctx, (uint32_t*)(data_out + i));
		if (res != 0) {
			goto out;
		}

		/* Process next block */
		i += ctx->block_u32 * sizeof(uint32_t);
		ctx->load_len += ctx->block_u32 * sizeof(uint32_t) * UINT8_BIT;
	}

	res = save_context(ctx);
	if (res != 0) {
		goto out;
	}

	/*
	 * Manage last block if not a block size multiple
	 * We saved context,
	 * Complete block with 0 and send to CRYP to get {en,de}crypted data
	 * Store data to resend as last block in final()
	 * or to complete next update_load() to get correct tag.
	 */
	if (i < data_size) {
		uint32_t block_out[AES_BLOCK_NB_U32] = { 0 };
		size_t prev_extra_size = ctx->extra_size;

		/* Re-enable the CRYP peripheral */
		mmio_setbits_32(ctx->base + _CRYP_CR, _CRYP_CR_CRYPEN);

		memcpy((uint8_t *)ctx->extra + ctx->extra_size, data_in + i,
		       data_size - i);
		ctx->extra_size += data_size - i;
		memset((uint8_t *)ctx->extra + ctx->extra_size, 0,
		       ctx->block_u32 * sizeof(uint32_t) - ctx->extra_size);

		res = write_block(ctx, ctx->extra); /* write align block */
		if (res != 0) {
			goto out;
		}

		res = read_block(ctx, block_out); /* read align block */
		if (res != 0) {
			goto out;
		}

		memcpy(data_out + i, (uint8_t *)block_out + prev_extra_size,
		       data_size - i);

		/* Disable the CRYP peripheral */
		mmio_clrbits_32(ctx->base + _CRYP_CR, _CRYP_CR_CRYPEN);
	}

out:
	if (res != 0) {
		cryp_end(ctx, res);
	}

	return res;
}

/**
 * @brief Get authentication tag for AES authenticated algorithms (CCM or GCM).
 * @param ctx: CRYP process context
 * @param tag: pointer where to save the tag
 * @param data_size: tag size
 *
 * @retval 0 if OK.
 */
int stm32_cryp_final(struct stm32_cryp_context *ctx, uint8_t *tag,
		     size_t tag_size)
{
	int res = 0;
	uint32_t tag_u32[4] = { 0 };
	uint32_t previous_phase = 0;

	previous_phase = (ctx->cr & _CRYP_CR_GCM_CCMPH_MSK) >>
			 _CRYP_CR_GCM_CCMPH_OFF;

	switch (previous_phase) {
	case _CRYP_CR_GCM_CCMPH_INIT:
		res = do_from_init_to_phase(ctx, _CRYP_CR_GCM_CCMPH_FINAL);
		break;
	case _CRYP_CR_GCM_CCMPH_HEADER:
		res = do_from_header_to_phase(ctx, _CRYP_CR_GCM_CCMPH_FINAL);
		break;
	case _CRYP_CR_GCM_CCMPH_PAYLOAD:
		res = restore_context(ctx);
		if (res != 0) {
			break;
		}

		/* Manage if incomplete block from a previous update_load() */
		if (ctx->extra_size) {
			uint32_t block_out[AES_BLOCK_NB_U32] = { 0 };
			size_t sz = ctx->block_u32 * sizeof(uint32_t) -
				    ctx->extra_size;

			if (does_need_npblb(ctx->cr)) {
				mmio_clrsetbits_32(ctx->base + _CRYP_CR, _CRYP_CR_NPBLB_MSK,
						   sz << _CRYP_CR_NPBLB_OFF);
			}

			memset((uint8_t *)ctx->extra + ctx->extra_size, 0, sz);

			res = write_block(ctx, ctx->extra); /* write align block */
			if (res != 0) {
				break;
			}

			/* Don't care {en,de}crypted data, already saved */
			res = read_block(ctx, block_out); /* read align block */
			if (res != 0) {
				break;
			}

			ctx->load_len += (ctx->extra_size * UINT8_BIT);
			ctx->extra_size = 0;
		}

		/* Move to final phase */
		mmio_clrsetbits_32(ctx->base + _CRYP_CR, _CRYP_CR_GCM_CCMPH_MSK,
				   _CRYP_CR_GCM_CCMPH_FINAL <<
				   _CRYP_CR_GCM_CCMPH_OFF);
		break;
	default:
		res = -EINVAL;
		break;
	}

	if (res != 0) {
		goto out;
	}

	if (IS_ALGOMODE(ctx->cr, AES_GCM)) {
		/* No need to htobe() as we configure the HW to swap bytes */
		mmio_write_32(ctx->base + _CRYP_DIN, 0U);
		mmio_write_32(ctx->base + _CRYP_DIN, ctx->assoc_len);
		mmio_write_32(ctx->base + _CRYP_DIN, 0U);
		mmio_write_32(ctx->base + _CRYP_DIN, ctx->load_len);
	} else if (IS_ALGOMODE(ctx->cr, AES_CCM)) {
		/* No need to htobe() in this phase */
		res = write_block(ctx, ctx->ctr0_ccm); /* write align block */
		if (res != 0) {
			goto out;
		}
	}

	res = read_block(ctx, tag_u32); /* read align block */
	if (res != 0) {
		goto out;
	}

	memcpy(tag, tag_u32, MIN(sizeof(tag_u32), tag_size));

out:
	cryp_end(ctx, res);

	return res;
}

/**
 * @brief Update (or start) a de/encrypt process.
 * @param ctx: CRYP process context
 * @param last_block: true if last payload data block
 * @param data_in: pointer to payload
 * @param data_out: pointer where to save de/encrypted payload
 * @param data_size: payload size
 *
 * @retval 0 if OK.
 */
int stm32_cryp_update(struct stm32_cryp_context *ctx, bool last_block,
		      uint8_t *data_in, uint8_t *data_out,
		      size_t data_size)
{
	int res = 0;
	unsigned int i = 0;

	/*
	 * In CBC and ECB encryption we need to manage specifically last
	 * 2 blocks if total size in not aligned to a block size.
	 * Currently return ENOTSUP. Moreover as we need to
	 * know last 2 blocks, if unaligned and call with less than two blocks,
	 * return -EINVAL.
	 */
	if (last_block && algo_mode_is_ecb_cbc(ctx->cr) && is_encrypt(ctx->cr) &&
	    (round_down(data_size, ctx->block_u32 * sizeof(uint32_t)) != data_size)) {
		if (data_size < ctx->block_u32 * sizeof(uint32_t) * 2) {
			/*
			 * If CBC, size of the last part should be at
			 * least 2*BLOCK_SIZE
			 */
			ERROR("%s: unexpected last block size\n", __func__);
			res = -EINVAL;
			goto out;
		}
		/*
		 * Moreover the ECB/CBC specific padding for encrypt is not
		 * yet implemented, and not used in OPTEE
		 */
		res = -ENOTSUP;
		goto out;
	}

	/* Manage remaining CTR mask from previous update call */
	if (IS_ALGOMODE(ctx->cr, AES_CTR) && ctx->extra_size) {
		unsigned int j = 0;
		uint8_t *mask = (uint8_t *)ctx->extra;

		for (j = 0; j < ctx->extra_size && i < data_size; j++, i++)
			data_out[i] = data_in[i] ^ mask[j];

		if (j != ctx->extra_size) {
			/*
			 * We didn't consume all saved mask,
			 * but no more data.
			 */

			/* We save remaining mask and its new size */
			memmove(ctx->extra, (uint8_t *)ctx->extra + j,
				ctx->extra_size - j);
			ctx->extra_size -= j;

			/*
			 * We don't need to save HW context we didn't
			 * modify HW state.
			 */
			res = 0;
			goto out;
		}

		/* All extra mask consumed */
		ctx->extra_size = 0;
	}

	res = restore_context(ctx);
	if (res != 0) {
		goto out;
	}

	while (data_size - i >= ctx->block_u32 * sizeof(uint32_t)) {
		/*
		 * We only write/read one block at a time
		 * but CRYP use a in (and out) FIFO of 8 * uint32_t
		 */
		res = write_block(ctx, (uint32_t*)(data_in + i));
		if (res != 0) {
			goto out;
		}

		res = read_block(ctx, (uint32_t*)(data_out + i));
		if (res != 0) {
			goto out;
		}

		/* Process next block */
		i += ctx->block_u32 * sizeof(uint32_t);
	}

	/* Manage last block if not a block size multiple */
	if (i < data_size) {
		uint32_t block_in[AES_BLOCK_NB_U32] = { 0 };
		uint32_t block_out[AES_BLOCK_NB_U32] = { 0 };

		if (!IS_ALGOMODE(ctx->cr, AES_CTR)) {
			/*
			 * Other algorithm than CTR can manage only multiple
			 * of block_size.
			 */
			res = -EINVAL;
			goto out;
		}

		/*
		 * For CTR we save the generated mask to use it at next
		 * update call.
		 */
		memcpy(block_in, data_in + i, data_size - i);

		res = write_block(ctx, block_in); /* write align block */
		if (res != 0) {
			goto out;
		}

		res = read_block(ctx, block_out); /* read align block */
		if (res != 0) {
			goto out;
		}

		memcpy(data_out + i, block_out, data_size - i);

		/* Save mask for possibly next call */
		ctx->extra_size = ctx->block_u32 * sizeof(uint32_t) -
			(data_size - i);
		memcpy(ctx->extra, (uint8_t *)block_out + data_size - i,
		       ctx->extra_size);
	}

	if (!last_block) {
		res = save_context(ctx);
	}

out:
	/* If last block or error, end of CRYP process */
	if (last_block || (res != 0)) {
		cryp_end(ctx, res);
	}

	return res;
}
