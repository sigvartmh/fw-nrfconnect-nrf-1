/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/** @file fota_download.h
 *
 * @defgroup fota_download Library to download a firmware over the air update.
 * @{
 * @brief Firmware Over the Air Download library
 *
 * @details Downloads the specified file from the specified host to
 * MCUboots secondary partition. After the file has been downloaded,
 * the secondary slot is tagged to have a valid firmware inside it.
 */

#ifndef FOTA_DOWNLOAD_H_
#define FOTA_DOWNLOAD_H_

#include <zephyr.h>
#include <zephyr/types.h>
#include <download_client.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief FOTA download event IDs.
 */
enum fota_download_evt_id {
	/** Event contains a download client event. */
	FOTA_DOWNLOAD_EVT_DOWNLOAD_CLIENT,
	/** Event contains a flash error code. */
	FOTA_DOWNLOAD_FLASH_ERR,
};

/**
 * @brief FOTA download event.
 */
struct fota_download_evt {
	/** The event ID */
	enum fota_download_evt_id id;
	union {
		const struct download_client_evt *dlc_evt;
		int flash_error_code;
	};
};

/**
 * @brief FOTA download asynchronous callback function.
 *
 * The application receives events through this callback. These events can
 * come from two sources, the flash storage system or the download client.
 * The source of the event is given by the fota_download_evt_id.
 *
 * @param[in] event The event.
 *
 */
typedef void (*fota_download_callback_t)(
	const struct fota_download_evt *evt);

/**@brief Start downloading the given file from the given host.
 *
 * When the download is complete, MCUboots secondary slot is tagged as having
 * a valid firmware inside it. The completion is reported through an event.
 *
 * @retval 0 	     If download has started successfully.
 * @retval -EALREADY If download is already ongoing.
 *           Otherwise, a negative value is returned.
 */
int fota_download_start(char *host, char *file);

/**@brief Initialize the Firmware Over the Air Download library.
 *
 * @param client_callback Callback for events generated.
 *
 * @retval 0 If successfully initialized.
 *           Otherwise, a negative value is returned.
 */
int fota_download_init(fota_download_callback_t client_callback);

#ifdef __cplusplus
}
#endif

#endif /* FOTA_DOWNLOAD_H_ */

/**@} */
