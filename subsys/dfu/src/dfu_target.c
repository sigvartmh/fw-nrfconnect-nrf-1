/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */
#include <zephyr.h>
#include <logging/log.h>
#include <dfu/mcuboot.h>
#include <dfu/dfu_target.h>
#include "dfu_target_mcuboot.h"
#include "dfu_target_modem.h"

extern struct dfu_target dfu_target_modem;
extern struct dfu_target dfu_target_mcuboot;

#define MIN_SIZE_IDENTIFY_BUF 32

#define MCUBOOT_IMAGE 1
#define MODEM_DELTA_IMAGE 2

LOG_MODULE_REGISTER(dfu_target, CONFIG_DFU_TARGET_LOG_LEVEL);

static struct dfu_target *ctx;

int dfu_target_img_type(const void *const buf, size_t len)
{
	if (IS_ENABLED(CONFIG_DFU_TARGET_MCUBOOT) &&
	    dfu_target_mcuboot_identify(buf)) {
		return MCUBOOT_IMAGE;
	}

	if (IS_ENABLED(CONFIG_DFU_TARGET_MODEM) &&
	    dfu_target_modem_identify(buf)) {
		return MODEM_DELTA_IMAGE;
	}

	if (len < MIN_SIZE_IDENTIFY_BUF) {
		return -EAGAIN;
	}

	LOG_ERR("No supported image type found");
	return -ENOTSUP;
}

int dfu_target_init(int img_type)
{
	if (ctx != NULL) {
		return -EBUSY;
	}

	if (IS_ENABLED(CONFIG_DFU_TARGET_MCUBOOT) &&
			img_type == MCUBOOT_IMAGE) {
		ctx = &dfu_target_mcuboot;
	} else if (IS_ENABLED(CONFIG_DFU_TARGET_MODEM) &&
			img_type == MODEM_DELTA_IMAGE) {
		ctx = &dfu_target_modem;
	}

	if (ctx == NULL) {
		LOG_ERR("Unknown image type");
		return -ENOTSUP;
	}

	return ctx->init();
}

int dfu_target_offset_get(size_t *offset)
{
	if (ctx == NULL) {
		return -EACCES;
	}

	return ctx->offset_get(offset);
}

int dfu_target_write(const void *const buf, size_t len)
{
	if (ctx == NULL || buf == NULL) {
		return -EACCES;
	}

	return ctx->write(buf, len);
}

int dfu_target_done(void)
{
	int err;

	if (ctx == NULL) {
		return -EACCES;
	}

	err = ctx->done();
	ctx = NULL;
	if (err != 0) {
		LOG_ERR("Unable to clean up dfu_target");
		return err;
	}

	return 0;
}

