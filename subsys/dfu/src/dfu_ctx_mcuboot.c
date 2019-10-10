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

#define MCUBOOT_HEADER_MAGIC 0x96f3b83d

static struct flash_img_context flash_img;
static size_t offset;

bool dfu_ctx_mcuboot_identify(const void *const buf)
{
	/* MCUBoot headers starts with 4 byte magic word */
	return *((u32_t *)buf) == MCUBOOT_HEADER_MAGIC;
}

int dfu_ctx_mcuboot_init(void)
{
	int err = flash_img_init(&flash_img);

	if (err != 0) {
		LOG_ERR("flash_img_init error %d", err);
		return err;
	}

	offset = 0;

	return 0;
}

int dfu_ctx_mcuboot_offset(void)
{
	return offset;
}

int dfu_ctx_mcuboot_write(const void *const buf, size_t len)
{
	int err = flash_img_buffered_write(&flash_img, (u8_t *)buf, len, false);

	if (err != 0) {
		LOG_ERR("flash_img_buffered_write error %d", err);
		return err;
	}

	offset += len;

	return 0;
}

int dfu_ctx_mcuboot_done(void)
{
	int err = flash_img_buffered_write(&flash_img, NULL, 0, true);
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

	return 0;
}

