// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023 KBEmbedded

#include <furi.h>
#include <storage/storage.h>
#include <lib/toolbox/stream/stream.h>
#include <lib/flipper_format/flipper_format.h>

#include <stdint.h>

#include <gblink/include/gblink.h>
#include "gblink_i.h"

#define PINCONF_FILE_TYPE	"Flipper GB Link Pinconf"
#define PINCONF_FILE_VER	1

#define PINCONF_ORIG		"Original"
#define PINCONF_MLVK		"MLVK2.5"
#define PINCONF_CUST		"Custom"

#define PINCONF_SI		"SI"
#define PINCONF_SO		"SO"
#define PINCONF_CLK		"CLK"

struct gblink_pins {
        const GpioPin *serin;
        const GpioPin *serout;
        const GpioPin *clk;
        const GpioPin *sd;
};

const struct gblink_pins common_pinouts[PINOUT_COUNT] = {
	/* Original */
	{
		&gpio_ext_pc3,
		&gpio_ext_pb3,
		&gpio_ext_pb2,
		&gpio_ext_pa4,
	},
	/* MALVEKE EXT1 */
	{
		&gpio_ext_pa6,
		&gpio_ext_pa7,
		&gpio_ext_pb3,
		&gpio_ext_pa4,
	},
};


int gblink_pin_set_by_gpiopin(void *handle, gblink_bus_pins pin, const GpioPin *gpio)
{
	furi_assert(handle);
	struct gblink *gblink = handle;

	if (furi_mutex_acquire(gblink->start_mutex, 0) != FuriStatusOk)
		return -1;

	switch (pin) {
	case PIN_SERIN:
		gblink->serin = gpio;
		break;
	case PIN_SEROUT:
		gblink->serout = gpio;
		break;
	case PIN_CLK:
		gblink->clk = gpio;
		break;
	case PIN_SD:
		gblink->sd = gpio;
		break;
	default:
		furi_crash();
		break;
	}

	furi_mutex_release(gblink->start_mutex);

	return 0;
}

const GpioPin *gblink_pin_get_by_gpiopin(void *handle, gblink_bus_pins pin)
{
	furi_assert(handle);
	struct gblink *gblink = handle;

	switch (pin) {
	case PIN_SERIN:
		return gblink->serin;
	case PIN_SEROUT:
		return gblink->serout;
	case PIN_CLK:
		return gblink->clk;
	case PIN_SD:
		return gblink->sd;
	default:
		furi_crash();
		break;
	}

	return NULL;
}

int gblink_pin_set_default(void *handle, gblink_pinouts pinout)
{
	furi_assert(handle);
	struct gblink *gblink = handle;

	if (pinout == PINOUT_CUSTOM || pinout >= PINOUT_COUNT)
		return -1;

	if (furi_mutex_acquire(gblink->start_mutex, 0) != FuriStatusOk)
		return -1;

	gblink->serin = common_pinouts[pinout].serin;
	gblink->serout = common_pinouts[pinout].serout;
	gblink->clk = common_pinouts[pinout].clk;
	gblink->sd = common_pinouts[pinout].sd;

	furi_mutex_release(gblink->start_mutex);

	return 0;
}

int gblink_pin_get_default(void *handle)
{
	furi_assert(handle);
	struct gblink *gblink = handle;
	int i;

	for (i = 0; i < PINOUT_COUNT; i++) {
		if (gblink->serin != common_pinouts[i].serin)
			continue;
		if (gblink->serout != common_pinouts[i].serout)
			continue;
		if (gblink->clk != common_pinouts[i].clk)
			continue;
		/* XXX: Currently not checked or used! */
		//if (gblink->sd != common_pinouts[pinout].sd;
		break;
	}

	if (i == PINOUT_COUNT)
		i = -1;

	return i;
}

int gblink_pin_set(void *handle, gblink_bus_pins pin, unsigned int pinnum)
{
	furi_assert(handle);
	furi_assert(pinnum < gpio_pins_count);

	struct gblink *gblink = handle;

	if (furi_mutex_acquire(gblink->start_mutex, 0) != FuriStatusOk)
		return -1;

	switch (pin) {
	case PIN_SERIN:
		gblink->serin = gpio_pins[pinnum].pin;
		break;
	case PIN_SEROUT:
		gblink->serout = gpio_pins[pinnum].pin;
		break;
	case PIN_CLK:
		gblink->clk = gpio_pins[pinnum].pin;
		break;
	case PIN_SD:
		gblink->sd = gpio_pins[pinnum].pin;
		break;
	default:
		furi_crash();
		break;
	}

	furi_mutex_release(gblink->start_mutex);

	return 0;
}

int gblink_pin_get(void *handle, gblink_bus_pins pin)
{
	furi_assert(handle);
	struct gblink *gblink = handle;
	unsigned int i;

	for (i = 0; i < gpio_pins_count; i++) {
		switch (pin) {
		case PIN_SERIN:
			if (gpio_pins[i].pin == gblink->serin)
				return i;
			break;
		case PIN_SEROUT:
			if (gpio_pins[i].pin == gblink->serout)
				return i;
			break;
		case PIN_CLK:
			if (gpio_pins[i].pin == gblink->clk)
				return i;
			break;
		case PIN_SD:
			if (gpio_pins[i].pin == gblink->sd)
				return i;
			break;
		default:
			furi_crash();
			break;
		}
	}

	return -1;
}

int gblink_pin_count_max(void)
{
	unsigned int i;
	int count = 0;

	for (i = 0; i < gpio_pins_count; i++)
		if (!gpio_pins[i].debug)
			count = i;

	return count;
}

int gblink_pin_get_next(unsigned int pinnum)
{
	unsigned int i;

	/* The check could be eliminated and just rely on the return -1 at the
	 * end of the function, but this is a shortcut so we don't have to walk
	 * through the whole table just to find out its an out-of-bounds pin.
	 */
	if (pinnum >= gpio_pins_count)
		return -1;

	for (i = pinnum; i < gpio_pins_count; i++)
		if (!gpio_pins[i].debug)
			return i;

	return -1;
}

int gblink_pin_get_prev(unsigned int pinnum)
{
	int i;

	/* The check could be eliminated and just rely on the return -1 at the
	 * end of the function, but this is a shortcut so we don't have to walk
	 * through the whole table just to find out its an out-of-bounds pin.
	 */
	if (pinnum >= gpio_pins_count)
		return -1;

	for (i = pinnum; i >= 0; i--)
		if (!gpio_pins[i].debug)
			return i;

	return -1;
}

bool gblink_pinconf_load(void *gblink)
{

	Storage *storage = NULL;;
	FlipperFormat *data_file = NULL;
	FuriString *string = NULL;
	uint32_t val;
	bool ret = false;

	storage = furi_record_open(RECORD_STORAGE);

	string = furi_string_alloc_set(APP_DATA_PATH(""));
	storage_common_resolve_path_and_ensure_app_directory(storage, string);
	furi_string_cat_str(string, ".gblink_pinconf");

	data_file = flipper_format_file_alloc(storage);

	if (!flipper_format_file_open_existing(data_file, furi_string_get_cstr(string))) {
		FURI_LOG_E("pinconf", "Error opening file %s", furi_string_get_cstr(string));
		goto out;
	}

	if (!flipper_format_read_header(data_file, string, &val)) {
		FURI_LOG_E("pinconf", "Missing or incorrect header");
		goto out;
	}

	if (strncmp(furi_string_get_cstr(string), PINCONF_FILE_TYPE, strlen(PINCONF_FILE_TYPE)) ||
	    val != PINCONF_FILE_VER) {
		FURI_LOG_E("pinconf", "Type or version mismatch");
		goto out;
	}

	if (!flipper_format_read_string(data_file, "Mode", string)) {
		FURI_LOG_E("pinconf", "Missing Mode");
		goto out;
	}

	if (!strncmp(furi_string_get_cstr(string), PINCONF_ORIG, strlen(PINCONF_ORIG))) {
		FURI_LOG_I("pinconf", "Setting Original pinout");
		gblink_pin_set_default(gblink, PINOUT_ORIGINAL);
		goto out;
	}

	if (!strncmp(furi_string_get_cstr(string), PINCONF_MLVK, strlen(PINCONF_MLVK))) {
		FURI_LOG_I("pinconf", "Setting MALVEKE 2.5 pinout");
		gblink_pin_set_default(gblink, PINOUT_MALVEKE_EXT1);
		goto out;
	}

	if (!strncmp(furi_string_get_cstr(string), PINCONF_CUST, strlen(PINCONF_CUST))) {
		FURI_LOG_I("pinconf", "Setting Custom pinout");
	}

	if (!flipper_format_read_uint32(data_file, PINCONF_SI, &val, 1)) {
		FURI_LOG_E("pinconf", "Missing SI");
		goto out;
	} else {
		gblink_pin_set(gblink, PIN_SERIN, val);
	}

	if (!flipper_format_read_uint32(data_file, PINCONF_SO, &val, 1)) {
		FURI_LOG_E("pinconf", "Missing SO");
		goto out;
	} else {
		gblink_pin_set(gblink, PIN_SEROUT, val);
	}

	if (!flipper_format_read_uint32(data_file, PINCONF_CLK, &val, 1)) {
		FURI_LOG_E("pinconf", "Missing CLK");
		goto out;
	} else {
		gblink_pin_set(gblink, PIN_CLK, val);
	}

	ret = true;

out:
	flipper_format_file_close(data_file);
	furi_string_free(string);
	furi_record_close(RECORD_STORAGE);
	return ret;
}

/* XXX: TODO: I think there is a way to build this ahead of time and
 * write it in one go to be a bit more time efficient.
 */
bool gblink_pinconf_save(void *gblink)
{

	Storage *storage = NULL;;
	FlipperFormat *data_file = NULL;
	FuriString *string = NULL;
	int rc;
	uint32_t pin;
	bool ret = false;

	storage = furi_record_open(RECORD_STORAGE);

	string = furi_string_alloc_set(APP_DATA_PATH(""));
	storage_common_resolve_path_and_ensure_app_directory(storage, string);
	furi_string_cat_str(string, ".gblink_pinconf");

	data_file = flipper_format_file_alloc(storage);

	if (!flipper_format_file_open_always(data_file, furi_string_get_cstr(string))) {
		FURI_LOG_E("pinconf", "Error opening file %s", furi_string_get_cstr(string));
		goto out;
	}

	if (!flipper_format_write_header_cstr(data_file, PINCONF_FILE_TYPE, PINCONF_FILE_VER)) {
		FURI_LOG_E("pinconf", "Error writing header to file");
		goto out;
	}

	rc = gblink_pin_get_default(gblink);
	switch (rc) {
	case 0:
		if (!flipper_format_write_string_cstr(data_file, "Mode", PINCONF_ORIG))
			FURI_LOG_E("pinconf", "Error writing mode to file");
		goto out;
		break;
	case 1:
		if (!flipper_format_write_string_cstr(data_file, "Mode", PINCONF_MLVK))
			FURI_LOG_E("pinconf", "Error writing mode to file");
		goto out;
		break;
	case -1:
		if (!flipper_format_write_string_cstr(data_file, "Mode", PINCONF_CUST))
			FURI_LOG_E("pinconf", "Error writing mode to file");
		break;
	default:
		FURI_LOG_E("pinconf", "Unknown mode");
		goto out;
		break;
	}

	pin = gblink_pin_get(gblink, PIN_SERIN);
	if (!flipper_format_write_uint32(data_file, "SI", &pin, 1)) {
		FURI_LOG_E("pinconf", "Error writing SI to file");
		goto out;
	}

	pin = gblink_pin_get(gblink, PIN_SEROUT);
	if (!flipper_format_write_uint32(data_file, "SO", &pin, 1)) {
		FURI_LOG_E("pinconf", "Error writing SO to file");
		goto out;
	}

	pin = gblink_pin_get(gblink, PIN_CLK);
	if (!flipper_format_write_uint32(data_file, "CLK", &pin, 1)) {
		FURI_LOG_E("pinconf", "Error writing CLK to file");
	}

	ret = true;

out:
	flipper_format_file_close(data_file);
	furi_string_free(string);
	furi_record_close(RECORD_STORAGE);
	return ret;
}

