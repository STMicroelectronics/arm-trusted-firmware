/*
 * Copyright (c) 2024, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <errno.h>
#include <stdint.h>

#include <arch_helpers.h>
#include <common/debug.h>
#include <common/desc_image_load.h>
#include <drivers/clk.h>
#include <drivers/delay_timer.h>
#include <drivers/generic_delay_timer.h>
#include <drivers/mmc.h>
#include <drivers/st/bsec.h>
#include <drivers/st/regulator.h>
#include <drivers/st/regulator_fixed.h>
#include <drivers/st/stm32_console.h>
#include <drivers/st/stm32_hash.h>
#include <drivers/st/stm32_iwdg.h>
#include <drivers/st/stm32_rifsc.h>
#include <drivers/st/stm32_rng.h>
#include <drivers/st/stm32_saes.h>
#include <drivers/st/stm32_uart.h>
#include <drivers/st/stm32mp_pmic2.h>
#include <drivers/st/stm32mp_reset.h>
#include <drivers/st/stm32mp_rifsc_regs.h>
#include <drivers/st/stm32mp_risab_regs.h>
#include <drivers/st/stm32mp2_ddr_helpers.h>
#include <drivers/st/stm32mp2_ram.h>
#include <drivers/st/stm32mp2_risaf.h>
#include <lib/fconf/fconf.h>
#include <lib/fconf/fconf_dyn_cfg_getter.h>
#include <lib/mmio.h>
#include <lib/optee_utils.h>
#include <lib/xlat_tables/xlat_tables_v2.h>
#include <plat/common/platform.h>

#include <platform_def.h>
#include <stm32mp_common.h>
#include <stm32mp_dt.h>
#include <stm32mp2_context.h>

#define BOOT_CTX_ADDR	0x0e000020UL

IMPORT_SYM(uintptr_t, __BSS_START__, BSS_START);
IMPORT_SYM(uintptr_t, __BSS_END__, BSS_END);
IMPORT_SYM(uintptr_t, __DATA_START__, DATA_START);
IMPORT_SYM(uintptr_t, __DATA_END__, DATA_END);

#define IAC_EXCEPT_LSB_BIT(x) ((x) * 32U)
#define IAC_EXCEPT_MSB_BIT(x) (IAC_EXCEPT_LSB_BIT(x) + 31U)

static void iac_dump(void)
{
#if !STM32MP_M33_TDCID
#if DEBUG
	unsigned int i;
	unsigned int bit;

	for (i = 0U; i < IAC_NB; i++) {
		uint32_t isr = mmio_read_32(IAC_BASE + IAC_ISR(i));
		uint32_t ier = mmio_read_32(IAC_BASE + IAC_IER(i));

		isr &= ier;
		if (isr == 0U) {
			continue;
		}

		WARN("IAC exceptions pending [%u:%u] = %x\n",
		     IAC_EXCEPT_MSB_BIT(i), IAC_EXCEPT_LSB_BIT(i), isr);
		bit = 0U;
		while ((isr != 0U) && (bit < 32U)) {
			if ((isr & BIT(0)) != 0U) {
				WARN(" - %u\n", IAC_EXCEPT_LSB_BIT(i) + bit);
			}
			isr >>= 1U;
			bit++;
		}
	}
#endif
#endif
}

static void print_reset_reason(void)
{
	uint32_t rstsr = mmio_read_32(stm32mp_rcc_base() + RCC_C1BOOTRSTSCLRR);

	if (rstsr == 0U) {
		WARN("Reset reason unknown\n");
		return;
	}

	INFO("Reset reason (0x%x):\n", rstsr);

	if ((rstsr & RCC_C1BOOTRSTSCLRR_PADRSTF) == 0U) {
		if ((rstsr & RCC_C1BOOTRSTSCLRR_STBYC1RSTF) != 0U) {
			INFO("System exits from Standby for CA35\n");
			return;
		}

		if ((rstsr & RCC_C1BOOTRSTSCLRR_D1STBYRSTF) != 0U) {
			INFO("D1 domain exits from DStandby\n");
			return;
		}
	}

	if ((rstsr & RCC_C1BOOTRSTSCLRR_PORRSTF) != 0U) {
		INFO("  Power-on Reset (rst_por)\n");
		return;
	}

	if ((rstsr & RCC_C1BOOTRSTSCLRR_BORRSTF) != 0U) {
		INFO("  Brownout Reset (rst_bor)\n");
		return;
	}

	if ((rstsr & RCC_C1BOOTRSTSSETR_SYSC2RSTF) != 0U) {
		INFO("  System reset (SYSRST) by M33\n");
		return;
	}

	if ((rstsr & RCC_C1BOOTRSTSSETR_SYSC1RSTF) != 0U) {
		INFO("  System reset (SYSRST) by A35\n");
		return;
	}

	if ((rstsr & RCC_C1BOOTRSTSCLRR_HCSSRSTF) != 0U) {
		INFO("  Clock failure on HSE\n");
		return;
	}

	if ((rstsr & RCC_C1BOOTRSTSCLRR_IWDG1SYSRSTF) != 0U) {
		INFO("  IWDG1 system reset (rst_iwdg1)\n");
		return;
	}

	if ((rstsr & RCC_C1BOOTRSTSCLRR_IWDG2SYSRSTF) != 0U) {
		INFO("  IWDG2 system reset (rst_iwdg2)\n");
		return;
	}

	if ((rstsr & RCC_C1BOOTRSTSCLRR_IWDG3SYSRSTF) != 0U) {
		INFO("  IWDG3 system reset (rst_iwdg3)\n");
		return;
	}

	if ((rstsr & RCC_C1BOOTRSTSCLRR_IWDG4SYSRSTF) != 0U) {
		INFO("  IWDG4 system reset (rst_iwdg4)\n");
		return;
	}

	if ((rstsr & RCC_C1BOOTRSTSCLRR_IWDG5SYSRSTF) != 0U) {
		INFO("  IWDG5 system reset (rst_iwdg5)\n");
		return;
	}

	if ((rstsr & RCC_C1BOOTRSTSCLRR_C1P1RSTF) != 0U) {
		INFO("  A35 processor core 1 reset\n");
		return;
	}

	if ((rstsr & RCC_C1BOOTRSTSCLRR_PADRSTF) != 0U) {
		INFO("  Pad Reset from NRST\n");
		return;
	}

	if ((rstsr & RCC_C1BOOTRSTSCLRR_VCORERSTF) != 0U) {
		INFO("  Reset due to a failure of VDD_CORE\n");
		return;
	}

	if ((rstsr & RCC_C1BOOTRSTSCLRR_C1RSTF) != 0U) {
		INFO("  A35 processor reset\n");
		return;
	}

	ERROR("  Unidentified reset reason\n");
}

void bl2_el3_early_platform_setup(u_register_t arg0 __unused,
				  u_register_t arg1 __unused,
				  u_register_t arg2 __unused,
				  u_register_t arg3 __unused)
{
	stm32mp_setup_early_console();

	stm32mp_save_boot_ctx_address(BOOT_CTX_ADDR);

	stm32_save_header();
}

void bl2_platform_setup(void)
{
	int ret;

#if !STM32MP_M33_TDCID
	ret = stm32mp2_ddr_probe();
	if (ret != 0) {
		ERROR("DDR probe: error %d\n", ret);
		panic();
	}

	if (stm32mp2_risaf_init() < 0) {
		panic();
	}
#endif

	/* Map DDR for binary load, now with cacheable attribute */
	ret = mmap_add_dynamic_region(STM32MP_DDR_BASE, STM32MP_DDR_BASE,
				      STM32MP_DDR_MAX_SIZE, MT_MEMORY | MT_RW | MT_SECURE);
	if (ret < 0) {
		ERROR("DDR mapping: error %d\n", ret);
		panic();
	}

#if !STM32MP_M33_TDCID
	/* Set QOS ICN priority */
	stm32mp_syscfg_set_icn_qos();
#endif
}

static void handle_potential_tamper(uint32_t bit_off)
{
	/* Fixme: Add implementation specific logic here */
	ERROR("Handling Potential tamper\n");
	mmio_setbits_32(TAMP_BASE + TAMP_SCR, BIT_32(bit_off));
}

static void handle_confirmed_tamper(uint32_t bit_off __unused)
{
	/* Fixme: Add implementation specific logic here */
	ERROR("Handling Confirmed tamper\n");
	panic();
}

static bool lse_tamper_detection(void)
{
	if ((mmio_read_32(TAMP_BASE + TAMP_SR) & TAMP_SR_LSE_MONITORING) != 0U) {
		mmio_clrbits_32(RCC_BASE + RCC_BDCR, RCC_BDCR_LSECSSON);
		mmio_clrbits_32(RCC_BASE + RCC_BDCR, RCC_BDCR_LSEON);
		if (mmio_read_32(RCC_BASE + RCC_C1CIFCLRR) & RCC_C1CIFCLRR_LSECSSF) {
			mmio_clrbits_32(RCC_BASE + RCC_C1CIFCLRR, RCC_C1CIFCLRR_LSECSSF);
		}

		return true;
	}

	return false;
}

static void reset_backup_domain(void)
{
#if !STM32MP_M33_TDCID
	uintptr_t pwr_base = stm32mp_pwr_base();
	uintptr_t rcc_base = stm32mp_rcc_base();

	/*
	 * Disable the backup domain write protection.
	 * The protection is enable at each reset by hardware
	 * and must be disabled by software.
	 */
	mmio_setbits_32(pwr_base + PWR_BDCR1, PWR_BDCR1_DBD3P);

	while ((mmio_read_32(pwr_base + PWR_BDCR1) & PWR_BDCR1_DBD3P) == 0U) {
		;
	}

	/* Reset backup domain on cold boot cases or when LSE tamper occurred */
	if ((mmio_read_32(rcc_base + RCC_BDCR) & RCC_BDCR_RTCCKEN) == 0U) {
		mmio_setbits_32(rcc_base + RCC_BDCR, RCC_BDCR_VSWRST);

		while ((mmio_read_32(rcc_base + RCC_BDCR) & RCC_BDCR_VSWRST) == 0U) {
			;
		}

		mmio_clrbits_32(rcc_base + RCC_BDCR, RCC_BDCR_VSWRST);
	}
#endif
}

static void check_tamper_event(bool lse_tamper_occured)
{
	uint32_t sr = mmio_read_32(TAMP_BASE + TAMP_SR);

	if (sr == 0U) {
		return;
	}

	ERROR("\n");
	if (lse_tamper_occured) {
		ERROR("** INTRUSION ALERT: LSE MONITORING TAMPER DETECTED **\n");
		ERROR("\n");

#if !STM32MP_M33_TDCID
		/*
		 * Fixme: Add logic to handle the LSE tamper here (e.g change RTC clock source
		 * instead). This part is implementation specific.
		 */
		mmio_clrbits_32(RCC_BASE + RCC_BDCR, RCC_BDCR_RTCCKEN);
		ERROR("** Rebooting... **\n");
		stm32mp_system_reset();
#endif
	} else {
		while (sr != 0U) {
			unsigned int bit_off = __builtin_ctz(sr);
			bool is_internal = bit_off >= TAMP_SR_INT_SHIFT;
			uint32_t cr2 = mmio_read_32(TAMP_BASE + TAMP_CR2);
			uint32_t cr3 = mmio_read_32(TAMP_BASE + TAMP_CR3);

			ERROR("** INTRUSION ALERT: %s TAMPER %u DETECTED **\n",
			      is_internal ? "INTERNAL" : "EXTERNAL",
			      is_internal ? (bit_off - TAMP_SR_INT_SHIFT + 1U) : (bit_off + 1U));

			if ((is_internal && ((cr3 & BIT_32(bit_off - TAMP_SR_INT_SHIFT)) != 0U)) ||
			    (!is_internal && ((cr2 & BIT_32(bit_off)) != 0U))) {
				handle_potential_tamper(bit_off);
			} else {
				handle_confirmed_tamper(bit_off);
			}

			sr &= ~BIT_32(bit_off);
		}
		ERROR("\n");
	}
}

#if TRUSTED_BOARD_BOOT && DYN_DISABLE_AUTH
static bool authentication_check(boot_api_context_t *boot_context)
{
	if (boot_context->auth_status == BOOT_API_CTX_AUTH_FAILED) {
		return false;
	}

	if (stm32mp_check_closed_device() != STM32MP_CHIP_SEC_CLOSED) {
		void *pk_ptr = NULL;
		unsigned int len = 0U;
		unsigned int flags = 0U;
		int rc;

		rc = plat_get_rotpk_info(NULL, &pk_ptr, &len, &flags);
		if ((rc == -EINVAL) || ((flags & ROTPK_NOT_DEPLOYED) > 0)) {
			return false;
		}
	}

	return true;
}
#endif /* TRUSTED_BOARD_BOOT && DYN_DISABLE_AUTH */

void bl2_el3_plat_arch_setup(void)
{
	const char *board_model;
	boot_api_context_t *boot_context =
		(boot_api_context_t *)stm32mp_get_boot_ctx_address();
	bool serial_uart_interface __unused =
				(boot_context->boot_interface_selected ==
				 BOOT_API_CTX_BOOT_INTERFACE_SEL_SERIAL_UART);
	uintptr_t uart_prog_addr __unused;
	bool lse_tamper_occured = false;

	if (stm32_otp_probe() != 0) {
		panic();
	}

	mmap_add_region(BL_CODE_BASE, BL_CODE_BASE,
			BL_CODE_END - BL_CODE_BASE,
			MT_CODE | MT_SECURE);

	configure_mmu();

	/* Prevent corruption of preloaded Device Tree */
	mmap_add_dynamic_region(DTB_BASE, DTB_BASE,
				DTB_LIMIT - DTB_BASE,
				MT_RO_DATA | MT_SECURE);

	if (dt_open_and_check(STM32MP_DTB_BASE) < 0) {
		panic();
	}

	lse_tamper_occured = lse_tamper_detection();

	reset_backup_domain();

#if !STM32MP_M33_TDCID
	/*
	 * Initialize DDR sub-system clock. This needs to be done before enabling DDR PLL (PLL2),
	 * and so before stm32mp2_clk_init().
	 */
	ddr_sub_system_clk_init();
#endif

	if (stm32mp2_clk_init() < 0) {
		panic();
	}

	stm32_tamp_nvram_init();

#if STM32MP_UART_PROGRAMMER
	uart_prog_addr = get_uart_address(boot_context->boot_interface_instance);

	/* Disable programmer UART before changing clock tree */
	if (serial_uart_interface) {
		stm32_uart_stop(uart_prog_addr);
	}
#endif

	if (stm32_iwdg_init() < 0) {
		panic();
	}

	stm32_iwdg_refresh();

	stm32_save_boot_info(boot_context);

	if (stm32mp_uart_console_setup() != 0) {
		goto skip_console_init;
	}

	iac_dump();

	/* Masking potential tamper during BL2 */
	stm32mp_syscfg_mask_potential_tamper_enable();

	stm32mp_print_cpuinfo();

	board_model = dt_get_board_model();
	if (board_model != NULL) {
		NOTICE("Model: %s\n", board_model);
	}

	stm32mp_print_boardinfo();

	print_reset_reason();

#if TRUSTED_BOARD_BOOT && DYN_DISABLE_AUTH
	if (stm32_hash_register() != 0) {
		ERROR("HASH register fail\n");
		panic();
	}

	if (boot_context->auth_status != BOOT_API_CTX_AUTH_NO) {
		NOTICE("Bootrom authentication %s\n",
		       authentication_check(boot_context) ? "succeeded" : "failed");
	}
#endif /* TRUSTED_BOARD_BOOT && DYN_DISABLE_AUTH */

skip_console_init:
	check_tamper_event(lse_tamper_occured);

#if !TRUSTED_BOARD_BOOT
	if (stm32mp_check_closed_device() == STM32MP_CHIP_SEC_CLOSED) {
		/* Closed chip mandates authentication */
		ERROR("Secure chip: TRUSTED_BOARD_BOOT must be enabled\n");
		panic();
	}
#endif

#if !STM32MP_M33_TDCID
	if (stm32_rifsc_semaphore_init() != 0) {
		panic();
	}
#endif

	if (stm32_rng_init() != 0) {
		panic();
	}

	if (fixed_regulator_register() != 0) {
		panic();
	}

	if (dt_pmic_status() > 0) {
		initialize_pmic();
	}

	fconf_populate("TB_FW", STM32MP_DTB_BASE);

#if STM32MP_DDR_FIP_IO_STORAGE || TRUSTED_BOARD_BOOT
#if !STM32MP_M33_TDCID
	/*
	 * RISAB3 setup (dedicated for SRAM1)
	 *
	 * Allow secure read/writes data accesses to non-secure
	 * blocks or pages, all RISAB registers are writable.
	 * DDR firmwares are saved there before being loaded in DDRPHY memory.
	 */
	mmio_write_32(RISAB3_BASE + RISAB_CR, RISAB_CR_SRWIAD);
#endif
#endif /* STM32MP_DDR_FIP_IO_STORAGE || TRUSTED_BOARD_BOOT */
#if STM32MP_USB_PROGRAMMER
	/*
	 * Set USB3DR Peripheriphal accesses to Secure/Privilege only
	 */
	mmio_setbits_32(RIFSC_BASE + _RIFSC_RISC_SECCFGR(STM32MP25_RIFSC_USB3DR_ID),
			RIFSC_USB3DR_SEC);
	mmio_setbits_32(RIFSC_BASE + _RIFSC_RISC_PRIVCFGR(STM32MP25_RIFSC_USB3DR_ID),
			RIFSC_USB3DR_PRIV);

	/*
	 * Apply USB boot specific configuration to RIF master USB3DR
	 */
	mmio_write_32(RIFSC_BASE + _RIFSC_RIMC_ATTR(STM32MP25_RIMU_USB3DR),
		      RIFSC_USB_BOOT_USBDR_RIMC_CONF);
#endif /* STM32MP_USB_PROGRAMMER */

#if !STM32MP_M33_TDCID
	/*
	 * RISAB5 setup (dedicated for RETRAM)
	 *
	 * Allow secure read/writes data accesses to non-secure
	 * blocks or pages, all RISAB registers are writable.
	 * DDR retention registers are saved there and restored
	 * when exiting standby low power state.
	 */
	mmio_write_32(RISAB5_BASE + RISAB_CR, RISAB_CR_SRWIAD);

	if (stm32mp2_pwr_init_io_domains() != 0) {
		panic();
	}
#endif

#if STM32MP_DDR_FIP_IO_STORAGE && !STM32MP_UART_PROGRAMMER && !STM32MP_USB_PROGRAMMER
	/* Skip DDR FW ID = the first image to load for standby exit */
	if (stm32mp_is_wakeup_from_standby()) {
		bl_mem_params_node_t *bl_mem_params;

		bl_mem_params = get_bl_mem_params_node(DDR_FW_ID);
		assert(bl_mem_params != NULL);
		bl_mem_params->image_info.h.attr |= IMAGE_ATTRIB_SKIP_LOADING;
	}
#endif

	stm32mp_io_setup();
}

#if STM32MP_M33_TDCID
static inline void prepare_encryption(void) {}
#else /* !STM32MP_M33_TDCID */
static int generate_enc_mkey_from_seed(uint8_t *mkey, uint8_t *seed, uint32_t seed_size){
	int ret;
	struct stm32_saes_context ctx;
	uint8_t payload[RISAF_KEY_SIZE_IN_BYTES] = {0U};

	assert(seed_size <= RISAF_KEY_SIZE_IN_BYTES);

	ret = stm32_saes_driver_init();
	if (ret != 0) {
		return ret;
	}

	memcpy(payload, seed, seed_size); /* add seed in a block sized payload */

	ret = stm32_saes_init(&ctx, false, STM32_SAES_MODE_ECB,
			      STM32_SAES_KEY_DHU, NULL, RISAF_KEY_SIZE_IN_BYTES, NULL, 0U);
	if (ret != 0) {
		return ret;
	}

	return stm32_saes_update(&ctx, true, payload, mkey, sizeof(payload));
}

static void prepare_encryption(void)
{
	uint8_t mkey[RISAF_KEY_SIZE_IN_BYTES];
	uint8_t seed[RISAF_SEED_SIZE_IN_BYTES];

#if STM32MP_UART_PROGRAMMER || STM32MP_USB_PROGRAMMER
	if (stm32_rng_read(seed, RISAF_SEED_SIZE_IN_BYTES) != 0) {
		panic();
	}
#else /* STM32MP_UART_PROGRAMMER || STM32MP_USB_PROGRAMMER */
	if (stm32mp_is_wakeup_from_standby()) {
		stm32mp_pm_get_enc_mkey_seed_from_context(seed);
	} else {
		/* Generate RISAF master key from RNG */
		if (stm32_rng_read(seed, RISAF_SEED_SIZE_IN_BYTES) != 0) {
			panic();
		}

		stm32mp_pm_save_enc_mkey_seed_in_context(seed);
	}
#endif /* STM32MP_UART_PROGRAMMER || STM32MP_USB_PROGRAMMER */

	if (generate_enc_mkey_from_seed(mkey, seed, RISAF_SEED_SIZE_IN_BYTES) != 0) {
		panic();
	}

	if (stm32mp2_risaf_write_master_key(RISAF4_INST, mkey) != 0) {
		panic();
	}
}
#endif /* STM32MP_M33_TDCID */

/*******************************************************************************
 * This function can be used by the platforms to update/use image
 * information for given `image_id`.
 ******************************************************************************/
int bl2_plat_handle_post_image_load(unsigned int image_id)
{
	int err = 0;
	bl_mem_params_node_t *bl_mem_params = get_bl_mem_params_node(image_id);
	bl_mem_params_node_t *pager_mem_params __unused;
	const struct dyn_cfg_dtb_info_t *config_info;
	unsigned int i;
	unsigned long long ddr_top __unused;
	const unsigned int image_ids[] = {
		BL31_IMAGE_ID,
		SOC_FW_CONFIG_ID,
		BL32_IMAGE_ID,
		BL33_IMAGE_ID,
		HW_CONFIG_ID,
	};
	uint32_t otp_idx __maybe_unused;
	uint32_t otp_len __maybe_unused;
	bool wakeup_standby;

	assert(bl_mem_params != NULL);

#if STM32MP_SDMMC || STM32MP_EMMC
	/*
	 * Invalidate remaining data read from MMC but not flushed by load_image_flush().
	 * We take the worst case which is 2 MMC blocks.
	 */
	if ((image_id != FW_CONFIG_ID) &&
	    ((bl_mem_params->image_info.h.attr & IMAGE_ATTRIB_SKIP_LOADING) == 0U)) {
		inv_dcache_range(bl_mem_params->image_info.image_base +
				 bl_mem_params->image_info.image_size,
				 2U * MMC_BLOCK_SIZE);
	}
#endif /* STM32MP_SDMMC || STM32MP_EMMC */

	switch (image_id) {
	case FW_CONFIG_ID:
		if ((stm32mp_check_closed_device() == STM32MP_CHIP_SEC_CLOSED) ||
		    stm32mp_is_auth_supported()) {
			prepare_encryption();
		}

		/* Set global DTB info for fixed fw_config information */
		set_config_info(STM32MP_FW_CONFIG_BASE, ~0UL, STM32MP_FW_CONFIG_MAX_SIZE,
				FW_CONFIG_ID);
		fconf_populate("FW_CONFIG", STM32MP_FW_CONFIG_BASE);

#if STM32MP_UART_PROGRAMMER || STM32MP_USB_PROGRAMMER
		wakeup_standby = false;
#else /* STM32MP_UART_PROGRAMMER || STM32MP_USB_PROGRAMMER */
		wakeup_standby = stm32mp_is_wakeup_from_standby();
#endif /* STM32MP_UART_PROGRAMMER || STM32MP_USB_PROGRAMMER */

		/* Iterate through all the fw config IDs */
		for (i = 0U; i < ARRAY_SIZE(image_ids); i++) {
			bl_mem_params = get_bl_mem_params_node(image_ids[i]);
			assert(bl_mem_params != NULL);

			config_info = FCONF_GET_PROPERTY(dyn_cfg, dtb, image_ids[i]);
			if (config_info == NULL) {
				continue;
			}

			bl_mem_params->image_info.image_base = config_info->config_addr;
			bl_mem_params->image_info.image_max_size = config_info->config_max_size;

			/*
			 * If not going back from STANDBY (preserve the DDR in Self-Refresh)
			 * or partition not located in DDR
			 * the partitions must be loaded = remove the SKIP flag
			 */
			if (!wakeup_standby || (config_info->config_addr < STM32MP_DDR_BASE)) {
				bl_mem_params->image_info.h.attr &= ~IMAGE_ATTRIB_SKIP_LOADING;
			}

			switch (image_ids[i]) {
			case BL31_IMAGE_ID:
				bl_mem_params->ep_info.pc = config_info->config_addr;
				if (bl_mem_params->image_info.image_base == STM32MP_SYSRAM_BASE) {
					bl_mem_params->image_info.image_max_size =
						BL2_BASE - STM32MP_SYSRAM_BASE;
					INFO("BL31 max size = 0x%x (%uB)\n",
					     bl_mem_params->image_info.image_max_size,
					     bl_mem_params->image_info.image_max_size);
				}
				break;
			case BL32_IMAGE_ID:
				bl_mem_params->ep_info.pc = config_info->config_addr;

				/* In case of OPTEE, initialize address space with tos_fw addr */
				pager_mem_params = get_bl_mem_params_node(BL32_EXTRA1_IMAGE_ID);
				if (pager_mem_params != NULL) {
					pager_mem_params->image_info.image_base =
						config_info->config_addr;
					pager_mem_params->image_info.image_max_size =
						config_info->config_max_size;
				}
				break;

			case BL33_IMAGE_ID:
				bl_mem_params->ep_info.pc = config_info->config_addr;
				break;

			case HW_CONFIG_ID:
			case SOC_FW_CONFIG_ID:
				break;

			default:
				return -EINVAL;
			}
		}

#ifndef DECRYPTION_SUPPORT_none
		/* Load encryption key info before DT is unmapped */
		err = stm32_get_enc_key_otp_idx_len(&otp_idx, &otp_len);
		if (err != 0) {
			panic();
		}
#endif
		mmap_remove_dynamic_region(DTB_BASE, DTB_LIMIT - DTB_BASE);
		inv_dcache_range(DTB_BASE, DTB_LIMIT - DTB_BASE);

		break;

	case BL32_IMAGE_ID:
		if (optee_header_is_valid(bl_mem_params->image_info.image_base)) {
			/* BL32 is OP-TEE header */
			bl_mem_params->ep_info.pc = bl_mem_params->image_info.image_base;
			pager_mem_params = get_bl_mem_params_node(BL32_EXTRA1_IMAGE_ID);
			assert(pager_mem_params != NULL);

			err = parse_optee_header(&bl_mem_params->ep_info,
						 &pager_mem_params->image_info,
						 NULL);
			if (err != 0) {
				ERROR("OPTEE header parse error.\n");
				panic();
			}

			/* Set optee boot info from parsed header data */
			bl_mem_params->ep_info.args.arg0 = 0U; /* Unused */
			bl_mem_params->ep_info.args.arg1 = 0U; /* Unused */
			bl_mem_params->ep_info.args.arg2 = 0U; /* No DT supported */
		}
		break;

#if STM32MP_M33_TDCID
	case BL32_EXTRA1_IMAGE_ID:
		dsbsy();
		isb();
		stm32mp_unmap_ddr();
		err = mmap_add_dynamic_region(STM32MP_BL33_BASE, STM32MP_BL33_BASE,
					      STM32MP_DDR_MAX_SIZE - (STM32MP_BL33_BASE -
								      STM32MP_DDR_BASE),
					      MT_MEMORY | MT_RW | MT_NS);
		if (err != 0) {
			panic();
		}
		break;
#endif

	case BL33_IMAGE_ID:
#if PSA_FWU_SUPPORT
		stm32_fwu_set_boot_idx();
#endif /* PSA_FWU_SUPPORT */
		break;

	default:
		/* Do nothing in default case */
		break;
	}

	return err;
}

void bl2_el3_plat_prepare_exit(void)
{
	flush_dcache_range(BSS_START, BSS_END - BSS_START);
	flush_dcache_range(DATA_START, DATA_END - DATA_START);

	stm32mp_io_exit();

	/* Unmask potential tamper before exit */
	stm32mp_syscfg_mask_potential_tamper_disable();
}
