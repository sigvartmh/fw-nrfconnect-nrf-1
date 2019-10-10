/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */
#include <zephyr.h>
#include <flash.h>
#include <logging/log.h>
#include <dfu/mcuboot.h>
#include <dfu/dfu_target.h>
#include <dfu/flash_img.h>
#include "dfu_target_mcuboot.h"

LOG_MODULE_REGISTER(dfu_target_mcuboot, CONFIG_DFU_TARGET_LOG_LEVEL);

#define MCUBOOT_HEADER_MAGIC 0x96f3b83d

/* Expose API compatible with dfu_target. This is used by dfu_target.c. */
struct dfu_target dfu_target_mcuboot = {
	.init  = dfu_target_mcuboot_init,
	.offset_get = dfu_target_mcuboot_offset_get,
	.write = dfu_target_mcuboot_write,
	.done  = dfu_target_mcuboot_done,
};

static struct flash_img_context flash_img;
static size_t offset;

bool dfu_target_mcuboot_identify(const void *const buf)
{
	/* MCUBoot headers starts with 4 byte magic word */
	return *((u32_t *)buf) == MCUBOOT_HEADER_MAGIC;
}

int dfu_target_mcuboot_init(void)
{
	int err = flash_img_init(&flash_img);

	if (err != 0) {
		LOG_ERR("flash_img_init error %d", err);
		return err;
	}

	offset = 0;

	return 0;
}

int dfu_target_mcuboot_offset_get(size_t *out)
{
	*out = offset;
	return 0;
}

int dfu_target_mcuboot_write(const void *const buf, size_t len)
{
	int err = flash_img_buffered_write(&flash_img, (u8_t *)buf, len, false);

	if (err != 0) {
		LOG_ERR("flash_img_buffered_write error %d", err);
		return err;
	}

	offset += len;

	return 0;
}

int dfu_target_mcuboot_done(void)
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
