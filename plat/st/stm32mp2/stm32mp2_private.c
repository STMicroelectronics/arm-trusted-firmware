/*
 * Copyright (c) 2023, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <errno.h>

#include <arch_helpers.h>
#include <drivers/arm/gicv2.h>
#include <drivers/st/stm32_iwdg.h>
#include <lib/mmio.h>
#include <lib/xlat_tables/xlat_tables_v2.h>
#include <plat/common/platform.h>

#include <platform_def.h>
#include <stm32mp2_context.h>

#if STM32MP_DDR_FIP_IO_STORAGE && defined(IMAGE_BL2)
/* Map the whole SRAM1 as secure, required to load DDR FW from FIP */
#define SRAM1_MAP_SIZE	SRAM1_SIZE_FOR_TFA
#else
/* Map the beginning of SRAM1 as secure */
#define SRAM1_MAP_SIZE	STM32MP_SEC_SRAM1_SIZE
#endif

#define MAP_SYSRAM	MAP_REGION_FLAT(STM32MP_SYSRAM_BASE, \
					STM32MP_SYSRAM_SIZE, \
					MT_MEMORY | \
					MT_RW | \
					MT_SECURE | \
					MT_EXECUTE_NEVER)

#define MAP_SEC_SYSRAM	MAP_REGION_FLAT(STM32MP_SEC_SYSRAM_BASE, \
					STM32MP_SEC_SYSRAM_SIZE, \
					MT_MEMORY | \
					MT_RW | \
					MT_SECURE | \
					MT_EXECUTE_NEVER)

#if STM32MP_USB_PROGRAMMER
#define MAP_SEC_DEVICE_SYSRAM	MAP_REGION_FLAT(STM32MP_SEC_DEVICE_SYSRAM_BASE, \
					STM32MP_SEC_DEVICE_SYSRAM_SIZE, \
					MT_DEVICE | \
					MT_RW | \
					MT_SECURE | \
					MT_EXECUTE_NEVER)
#endif

/* Non-secure SYSRAM is used a uncached memory for SCMI message transfer */
#define MAP_NS_SYSRAM	MAP_REGION_FLAT(STM32MP_NS_SYSRAM_BASE, \
					STM32MP_NS_SYSRAM_SIZE, \
					MT_DEVICE | \
					MT_RW | \
					MT_NS | \
					MT_EXECUTE_NEVER)

#define MAP_SRAM1	MAP_REGION_FLAT(SRAM1_BASE, \
					SRAM1_MAP_SIZE, \
					MT_MEMORY | \
					MT_RW | \
					MT_SECURE | \
					MT_EXECUTE_NEVER)

#define MAP_DEVICE	MAP_REGION_FLAT(STM32MP_DEVICE_BASE, \
					STM32MP_DEVICE_SIZE, \
					MT_DEVICE | \
					MT_RW | \
					MT_SECURE | \
					MT_EXECUTE_NEVER)

#if defined(IMAGE_BL2)
static const mmap_region_t stm32mp2_mmap[] = {
#if STM32MP_USB_PROGRAMMER
	MAP_SEC_SYSRAM,
	MAP_SEC_DEVICE_SYSRAM,
#else
	MAP_SYSRAM,
#endif
	MAP_SRAM1,
	MAP_DEVICE,
	{0}
};
#endif
#if defined(IMAGE_BL31)
static const mmap_region_t stm32mp2_mmap[] = {
	MAP_SEC_SYSRAM,
	MAP_NS_SYSRAM,
	MAP_SRAM1,
	MAP_DEVICE,
	{0}
};
#endif

static uint8_t saved_header[STM32MP_HEADER_SIZE] __maybe_unused;

void configure_mmu(void)
{
	mmap_add(stm32mp2_mmap);
	init_xlat_tables();

	enable_mmu_el3(0);
}

int stm32mp_map_retram(void)
{
	return  mmap_add_dynamic_region(RETRAM_BASE, RETRAM_BASE,
					RETRAM_SIZE,
					MT_RW | MT_SECURE);
}

int stm32mp_unmap_retram(void)
{
	return  mmap_remove_dynamic_region(RETRAM_BASE,
					   RETRAM_SIZE);
}

uintptr_t stm32_get_gpio_bank_base(unsigned int bank)
{
	if (bank == GPIO_BANK_Z) {
		return GPIOZ_BASE;
	}

	assert(bank <= GPIO_BANK_K);

	return GPIOA_BASE + (bank * GPIO_BANK_OFFSET);
}

uint32_t stm32_get_gpio_bank_offset(unsigned int bank)
{
	if (bank == GPIO_BANK_Z) {
		return 0;
	}

	assert(bank <= GPIO_BANK_K);

	return bank * GPIO_BANK_OFFSET;
}

unsigned long stm32_get_gpio_bank_clock(unsigned int bank)
{
	if (bank == GPIO_BANK_Z) {
		return CK_BUS_GPIOZ;
	}

	assert(bank <= GPIO_BANK_K);

	return CK_BUS_GPIOA + (bank - GPIO_BANK_A);
}

#if STM32MP_UART_PROGRAMMER || !defined(IMAGE_BL2)
/*
 * UART Management
 */
static const uintptr_t stm32mp2_uart_addresses[STM32MP_NB_OF_UART] = {
	USART1_BASE,
	USART2_BASE,
	USART3_BASE,
	UART4_BASE,
	UART5_BASE,
	USART6_BASE,
	UART7_BASE,
	UART8_BASE,
	UART9_BASE,
};

uintptr_t get_uart_address(uint32_t instance_nb)
{
	if ((instance_nb == 0U) ||
	    (instance_nb > STM32MP_NB_OF_UART)) {
		return 0U;
	}

	return stm32mp2_uart_addresses[instance_nb - 1U];
}
#endif

void stm32_save_header(void)
{
#if (STM32MP_UART_PROGRAMMER || STM32MP_USB_PROGRAMMER) && TRUSTED_BOARD_BOOT
	memcpy(&saved_header, (uint8_t *)STM32MP_HEADER_BASE, STM32MP_HEADER_SIZE);
#endif /* STM32MP_USB_PROGRAMMER && TRUSTED_BOARD_BOOT */
}

uintptr_t stm32_get_header_address(void)
{
#if (STM32MP_UART_PROGRAMMER || STM32MP_USB_PROGRAMMER) && TRUSTED_BOARD_BOOT
	return (uintptr_t)&saved_header;
#else
	return STM32MP_HEADER_BASE;
#endif /* STM32MP_USB_PROGRAMMER && TRUSTED_BOARD_BOOT */
}

uint32_t stm32mp_get_chip_version(void)
{
	static uint32_t rev;

	if (rev != 0U) {
		return rev;
	}

	if (stm32_get_otp_value(REVISION_OTP, &rev) != 0) {
		panic();
	}

	return rev;
}

uint32_t stm32mp_get_chip_dev_id(void)
{
	return stm32mp_syscfg_get_chip_dev_id();
}

static uint32_t get_part_number(void)
{
	static uint32_t part_number;

	if (part_number != 0U) {
		return part_number;
	}

	if (stm32_get_otp_value(PART_NUMBER_OTP, &part_number) != 0) {
		panic();
	}

	return part_number;
}

static uint32_t get_cpu_package(void)
{
	static uint32_t package = UINT32_MAX;

	if (package == UINT32_MAX) {
		if (stm32_get_otp_value(PACKAGE_OTP, &package) != 0) {
			panic();
		}
	}

	return (package & PACKAGE_OTP_PKG_MASK) >> PACKAGE_OTP_PKG_SHIFT;
}

void stm32mp_get_soc_name(char name[STM32_SOC_NAME_SIZE])
{
	char *cpu_s, *cpu_r, *pkg;

	/* MPUs Part Numbers */
	switch (get_part_number()) {
	case STM32MP251A_PART_NB:
		cpu_s = "251A";
		break;
	case STM32MP251C_PART_NB:
		cpu_s = "251C";
		break;
	case STM32MP251D_PART_NB:
		cpu_s = "251D";
		break;
	case STM32MP251F_PART_NB:
		cpu_s = "251F";
		break;
	case STM32MP253A_PART_NB:
		cpu_s = "253A";
		break;
	case STM32MP253C_PART_NB:
		cpu_s = "253C";
		break;
	case STM32MP253D_PART_NB:
		cpu_s = "253D";
		break;
	case STM32MP253F_PART_NB:
		cpu_s = "253F";
		break;
	case STM32MP255A_PART_NB:
		cpu_s = "255A";
		break;
	case STM32MP255C_PART_NB:
		cpu_s = "255C";
		break;
	case STM32MP255D_PART_NB:
		cpu_s = "255D";
		break;
	case STM32MP255F_PART_NB:
		cpu_s = "255F";
		break;
	case STM32MP257A_PART_NB:
		cpu_s = "257A";
		break;
	case STM32MP257C_PART_NB:
		cpu_s = "257C";
		break;
	case STM32MP257D_PART_NB:
		cpu_s = "257D";
		break;
	case STM32MP257F_PART_NB:
		cpu_s = "257F";
		break;
	default:
		cpu_s = "????";
		break;
	}

	/* Package */
	switch (get_cpu_package()) {
	case STM32MP25_PKG_CUSTOM:
		pkg = "XX";
		break;
	case STM32MP25_PKG_AL_VFBGA361:
		pkg = "AL";
		break;
	case STM32MP25_PKG_AK_VFBGA424:
		pkg = "AK";
		break;
	case STM32MP25_PKG_AI_TFBGA436:
		pkg = "AI";
		break;
	default:
		pkg = "??";
		break;
	}

	/* REVISION */
	switch (stm32mp_get_chip_version()) {
#if STM32MP25
	case 0:
#endif
	case STM32MP2_REV_B:
		cpu_r = "B";
		break;
	case STM32MP2_REV_X:
		cpu_r = "X";
		break;
	case STM32MP2_REV_Y:
		cpu_r = "Y";
		break;
	case STM32MP2_REV_Z:
		cpu_r = "Z";
		break;
	default:
		cpu_r = "?";
		break;
	}

	snprintf(name, STM32_SOC_NAME_SIZE,
		 "STM32MP%s%s Rev.%s", cpu_s, pkg, cpu_r);
}

void stm32mp_print_cpuinfo(void)
{
	char name[STM32_SOC_NAME_SIZE];

	stm32mp_get_soc_name(name);
	NOTICE("CPU: %s\n", name);
}

void stm32mp_print_boardinfo(void)
{
	uint32_t board_id = 0U;

	if (stm32_get_otp_value(BOARD_ID_OTP, &board_id) != 0) {
		return;
	}

	if (board_id != 0U) {
		stm32_display_board_info(board_id);
	}
}

/* Return true when SoC provides a single Cortex-A35 core, and false otherwise */
bool stm32mp_is_single_core(void)
{
	bool single_core = false;

	switch (get_part_number()) {
	case STM32MP251A_PART_NB:
	case STM32MP251C_PART_NB:
	case STM32MP251D_PART_NB:
	case STM32MP251F_PART_NB:
		single_core = true;
		break;
	default:
		break;
	}

	return single_core;
}

/* Return true when device is in closed state */
uint32_t stm32mp_check_closed_device(void)
{
	static uint32_t otp_idx;
	static uint32_t otp_len;
	uint32_t otp_val;
	uint32_t status = STM32MP_CHIP_SEC_CLOSED;

	if (otp_len == 0U) {
		if (stm32_get_otp_index(LIFECYCLE2_OTP, &otp_idx, &otp_len) != 0) {
			panic();
		}

		if (otp_len != __WORD_BIT) {
			panic();
		}
	}

	if (stm32_get_otp_value_from_idx(otp_idx, &otp_val) != 0) {
		panic();
	}

	if (!stm32_otp_is_closed_device() ||
	    ((otp_val & SECURE_BOOT_CLOSED_SECURE) == 0U)) {
		status = STM32MP_CHIP_SEC_OPEN;
	}

	return status;
}

/* Return true when device supports secure boot */
bool stm32mp_is_auth_supported(void)
{
	bool supported = false;

	switch (get_part_number()) {
	case STM32MP251C_PART_NB:
	case STM32MP251F_PART_NB:
	case STM32MP253C_PART_NB:
	case STM32MP253F_PART_NB:
	case STM32MP255C_PART_NB:
	case STM32MP255F_PART_NB:
	case STM32MP257C_PART_NB:
	case STM32MP257F_PART_NB:
		supported = true;
		break;
	default:
		break;
	}

	return supported;
}

uint32_t stm32_iwdg_get_instance(uintptr_t base)
{
	uint32_t instance = UINT32_MAX;

	switch (base) {
	case IWDG1_BASE:
		instance = IWDG1_INST;
		break;
	case IWDG2_BASE:
		instance = IWDG2_INST;
		break;
	default:
		break;
	}

	if (instance == UINT32_MAX) {
		panic();
	}

	return instance;
}

uint32_t stm32_iwdg_get_otp_config(uint32_t iwdg_inst)
{
	uint32_t iwdg_cfg = 0U;
	uint32_t otp_value;

	if (stm32_get_otp_value(HCONF1_OTP, &otp_value) != 0U) {
		panic();
	}

	if ((otp_value & HCONF1_OTP_IWDG_HW_MASK(iwdg_inst)) != 0U) {
		iwdg_cfg |= IWDG_HW_ENABLED;
	}

	return iwdg_cfg;
}

bool stm32mp_skip_boot_device_after_standby(void)
{
	return false;
}

bool stm32mp_is_wakeup_from_standby(void)
{
#if STM32MP_UART_PROGRAMMER || STM32MP_USB_PROGRAMMER
	return false;
#else /* STM32MP_UART_PROGRAMMER || STM32MP_USB_PROGRAMMER */
	static int standby = -1;
	uint32_t rstsr;

	if (standby == -1) {
		rstsr = mmio_read_32(stm32mp_rcc_base() + RCC_C1BOOTRSTSCLRR);

		if ((rstsr & RCC_C1BOOTRSTSCLRR_PADRSTF) != 0U) {
			return false;
		}
		if (stm32_pm_context_is_valid()) {
			standby = 1;
		} else {
			standby = 0;
		}
	}

	return standby == 1;
#endif /* STM32MP_UART_PROGRAMMER || STM32MP_USB_PROGRAMMER */
}

int stm32_risaf_get_instance(uintptr_t base)
{
	switch (base) {
	case RISAF2_BASE:
		return (int)RISAF2_INST;
	case RISAF4_BASE:
		return (int)RISAF4_INST;
	default:
		return -ENODEV;
	}
}

uintptr_t stm32_risaf_get_base(int instance)
{
	switch (instance) {
	case RISAF2_INST:
		return (uintptr_t)RISAF2_BASE;
	case RISAF4_INST:
		return (uintptr_t)RISAF4_BASE;
	default:
		return 0U;
	}
}

int stm32_risaf_get_max_region(int instance)
{
	switch (instance) {
	case RISAF2_INST:
		return (int)RISAF2_MAX_REGION;
	case RISAF4_INST:
		return (int)RISAF4_MAX_REGION;
	default:
		return 0;
	}
}

uintptr_t stm32_risaf_get_memory_base(int instance)
{
	switch (instance) {
	case RISAF2_INST:
		return (uintptr_t)STM32MP_OSPI_MM_BASE;
	case RISAF4_INST:
		return (uintptr_t)STM32MP_DDR_BASE;
	default:
		return 0U;
	}
}

size_t stm32_risaf_get_memory_size(int instance)
{
	switch (instance) {
	case RISAF2_INST:
		return STM32MP_OSPI_MM_SIZE;
	case RISAF4_INST:
		return dt_get_ddr_size();
	default:
		return 0U;
	}
}

uintptr_t stm32_ddrdbg_get_base(void)
{
	return DDRDBG_BASE;
}
