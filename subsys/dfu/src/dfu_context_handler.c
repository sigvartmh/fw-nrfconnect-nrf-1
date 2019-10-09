/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <logging/log.h>
#include <dfu/mcuboot.h>
#include <dfu/dfu_context_handler.h>
#include <dfu_ctx_mcuboot.h>
#include <dfu_ctx_modem.h>

#define MIN_SIZE_IDENTIFY_BUF 32

#define MCUBOOT_IMAGE 1
#define MODEM_DELTA_IMAGE 2

LOG_MODULE_REGISTER(dfu_context_handler, CONFIG_DFU_CTX_LOG_LEVEL);

static struct dfu_ctx *ctx;
static bool   initialized = false;

int dfu_ctx_img_type(const void *const buf, size_t len)
{
	if (len < MIN_SIZE_IDENTIFY_BUF) {
		return -EAGAIN;
	}

	if (dfu_ctx_mcuboot_identify(buf)) {
		return MCUBOOT_IMAGE;
	}

	if (IS_ENABLED(CONFIG_DFU_CTX_MODEM_UPDATE_SUPPORT) &&
	    dfu_ctx_modem_identify(buf)){
		return MODEM_DELTA_IMAGE;
	}

	LOG_ERR("No supported image type found");
	return -ENOTSUP;
}

int dfu_ctx_init(int img_type)
{
	if (!initialized) {
		if (IS_ENABLED(CONFIG_BOOTLOADER_MCUBOOT) &&
		    img_type == MCUBOOT_IMAGE) {
			ctx = &dfu_ctx_mcuboot;
		} else if (IS_ENABLED(CONFIG_DFU_CTX_MODEM_UPDATE_SUPPORT) &&
		           img_type == MODEM_DELTA_IMAGE) {
			ctx = &dfu_ctx_modem;
		}

		if (ctx == NULL) {
			LOG_ERR("Unknown image type");
			return -ENOTSUP;
		}

		initialized = true;

		return ctx->init();
	}

	return 0;
}

int dfu_ctx_offset(void)
{
	__ASSERT_NO_MSG(initialized);

	if (ctx == NULL) {
		return -ESRCH;
	}

	return ctx->offset();
}


int dfu_ctx_write(const void *const buf, size_t len)
{
	__ASSERT_NO_MSG(initialized);

	if (ctx == NULL) {
		return -ESRCH;
	}

	return ctx->write(buf, len);
}


int dfu_ctx_done(void)
{
	__ASSERT_NO_MSG(initialized);
	int err;

	if (ctx == NULL) {
		return -ESRCH;
	}

	err = ctx->done();
	if (err < 0) {
		LOG_ERR("Unable to clean up dfu_ctx");
		return err;
	}

	ctx = NULL;

	initialized = false;

	return 0;
}

