/*
 * Copyright (c) 2022-2024, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <common/debug.h>
#include <common/fdt_wrappers.h>
#include <drivers/st/regulator.h>
#include <lib/mmio.h>
#include <lib/utils_def.h>
#include <libfdt.h>

#include <platform_def.h>
#include <stm32mp2_private.h>

#define IO_VOLTAGE_THRESHOLD_MV 2700
#define IOCOMP_CODE_MAX 2

struct pwr_regu {
	const char *node_name;
	uint32_t enable_reg;
	uint32_t valid_mask;
	uint32_t vrsel_mask;
	enum syscfg_io_ids comp_idx;
	const char *supply_name;
};

enum pwr_regulator_id {
	IOD_VDDIO1,
	IOD_VDDIO2,
	IOD_VDDIO3,
#if !STM32MP21
	IOD_VDDIO4,
#endif /* !STM32MP21 */
	IOD_VDDIO,
	PWR_REGU_COUNT
};

static const struct pwr_regu pwr_regulators[] = {
	 [IOD_VDDIO1] = {
		.node_name = "vddio1",
		.enable_reg = PWR_CR8,
		.valid_mask = PWR_CR8_VDDIO1SV,
		.vrsel_mask = PWR_CR8_VDDIO1VRSEL,
		.comp_idx = SYSFG_VDDIO1_ID,
		.supply_name = "vddio1",
	 },
	 [IOD_VDDIO2] = {
		.node_name = "vddio2",
		.enable_reg = PWR_CR7,
		.valid_mask = PWR_CR7_VDDIO2SV,
		.vrsel_mask = PWR_CR7_VDDIO2VRSEL,
		.comp_idx = SYSFG_VDDIO2_ID,
		.supply_name = "vddio2",
	 },
	 [IOD_VDDIO3] = {
		.node_name = "vddio3",
		.enable_reg = PWR_CR1,
		.valid_mask = PWR_CR1_VDDIO3SV,
		.vrsel_mask = PWR_CR1_VDDIO3VRSEL,
		.comp_idx = SYSFG_VDDIO3_ID,
		.supply_name = "vddio3",
	 },
#if !STM32MP21
	 [IOD_VDDIO4] = {
		.node_name = "vddio4",
		.enable_reg = PWR_CR1,
		.valid_mask = PWR_CR1_VDDIO4SV,
		.vrsel_mask = PWR_CR1_VDDIO4VRSEL,
		.comp_idx = SYSFG_VDDIO4_ID,
		.supply_name = "vddio4",
	 },
#endif /* !STM32MP21 */
	 [IOD_VDDIO] = {
		.node_name = "vddio",
		.enable_reg = PWR_CR1,
		.vrsel_mask = PWR_CR1_VDDIOVRSEL,
		.comp_idx = SYSFG_VDD_IO_ID,
		.supply_name = "vdd",
	 },
};

static bool stm32mp2_pwr_domain_is_enabled(const struct pwr_regu *regu)
{
	uint32_t val = mmio_read_32(stm32mp_pwr_base() + regu->enable_reg);
	bool status = (val & regu->valid_mask) != 0U;

	VERBOSE("pwr: IO domain status for %s is %d\n", regu->node_name, status);

	return status;

}

static int stm32mp2_pwr_handle_vrsel(void *fdt, int node, const struct pwr_regu *regu)
{
	struct rdev *supply;
	int mv;

	VERBOSE("Check VRSEL for %s\n", regu->node_name);

	/* Look for supply */
	supply = regulator_get_by_supply_name(fdt, node, regu->supply_name);
	if (supply == NULL) {
		VERBOSE("Failed to get %s-supply\n", regu->supply_name);
		return 0;
	}

	mv = regulator_get_voltage(supply);
	if (mv < 0) {
		return mv;
	}

	if (mv < IO_VOLTAGE_THRESHOLD_MV) {
		VERBOSE("Enable VRSEL for %s\n", regu->node_name);

		mmio_setbits_32(stm32mp_pwr_base() + regu->enable_reg, regu->vrsel_mask);

		if ((mmio_read_32(stm32mp_pwr_base() + regu->enable_reg) &
		    regu->vrsel_mask) == 0U) {
			WARN("Could not enable VRSEL for %s\n", regu->node_name);
		}
	}

	return 0;
}

/*
 * @brief  Enable IO compensation for the IO domains that are already ON,
 *         and set domain to high speed depending on power supply voltage.
 * @retval 0 or error.
 */
int stm32mp2_pwr_init_io_domains(void)
{
	void *fdt;
	int node;
	int subnode = 0;

	VERBOSE("Init IO domains\n");

	if (fdt_get_address(&fdt) == 0) {
		return -FDT_ERR_NOTFOUND;
	}

	node = fdt_node_offset_by_compatible(fdt, -1, DT_PWR_COMPAT);
	if (node < 0) {
		ERROR("Pwr node not found\n");
		return -FDT_ERR_NOTFOUND;
	}

	fdt_for_each_subnode(subnode, fdt, node) {
		const struct pwr_regu *regu = NULL;
		int i;

		/* Find regu from node name */
		const char *node_name = fdt_get_name(fdt, subnode, NULL);

		for (i = 0; i < PWR_REGU_COUNT; i++) {
			regu = &pwr_regulators[i];

			if (strcmp(regu->node_name, node_name) == 0) {
				break;
			}
		}

		if (i == PWR_REGU_COUNT) {
			continue;
		}

		if (stm32mp2_pwr_domain_is_enabled(regu)) {
			uint32_t iocomp_code[IOCOMP_CODE_MAX] = {0U};
			int ret;

			VERBOSE("Init IO domain %d\n", i);

			ret = stm32mp2_pwr_handle_vrsel(fdt, subnode, regu);
			if (ret < 0) {
				return ret;
			}

			ret = fdt_read_uint32_array(fdt, subnode, "st,iocomp",
						    IOCOMP_CODE_MAX,
						    iocomp_code);
			if ((ret != 0) && (ret != -FDT_ERR_NOTFOUND)) {
				return ret;
			}

			if (ret == -FDT_ERR_NOTFOUND) {
				stm32mp_syscfg_enable_io_comp(regu->comp_idx);
			} else {
				stm32mp_syscfg_fixed_io_comp(regu->comp_idx,
							     iocomp_code[0],
							     iocomp_code[1]);
			}
		}
	}

	return 0;
}
