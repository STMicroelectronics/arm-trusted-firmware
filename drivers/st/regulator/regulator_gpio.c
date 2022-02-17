/*
 * Copyright (c) 2022, STMicroelectronics
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <errno.h>

#include <common/debug.h>
#include <common/fdt_wrappers.h>
#include <drivers/st/regulator.h>
#include <drivers/st/regulator_gpio.h>
#include <drivers/st/stm32_gpio.h>
#include <libfdt.h>

#ifndef PLAT_NB_GPIO_REGUS
#error "Missing PLAT_NB_GPIO_REGUS"
#endif

#define MAX_NAME_LEN 32

#define REGU_DISABLED 0
#define REGU_ENABLED 1

#define GPIO_DESC_SIZE 3U

struct gpio_config {
	uint32_t bank;
	uint32_t pin;
	uint32_t config;
	uint8_t status;
};

struct gpio_regu_data {
	char name[MAX_NAME_LEN];
	struct regul_description desc;
	struct gpio_config en_gpio;
	bool active_high;
};

static struct gpio_regu_data data[PLAT_NB_GPIO_REGUS];

static int gpio_set_state(const struct regul_description *desc, bool state)
{
	struct gpio_regu_data *d = (struct gpio_regu_data *)desc->driver_data;
	enum gpio_level level = GPIO_LEVEL_LOW;

	VERBOSE("regul %s set state=%d\n", d->name, state);

	if ((d->active_high && state) || (!d->active_high && !state)) {
		level = GPIO_LEVEL_HIGH;
	}

	set_gpio_level(d->en_gpio.bank, d->en_gpio.pin, level);

	return 0;
}

static int gpio_get_state(const struct regul_description *desc)
{
	struct gpio_regu_data *d = (struct gpio_regu_data *)desc->driver_data;
	enum gpio_level level = get_gpio_level(d->en_gpio.bank, d->en_gpio.pin);

	if ((d->active_high && level == GPIO_LEVEL_HIGH) ||
	    (!d->active_high && level == GPIO_LEVEL_LOW)) {
		return REGU_ENABLED;
	}

	return REGU_DISABLED;
}

static struct regul_ops gpio_regu_ops = {
	.set_state = gpio_set_state,
	.get_state = gpio_get_state,
};

static int read_gpio_config_from_dt(const void *fdt, int node, char *name,
				    struct gpio_config *gpio)
{
	uint32_t utable[GPIO_DESC_SIZE];
	int ret;

	gpio->status = fdt_get_status(node);
	if (gpio->status == DT_DISABLED) {
		return -FDT_ERR_NOTFOUND;
	}

	ret = fdt_read_uint32_array(fdt, node, name, GPIO_DESC_SIZE, utable);
	if (ret != 0) {
		ERROR("Missing gpio description\n");
		return ret;
	}

	gpio->bank = utable[0];
	gpio->pin = utable[1];
	gpio->config = utable[2];

	return 0;
}

int gpio_regulator_register(void)
{
	uint32_t count = 0U;
	void *fdt;
	int node = 0;

	VERBOSE("gpio_regu reg init\n");

	if (fdt_get_address(&fdt) == 0) {
		return -FDT_ERR_NOTFOUND;
	}

	do {
		int len __unused;
		int ret;
		struct gpio_regu_data *d = &data[count];
		const char *reg_name;

		node = fdt_node_offset_by_compatible(fdt, node, "st,stm32-regulator-gpio");
		if (node < 0) {
			break;
		}

		reg_name = fdt_get_name(fdt, node, NULL);

		VERBOSE("register gpio_regu reg %s\n", reg_name);

		if (count == PLAT_NB_GPIO_REGUS) {
			WARN("Not enough place for regulators, PLAT_NB_GPIO_REGUS should be increased.\n");
			return -ENOMEM;
		}

		len = snprintf(d->name, MAX_NAME_LEN - 1, "%s", reg_name);
		assert((len > 0) && (len < (MAX_NAME_LEN - 1)));

		d->desc.node_name = d->name;
		d->desc.driver_data = d;
		d->desc.ops = &gpio_regu_ops;

		ret = read_gpio_config_from_dt(fdt, node, "st,enable-gpios", &d->en_gpio);
		if (ret != 0) {
			ERROR("Incorrect gpio regul description\n");
			return ret;
		}

		VERBOSE("%s: en_gpio bank=%u pin=%u cfg=%x\n", d->name,
			d->en_gpio.bank, d->en_gpio.pin, d->en_gpio.config);

		set_gpio_config(d->en_gpio.bank, d->en_gpio.pin, d->en_gpio.config,
				d->en_gpio.status);

		if (fdt_getprop(fdt, node, "enable-active-high", NULL))
			d->active_high = true;

		ret = regulator_register(&d->desc, node);
		if (ret != 0) {
			WARN("Failed to register %s\n", reg_name);
			return ret;
		}

		count++;
	} while (node > 0);

	return 0;
}
