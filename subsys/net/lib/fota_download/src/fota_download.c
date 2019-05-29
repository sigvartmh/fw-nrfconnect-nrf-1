/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdint.h>
#include <zephyr.h>
#include <flash.h>
#include <download_client.h>
#include <dfu/mcuboot.h>
#include <pm_config.h>
#include <logging/log.h>
#include <fota_download.h>

LOG_MODULE_REGISTER(fota_download, CONFIG_FOTA_DOWNLOAD_LOG_LEVEL);

static struct	device *flash_dev;
static bool	is_flash_page_erased[FLASH_PAGE_MAX_CNT];
static		fota_download_callback_t callback;
static struct	download_client dfu;

static void report_flash_err(int err)
{
	struct fota_download_evt evt = {.id = FOTA_DOWNLOAD_FLASH_ERR,
					.flash_error_code = err};
	callback(&evt);
}

static void report_dlc_evt(const struct download_client_evt *dlc_evt)
{
	struct fota_download_evt evt = {.id = FOTA_DOWNLOAD_EVT_DOWNLOAD_CLIENT,
					.dlc_evt = dlc_evt};
	callback(&evt);
}

static int flash_page_erase_if_needed(u32_t address)
{
	int err;
	struct flash_pages_info info;

	err = flash_get_page_info_by_offs(flash_dev, address, &info);
	if (err != 0) {
		LOG_ERR("flash_get_page_info_by_offs returned error %d\n", err);
		return err;
	}
	if (!is_flash_page_erased[info.index]) {
		err = flash_write_protection_set(flash_dev, false);
		if (err != 0) {
			LOG_ERR("flash_write_protection_set error %d\n", err);
			return err;
		}
		err = flash_erase(flash_dev, info.start_offset, info.size);
		if (err != 0) {
			LOG_ERR("flash_erase error %d at address %08x\n",
				err, info.start_offset);
			return err;
		}
		is_flash_page_erased[info.index] = true;
		err = flash_write_protection_set(flash_dev, true);
		if (err != 0) {
			LOG_ERR("flash_write_protection_set error %d\n", err);
			return err;
		}
	}
	return 0;
}

static int download_client_callback(const struct download_client_evt *event)
{
	int err;
	static u32_t flash_address = PM_MCUBOOT_SECONDARY_ADDRESS;

	__ASSERT(event != NULL, "invalid dfu object\n");

	switch (event->id) {
	case DOWNLOAD_CLIENT_EVT_FRAGMENT: {
		size_t size;

		err = download_client_file_size_get(&dfu, &size);
		if (err != 0) {
			LOG_ERR("download_client_file_size_get returned error"
				"%d\n", err);
			return err;
		}
		if (size > PM_MCUBOOT_SECONDARY_SIZE) {
			LOG_ERR("Requested file too big to fit in flash\n");
			report_flash_err(-EFBIG);
			return -EFBIG;
		}

		err = flash_page_erase_if_needed(flash_address);
		if (err != 0) {
			report_flash_err(err);
			return err;
		}

		err = flash_write_protection_set(flash_dev, false);
		if (err != 0) {
			LOG_ERR("flash_write_protection_set error %d\n", err);
			report_flash_err(err);
			return err;
		}

		err = flash_write(flash_dev, flash_address,
				event->fragment.buf, event->fragment.len);
		if (err != 0) {
			flash_write_protection_set(flash_dev, true);
			LOG_ERR("Flash write error %d at address %08x\n",
				err, flash_address);
			report_flash_err(err);
			return err;
		}

		err = flash_write_protection_set(flash_dev, true);
		if (err != 0) {
			LOG_ERR("flash_write_protection_set error %d\n", err);
			report_flash_err(err);
			return err;
		}

		report_dlc_evt(event);
		flash_address += event->fragment.len;
		break;
	}

	case DOWNLOAD_CLIENT_EVT_DONE:
		/* The download is complete, and we need to tag the secondary
		 * slot as a valid upgrade candidate. To do this, we need to
		 * ensure that the last flash page of the secondary slot is
		 * writeable (i.e. erased). Do this by erasing the last page
		 * of the secondary slot.
		 */
		flash_address = PM_MCUBOOT_SECONDARY_ADDRESS
				+ PM_MCUBOOT_SECONDARY_SIZE - 0x4;
		err = flash_page_erase_if_needed(flash_address);
		if (err != 0) {
			report_flash_err(err);
			return err;
		}
		err = boot_request_upgrade(0);
		if (err != 0) {
			LOG_ERR("boot_request_upgrade error %d\n", err);
			report_flash_err(err);
			return err;
		}
		download_client_disconnect(&dfu);
		report_dlc_evt(event);
		break;

	case DOWNLOAD_CLIENT_EVT_ERROR: {
		download_client_disconnect(&dfu);
		report_dlc_evt(event);
		LOG_ERR("Download client error\n");
		break;
	}
	default:
		break;
	}
	return 0;
}

int fota_download_start(char *host, char *file)
{
	__ASSERT(host != NULL, "invalid host\n");
	__ASSERT(file != NULL, "invalid file\n");
	__ASSERT(callback != NULL, "invalid callback\n");

	/* Verify that a download is not already ongoing */
	if (dfu.fd != -1) {
		return -EALREADY;
	}

	int err = download_client_connect(&dfu, host);

	if (err != 0) {
		LOG_ERR("download_client_connect() failed, err %d", err);
		return err;
	}

	for (int i = 0; i < FLASH_PAGE_MAX_CNT; i++) {
		is_flash_page_erased[i] = false;
	}

	err = download_client_start(&dfu, file, 0);
	if (err != 0) {
		LOG_ERR("download_client_start() failed, err %d", err);
		download_client_disconnect(&dfu);
		return err;
	}
	return 0;
}

int fota_download_init(fota_download_callback_t client_callback)
{
	__ASSERT(client_callback != NULL, "invalid client_callback\n");

	callback = client_callback;

	flash_dev = device_get_binding(DT_FLASH_DEV_NAME);
	if (flash_dev == 0) {
		LOG_ERR("Nordic nRF flash driver was not found!\n");
		return -ENXIO;
	}

	int err = download_client_init(&dfu, download_client_callback);

	if (err != 0) {
		LOG_ERR("download_client_init() failed, err %d", err);
		return err;
	}

	return 0;
}

