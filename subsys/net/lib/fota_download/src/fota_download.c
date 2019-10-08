/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <pm_config.h>
#include <logging/log.h>
#include <net/fota_download.h>
#include <net/download_client.h>
#include <dfu/dfu_context_handler.h>

LOG_MODULE_REGISTER(fota_download, CONFIG_FOTA_DOWNLOAD_LOG_LEVEL);

static fota_download_callback_t callback;
static struct download_client   dlc;
static struct k_delayed_work    restart_dlc_work;
static int                      offset;

static int download_client_callback(const struct download_client_evt *event)
{
	static bool first_fragment = true;
	int err;

	if (event == NULL) {
		return -EINVAL;
	}

	switch (event->id) {
	case DOWNLOAD_CLIENT_EVT_FRAGMENT: {
		size_t file_size;

		err = download_client_file_size_get(&dlc, &file_size);
		if (err != 0) {
			LOG_ERR("download_client_file_size_get error %d", err);
			callback(FOTA_DOWNLOAD_EVT_ERROR);
			return err;
		}

		if (file_size > PM_MCUBOOT_SECONDARY_SIZE) {
			LOG_ERR("Requested file too big to fit in flash\n");
			callback(FOTA_DOWNLOAD_EVT_ERROR);
			return -EFBIG;
		}

		if (first_fragment) {
			first_fragment = false;
			int img_type = dfu_ctx_img_type(event->fragment.buf,
						        event->fragment.len);

			err = dfu_ctx_init(img_type);
			if (err != 0) {
				LOG_ERR("dfu_ctx_init error %d", err);
				return err;
			}

			/* Should offset be static in this file, or should we
			 * require that dfu_ctx_offset is idempotent?
			 */
			offset = dfu_ctx_offset();
			LOG_INF("Offset: 0x%x", offset);

			if (offset != 0) {
				/* Abort current download procedure, and
				 * schedule new download from offset.
				 */
				k_delayed_work_submit(&restart_dlc_work,
						K_SECONDS(1));

				return -1;
			}

		}

		err = dfu_ctx_write(event->fragment.buf, event->fragment.len);
		if (err != 0) {
			LOG_ERR("dfu_ctx_write error %d", err);
			err = download_client_disconnect(&dlc);
			callback(FOTA_DOWNLOAD_EVT_ERROR);
			return err;
		}
	break;
	}

	case DOWNLOAD_CLIENT_EVT_DONE:
		err = dfu_ctx_done();
		if (err != 0) {
			LOG_ERR("dfu_ctx_done error: %d", err);
			callback(FOTA_DOWNLOAD_EVT_ERROR);
			return err;
		}

		err = download_client_disconnect(&dlc);
		if (err != 0) {
			LOG_ERR("download_client_disconncet error %d", err);
			callback(FOTA_DOWNLOAD_EVT_ERROR);
			return err;
		}
		first_fragment = true;
		break;

	case DOWNLOAD_CLIENT_EVT_ERROR: {
		download_client_disconnect(&dlc);
		first_fragment = true;
		LOG_ERR("Download client error");
		callback(FOTA_DOWNLOAD_EVT_ERROR);
		return event->error;
	}
	default:
		break;
	}

	return 0;
}

static void download_with_offset(struct k_work *unused)
{
	LOG_INF("Downloading from offset: 0x%d", offset);
	int err = download_client_start(&dlc, dlc.file, offset);
	if (err != 0) {
		LOG_ERR("download_client_start error %d", err);
	}
}

int fota_download_start(char *host, char *file)
{
	int err = -1;

	/* TODO: add configurability of the .sec_tag for TLS certificates */
	struct download_client_cfg config = {
		.sec_tag = -1, /* HTTP */
	};

	if (host == NULL || file == NULL || callback == NULL) {
		return -EINVAL;
	}

	/* TODO: Is it possible to download modem fw with a threaded model from
	 * a host while downloading firmware that's written to flash?
	 */

	/* Verify that a download is not already ongoing */
	if (dlc.fd != -1) {
		return -EALREADY;
	}

	err = download_client_connect(&dlc, host, &config);

	if (err != 0) {
		LOG_ERR("download_client_connect error %d", err);
		return err;
	}


	err = download_client_start(&dlc, file, 0);
	if (err != 0) {
		LOG_ERR("download_client_start error %d", err);
		download_client_disconnect(&dlc);
		return err;
	}
	return 0;
}

int fota_download_init(fota_download_callback_t client_callback)
{
	if (client_callback == NULL) {
		return -EINVAL;
	}

	offset = 0;

	callback = client_callback;

	k_delayed_work_init(&restart_dlc_work, download_with_offset);

	int err = download_client_init(&dlc, download_client_callback);

	if (err != 0) {
		LOG_ERR("download_client_init error %d", err);
		return err;
	}


	return 0;
}
