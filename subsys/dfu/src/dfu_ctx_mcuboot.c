/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */
#include <zephyr.h>
#include <flash.h>
#include <logging/log.h>
#include <dfu/mcuboot.h>
#include <dfu/flash_img.h>

LOG_MODULE_REGISTER(dfu_ctx_mcuboot, CONFIG_DFU_CTX_LOG_LEVEL);
static struct flash_img_context flash_img;

int dfu_ctx_mcuboot_init(void)
{

	int err = flash_img_init(&flash_img);

	if (err != 0) {
		LOG_ERR("flash_img_init error %d", err);
		return err;
	}

	return 0;
}

int dfu_ctx_mcuboot_done(void)
{
	int err = flash_img_buffered_write(&flash_img, NULL,
				       0, true);
	if (err != 0) {
		LOG_ERR("flash_img_buffered_write error %d", err);
		return err;
	}

	err = boot_request_upgrade(BOOT_UPGRADE_TEST);
	if (err != 0) {
		LOG_ERR("boot_request_upgrade error %d", err);
		return err;
	}

	LOG_INF("McuBoot image upgrade scheduled. Reset the device to apply");
	return 0;
}

int dfu_ctx_mcuboot_write(const void *const buf, size_t len)
{
	return flash_img_buffered_write(&flash_img, (u8_t *)buf, len, false);
}
