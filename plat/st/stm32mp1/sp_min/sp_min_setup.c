/*
 * Copyright (c) 2015-2020, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <string.h>

#include <platform_def.h>

#include <arch_helpers.h>
#include <common/bl_common.h>
#include <common/debug.h>
#include <context.h>
#include <drivers/arm/gicv2.h>
#include <drivers/arm/tzc400.h>
#include <drivers/generic_delay_timer.h>
#include <drivers/st/bsec.h>
#include <drivers/st/etzpc.h>
#include <drivers/st/stm32_console.h>
#include <drivers/st/stm32_gpio.h>
#include <drivers/st/stm32_iwdg.h>
#include <drivers/st/stm32_rng.h>
#include <drivers/st/stm32_rtc.h>
#include <drivers/st/stm32_tamp.h>
#include <drivers/st/stm32_timer.h>
#include <drivers/st/stm32mp_clkfunc.h>
#include <drivers/st/stm32mp_pmic.h>
#include <drivers/st/stm32mp1_clk.h>
#include <drivers/st/stm32mp1_ddr_helpers.h>
#include <drivers/st/stpmic1.h>
#include <dt-bindings/clock/stm32mp1-clks.h>
#include <lib/el3_runtime/context_mgmt.h>
#include <lib/mmio.h>
#include <lib/xlat_tables/xlat_tables_v2.h>
#include <plat/common/platform.h>

#include <platform_sp_min.h>
#include <stm32mp1_power_config.h>

/******************************************************************************
 * Placeholder variables for copying the arguments that have been passed to
 * BL32 from BL2.
 ******************************************************************************/
static entry_point_info_t bl33_image_ep_info;

static struct console_stm32 console;
static void stm32mp1_tamper_action(int id);

static const char *tamper_name[PLAT_MAX_TAMP_INT] = {
	"RTC power domain",
	"Temperature monitoring",
	"LSE monitoring",
	"HSE monitoring",
	"RTC calendar overflow",
	"Monotonic counter"
};

static struct stm32_tamp_int int_tamp[PLAT_MAX_TAMP_INT] = {
	{
		.id = ITAMP1,
		.func = stm32mp1_tamper_action,
	},
	{
		.id = ITAMP2,
		.func = stm32mp1_tamper_action,
	},
	{
		.id = ITAMP3,
		.func = stm32mp1_tamper_action,
	},
	{
		.id = ITAMP4,
		.func = stm32mp1_tamper_action,
	},
	TAMP_UNUSED,
	TAMP_UNUSED,
};

static struct stm32_tamp_ext ext_tamp[PLAT_MAX_TAMP_EXT] = {
	TAMP_UNUSED,
	TAMP_UNUSED,
	TAMP_UNUSED,
};

static void tzc_it_handler(void)
{
	ERROR("No IT handler in ARM tzc400 driver\n");
}

static void stm32_sgi1_it_handler(void)
{
	uint32_t id;

	stm32mp_mask_timer();

	gicv2_end_of_interrupt(ARM_IRQ_SEC_SGI_1);

	do {
		id = plat_ic_get_pending_interrupt_id();

		if (id <= MAX_SPI_ID) {
			gicv2_end_of_interrupt(id);

			plat_ic_disable_interrupt(id);
		}
	} while (id <= MAX_SPI_ID);

	stm32mp_wait_cpu_reset();
}

static void stm32mp1_tamper_action(int id)
{
	ERROR("Tamper %s occurs\n", tamper_name[id]);
	stm32mp_plat_reset(plat_my_core_pos());
}

static void configure_wakeup_interrupt(void)
{
	int irq_num = fdt_rcc_enable_it("wakeup");

	if (irq_num < 0) {
		ERROR("irq_num = %d\n", irq_num);
		panic();
	}

	plat_ic_set_interrupt_priority(irq_num, STM32MP1_IRQ_RCC_SEC_PRIO);
}

static void initialize_pll1_settings(void)
{
	uint32_t cpu_voltage = 0U;

	if (stm32_are_pll1_settings_valid_in_context()) {
		set_clk_pll1_settings_to_valid();
		return;
	}

	if (dt_pmic_status() > 0) {
		const char *name = stm32mp_get_cpu_supply_name();
		int ret;

		if (name == NULL) {
			panic();
		}

		ret = stpmic1_regulator_voltage_get(name);
		if (ret < 0) {
			panic();
		}

		cpu_voltage = (uint32_t)ret;
	}

	if (stm32mp1_clk_compute_all_pll1_settings(cpu_voltage) != 0) {
		panic();
	}
}

/*******************************************************************************
 * Interrupt handler for FIQ (secure IRQ)
 ******************************************************************************/
void sp_min_plat_fiq_handler(uint32_t id)
{
	uint32_t value = 0;

	switch (id & INT_ID_MASK) {
	case ARM_IRQ_SEC_PHY_TIMER:
	case STM32MP1_IRQ_MCU_SEV:
	case STM32MP1_IRQ_RCC_WAKEUP:
		stm32mp1_calib_it_handler(id);
		break;
	case STM32MP1_IRQ_TZC400:
		tzc400_init(STM32MP1_TZC_BASE);
		tzc400_it_handler();
		panic();
		break;
	case STM32MP1_IRQ_TAMPSERRS:
		stm32_tamp_it_handler();
		break;
	case ARM_IRQ_SEC_SGI_1:
		stm32_sgi1_it_handler();
		break;
	case ARM_IRQ_SEC_SGI_6:
		/* tell the primary cpu to exit from stm32_pwr_down_wfi() */
		if (plat_my_core_pos() == STM32MP_PRIMARY_CPU) {
			stm32mp1_calib_set_wakeup(true);
		}
		gicv2_end_of_interrupt(ARM_IRQ_SEC_SGI_6);
		break;
	case STM32MP1_IRQ_IWDG1:
	case STM32MP1_IRQ_IWDG2:
		stm32_iwdg_it_handler(id);
		break;
	case STM32MP1_IRQ_AXIERRIRQ:
		ERROR("STM32MP1_IRQ_AXIERRIRQ generated\n");
		tzc400_init(STM32MP1_TZC_BASE);
		__asm__("mrc	p15, 1, %0, c9, c0, 3" : "=r" (value));
		if (value) {
			/* we have a pending IT clear it */
			value = 0;
			__asm__("mcr	p15, 1, %0, c9, c0, 3" :: "r" (value));
		} else {
			ERROR("IRQ_AXIERRIRQ handle call w/o any flag set!!\n");
		}

		/* Check if FIQ has been generated due to TZC400 abort*/
		if (tzc400_is_pending_interrupt()) {
			tzc_it_handler();
		} else {
			ERROR("IRQ_AXIERRIRQ cause can't be detected");
		}

		panic();
		break;
	default:
		ERROR("SECURE IT handler not define for it : %u\n", id);
		break;
	}
}

/*******************************************************************************
 * Return a pointer to the 'entry_point_info' structure of the next image for
 * the security state specified. BL33 corresponds to the non-secure image type
 * while BL32 corresponds to the secure image type. A NULL pointer is returned
 * if the image does not exist.
 ******************************************************************************/
entry_point_info_t *sp_min_plat_get_bl33_ep_info(void)
{
	entry_point_info_t *next_image_info;
	uint32_t bkpr_core1_addr =
		tamp_bkpr(BOOT_API_CORE1_BRANCH_ADDRESS_TAMP_BCK_REG_IDX);
	uint32_t bkpr_core1_magic =
		tamp_bkpr(BOOT_API_CORE1_MAGIC_NUMBER_TAMP_BCK_REG_IDX);

	next_image_info = &bl33_image_ep_info;

	/*
	 * PC is set to 0 when resetting after STANDBY
	 * The context should be restored, and the image information
	 * should be filled with what what was saved
	 */
	if (next_image_info->pc == 0U) {
		void *cpu_context;
		uint32_t magic_nb, saved_pc;

		stm32mp_clk_enable(RTCAPB);

		magic_nb = mmio_read_32(bkpr_core1_magic);
		saved_pc = mmio_read_32(bkpr_core1_addr);

		stm32mp_clk_disable(RTCAPB);

		if (stm32_restore_context() != 0) {
			panic();
		}

		cpu_context = cm_get_context(NON_SECURE);

		next_image_info->spsr = read_ctx_reg(get_regs_ctx(cpu_context),
						     CTX_SPSR);

		/* PC should be retrieved in backup register if OK, else it can
		 * be retrieved from non-secure context
		 */
		if (magic_nb == BOOT_API_A7_CORE0_MAGIC_NUMBER) {
			/* BL33 return address should be in DDR */
			if ((saved_pc < STM32MP_DDR_BASE) ||
			    (saved_pc > (STM32MP_DDR_BASE +
					 (dt_get_ddr_size() - 1U)))) {
				panic();
			}

			next_image_info->pc = saved_pc;
		} else {
			next_image_info->pc =
				read_ctx_reg(get_regs_ctx(cpu_context), CTX_LR);
		}
	}

	return next_image_info;
}

CASSERT((STM32MP_SEC_SYSRAM_BASE >= STM32MP_SYSRAM_BASE) &&
	((STM32MP_SEC_SYSRAM_BASE + STM32MP_SEC_SYSRAM_SIZE) <=
	 (STM32MP_SYSRAM_BASE + STM32MP_SYSRAM_SIZE)),
	assert_secure_sysram_fits_into_sysram);

#ifdef STM32MP_NS_SYSRAM_BASE
CASSERT((STM32MP_NS_SYSRAM_BASE >= STM32MP_SEC_SYSRAM_BASE) &&
	((STM32MP_NS_SYSRAM_BASE + STM32MP_NS_SYSRAM_SIZE) ==
	 (STM32MP_SYSRAM_BASE + STM32MP_SYSRAM_SIZE)),
	assert_non_secure_sysram_fits_at_end_of_sysram);

CASSERT((STM32MP_NS_SYSRAM_BASE & GENMASK(11, 0)) == 0,
	assert_non_secure_sysram_base_is_4kbyte_aligned);

/* Last 4kByte page (12 bit wide) of SYSRAM is non-secure */
#define TZMA1_SECURE_RANGE \
	(((STM32MP_NS_SYSRAM_BASE - STM32MP_SYSRAM_BASE) >> 12) - 1U)
#else
/* STM32MP_NS_SYSRAM_BASE not defined means all SYSRAM is secure */
#define TZMA1_SECURE_RANGE		STM32MP1_ETZPC_TZMA_ALL_SECURE
#endif /* STM32MP_NS_SYSRAM_BASE */

#define TZMA0_SECURE_RANGE		STM32MP1_ETZPC_TZMA_ALL_SECURE

static void stm32mp1_etzpc_early_setup(void)
{
	etzpc_init();
	etzpc_configure_tzma(0U, TZMA0_SECURE_RANGE);
	etzpc_configure_tzma(1U, TZMA1_SECURE_RANGE);
}

/*******************************************************************************
 * Perform any BL32 specific platform actions.
 ******************************************************************************/
void sp_min_early_platform_setup2(u_register_t arg0, u_register_t arg1,
				  u_register_t arg2, u_register_t arg3)
{
#if STM32MP_UART_PROGRAMMER
	uint32_t boot_itf, boot_instance;
#endif
	struct dt_node_info dt_uart_info;
	int result;
	bl_params_t *params_from_bl2 = (bl_params_t *)arg0;

	/* Imprecise aborts can be masked in NonSecure */
	write_scr(read_scr() | SCR_AW_BIT);

	mmap_add_region(BL_CODE_BASE, BL_CODE_BASE,
			BL_CODE_END - BL_CODE_BASE,
			MT_CODE | MT_SECURE);

	configure_mmu();

	assert(params_from_bl2 != NULL);
	assert(params_from_bl2->h.type == PARAM_BL_PARAMS);
	assert(params_from_bl2->h.version >= VERSION_2);

	bl_params_node_t *bl_params = params_from_bl2->head;

	/*
	 * Copy BL33 entry point information.
	 * They are stored in Secure RAM, in BL2's address space.
	 */
	while (bl_params != NULL) {
		if (bl_params->image_id == BL33_IMAGE_ID) {
			bl33_image_ep_info = *bl_params->ep_info;
			break;
		}

		bl_params = bl_params->next_params_info;
	}

	if (dt_open_and_check() < 0) {
		panic();
	}

	if (bsec_probe() != 0) {
		panic();
	}

	if (stm32mp1_clk_probe() < 0) {
		panic();
	}

	if (etzpc_init() != 0) {
		panic();
	}

	stm32mp1_etzpc_early_setup();

	result = dt_get_stdout_uart_info(&dt_uart_info);
#if STM32MP_UART_PROGRAMMER
	stm32_get_boot_interface(&boot_itf, &boot_instance);

	if ((result > 0) && (dt_uart_info.status != 0U) &&
	    !((boot_itf == BOOT_API_CTX_BOOT_INTERFACE_SEL_SERIAL_UART) &&
	      (get_uart_address(boot_instance) == dt_uart_info.base))) {
#else
	if ((result > 0) && (dt_uart_info.status != 0U)) {
#endif
		unsigned int console_flags;

		if (console_stm32_register(dt_uart_info.base, 0,
					   STM32MP_UART_BAUDRATE, &console) ==
		    0) {
			panic();
		}

		console_flags = CONSOLE_FLAG_BOOT | CONSOLE_FLAG_CRASH |
			CONSOLE_FLAG_TRANSLATE_CRLF;
#ifdef DEBUG
		console_flags |= CONSOLE_FLAG_RUNTIME;
#endif
		console_set_scope(&console.console, console_flags);
	}

	if (dt_pmic_status() > 0) {
		initialize_pmic();
	}

	initialize_pll1_settings();

	stm32mp1_init_lp_states();
}

/*******************************************************************************
 * Set security setup in sp_min
 ******************************************************************************/
static void stm32mp1_sp_min_security_setup(void)
{
	uint32_t filter_conf = 0;
	uint32_t active_conf = 0;
	int ret;

	/* Init rtc driver */
	ret = stm32_rtc_init();
	if (ret < 0) {
		WARN("RTC driver init error %i\n", ret);
	}

	/*  Init rng driver */
	ret = stm32_rng_init();
	if (ret < 0) {
		WARN("RNG driver init error %i\n", ret);
	}

	/* Init tamper */
	if (stm32_tamp_init() > 0) {
		stm32_tamp_configure_internal(int_tamp, PLAT_MAX_TAMP_INT);
		stm32_tamp_configure_external(ext_tamp, PLAT_MAX_TAMP_EXT,
					      filter_conf, active_conf);

		/* Enable timestamp for tamper */
		stm32_rtc_set_tamper_timestamp();
	}

	if (stm32_timer_init() == 0) {
		stm32mp1_calib_init();
	}
}

/*******************************************************************************
 * Initialize the MMU, security and the GIC.
 ******************************************************************************/
void sp_min_platform_setup(void)
{
	ddr_save_sr_mode();

	/* Initialize tzc400 after DDR initialization */
	stm32mp1_security_setup();

	generic_delay_timer_init();

	stm32_gic_init();

	/* Update security settings */
	stm32mp1_sp_min_security_setup();

	if (stm32_iwdg_init() < 0) {
		panic();
	}

	configure_wakeup_interrupt();

	stm32mp_lock_periph_registering();

	stm32mp1_init_scmi_server();
}

void sp_min_plat_arch_setup(void)
{
}
