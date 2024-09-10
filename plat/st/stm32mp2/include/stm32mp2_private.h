/*
 * Copyright (c) 2024-2026, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef STM32MP2_PRIVATE_H
#define STM32MP2_PRIVATE_H

void configure_mmu(void);

uint32_t stm32mp_syscfg_get_chip_dev_id(void);
void stm32mp_syscfg_mask_potential_tamper_enable(void);
void stm32mp_syscfg_mask_potential_tamper_disable(void);
size_t stm32mp_syscfg_get_mm_size(uint8_t bank);
int stm32mp_syscfg_dlyb_init(uint8_t bank, bool bypass_mode,
			     uint16_t period_ps);
int stm32mp_syscfg_dlyb_find_tap(uint8_t bank, int (*check_transfer)(void),
				 bool rx_only, uint8_t *window_len);
int stm32mp_syscfg_dlyb_set_cr(uint8_t bank, uint32_t dlyb_cr);
void stm32mp_syscfg_dlyb_get_cr(uint8_t bank, uint32_t *dlyb_cr);
void stm32mp_syscfg_dlyb_stop(uint8_t bank);

/* IO compensation identifiers */
enum syscfg_io_ids {
	SYSFG_VDDIO1_ID,
	SYSFG_VDDIO2_ID,
	SYSFG_VDDIO3_ID,
#if !STM32MP21
	SYSFG_VDDIO4_ID,
#endif /* !STM32MP21 */
	SYSFG_VDD_IO_ID,
	SYSFG_NB_IO_ID
};

void stm32mp_syscfg_enable_io_compensation(enum syscfg_io_ids id);

void stm32mp_syscfg_set_icn_qos(void);

int stm32mp2_pwr_init_io_domains(void);

/* Get RISAF platform instance ID from peripheral IO memory base address */
int stm32_risaf_get_instance(uintptr_t base);

/* Get RISAF peripheral IO memory base address from platform instance ID */
uintptr_t stm32_risaf_get_base(int instance);

/* Get RISAF maximum number of regions from platform instance ID */
int stm32_risaf_get_max_region(int instance);

/* Get RISAF memory base address from platform instance ID */
uintptr_t stm32_risaf_get_memory_base(int instance);

/* Get RISAF memory size in bytes from platform instance ID */
size_t stm32_risaf_get_memory_size(int instance);

/* Get DDRDBG peripheral IO memory base address */
uintptr_t stm32_ddrdbg_get_base(void);

/* Wrappers for OTP / BSEC functions */
#if !STM32MP_M33_TDCID
static inline uint32_t stm32_otp_probe(void)
{
	return bsec_probe();
}

static inline uint32_t stm32_otp_read(uint32_t *val, uint32_t otp)
{
	return bsec_read_otp(val, otp);
}

static inline uint32_t stm32_otp_shadow_read(uint32_t *val, uint32_t otp)
{
	return bsec_shadow_read_otp(val, otp);
}

static inline uint32_t stm32_otp_write(uint32_t val, uint32_t otp)
{
	return bsec_write_otp(val, otp);
}

static inline uint32_t stm32_otp_set_sr_lock(uint32_t otp)
{
	return bsec_set_sr_lock(otp);
}

static inline uint32_t stm32_otp_read_sw_lock(uint32_t otp, bool *value)
{
	return bsec_read_sw_lock(otp, value);
}

static inline bool stm32_is_bsec_closed(void)
{
	return (bsec_get_state() & BSEC_STATE_MASK) == BSEC_STATE_CLOSED;
}

static inline bool stm32_otp_is_hwkey_valid(void)
{
	if ((bsec_get_state() & BSEC_HARDWARE_KEY) != 0U) {
		return true;
	}

	return false;
}
#else /* STM32MP_M33_TDCID */
uint32_t stm32_otp_probe(void);
uint32_t stm32_otp_read(uint32_t *val, uint32_t otp);
uint32_t stm32_otp_shadow_read(uint32_t *val, uint32_t otp);
uint32_t stm32_otp_write(uint32_t val, uint32_t otp);
uint32_t stm32_otp_set_sr_lock(uint32_t otp);
uint32_t stm32_otp_read_sw_lock(uint32_t otp, bool *value);
bool stm32_is_bsec_closed(void);
#endif /* STM32MP_M33_TDCID */

uint32_t otp_mirror_read(uint32_t *val, uint32_t otp);

void stm32mp_ca35_set_vbar(uintptr_t vbar);

#endif /* STM32MP2_PRIVATE_H */
