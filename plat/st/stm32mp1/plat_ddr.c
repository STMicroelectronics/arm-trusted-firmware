/*
 * Copyright (C) 2024-2026, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <errno.h>

#include <drivers/st/regulator.h>
#include <drivers/st/stm32mp_ddr.h>
#include <drivers/st/stm32mp_pmic.h>
#include <libfdt.h>

/* configure the STPMIC1 regulators on STMicroelectronics boards */
static int pmic_ddr_power_init(enum ddr_type ddr_type)
{
	int status;
	void *fdt;
	int node;

	if (fdt_get_address(&fdt) == 0) {
		return -FDT_ERR_NOTFOUND;
	}

	node = fdt_node_offset_by_compatible(fdt, -1, DT_DDR_COMPAT);
	if (node < 0) {
		ERROR("%s: Cannot read DDR node in DT\n", __func__);
		return -EINVAL;
	}

	switch (ddr_type) {
	case STM32MP_DDR3: {
		struct rdev *vref;
		struct rdev *vdd;
		struct rdev *vtt;

		vdd = regulator_get_by_supply_name(fdt, node, "vdd");
		if (vdd == NULL) {
			return -ENOENT;
		}

		status = regulator_set_min_voltage(vdd);
		if (status != 0) {
			return status;
		}

		vref = regulator_get_by_supply_name(fdt, node, "vref");
		if (vref != NULL) {
			status = regulator_enable(vref);
			if (status != 0) {
				return status;
			}
		}

		vtt = regulator_get_by_supply_name(fdt, node, "vtt");
		if (vtt != NULL) {
			status = regulator_enable(vtt);
			if (status != 0) {
				return status;
			}
		}

		status = regulator_enable(vdd);
		if (status != 0) {
			return status;
		}
		break;
	}

	case STM32MP_LPDDR2:
	case STM32MP_LPDDR3: {
		struct rdev *vddq;
		struct rdev *vdd1_ddr;
		struct rdev *vdd2_ddr;

		vdd1_ddr = regulator_get_by_supply_name(fdt, node, "vdd1");
		if (vdd1_ddr == NULL) {
			return -ENOENT;
		}

		vdd2_ddr = regulator_get_by_supply_name(fdt, node, "vdd2");
		if (vdd2_ddr == NULL) {
			return -ENOENT;
		}

		vddq = regulator_get_by_supply_name(fdt, node, "vddq");
		if (vddq == NULL) {
			return -ENOENT;
		}

		status = regulator_set_min_voltage(vdd1_ddr);
		if (status != 0) {
			return status;
		}

		status = regulator_set_min_voltage(vdd2_ddr);
		if (status != 0) {
			return status;
		}

		status = regulator_set_min_voltage(vddq);
		if (status != 0) {
			return status;
		}

		status = regulator_enable(vdd1_ddr);
		if (status != 0) {
			return status;
		}

		status = regulator_enable(vdd2_ddr);
		if (status != 0) {
			return status;
		}

		status = regulator_enable(vddq);
		if (status != 0) {
			return status;
		}
		break;
	}

	default:
		break;
	};

	return 0;
}

int stm32mp_board_ddr_power_init(enum ddr_type ddr_type)
{
	if (dt_pmic_status() > 0) {
		return pmic_ddr_power_init(ddr_type);
	}

	return 0;
}
