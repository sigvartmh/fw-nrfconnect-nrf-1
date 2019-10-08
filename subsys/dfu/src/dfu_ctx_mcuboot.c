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
static size_t offset;
static bool initialized = false;

int dfu_ctx_mcuboot_init(void)
{
	if (!initialized) {
		int err = flash_img_init(&flash_img);

		if (err != 0) {
			LOG_ERR("flash_img_init error %d", err);
			return err;
		}

		offset = 0;
		initialized = true;
	}

	return 0;
}

int dfu_ctx_mcuboot_done(void)
{
	__ASSERT_NO_MSG(initialized);

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

	LOG_INF("MCUBoot image upgrade scheduled. Reset the device to apply");

	/* Reset state to ensure another dfu process can be completed */
	initialized = false;

	return 0;
}

int dfu_ctx_mcuboot_offset(void)
{
	__ASSERT_NO_MSG(initialized);

	return offset;
}

int dfu_ctx_mcuboot_write(const void *const buf, size_t len)
{
	__ASSERT_NO_MSG(initialized);

	int err = flash_img_buffered_write(&flash_img, (u8_t *)buf, len, false);

	if (err != 0) {
		LOG_ERR("flash_img_buffered_write error %d", err);
		return err;
	}

	offset += len;

	return 0;
}
