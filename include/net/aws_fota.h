/*
 *Copyright (c) 2019 Nordic Semiconductor ASA
 *
 *SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/**@file
 *@brief AWS FOTA library header.
 */

#ifndef AWS_FOTA_H__
#define AWS_FOTA_H__

#ifdef __cplusplus
extern "C" {
#endif
/* Enum to keep the fota status */
enum fota_status {
	NONE = 0,
	DOWNLOAD_FIRMWARE,
	APPLY_FIRMWARE,
};

/* Map of fota status to report back */
const char *fota_status_strings[] = {
	[DOWNLOAD_FIRMWARE] = "download_firmware",
	[APPLY_FIRMWARE] = "apply_update",
	[NONE] = "none",
};

enum aws_fota_evt_id {
	/** AWS FOTA complete and status reported to job document */
	AWS_FOTA_EVT_DONE,
	/** AWS FOTA error */
	AWS_FOTA_EVT_ERROR
};

typedef void (*aws_fota_callback_t)(enum aws_fota_evt_id evt_id);

/**@brief Initialize the AWS Firmware Over the Air library.
 *
 * @param app_version Current version number of the application as a \0
 *	terminated ASCII string.
 * @param callback Callback for events generated.
 *
 * @retval 0 If successfully initialized.
 * @retval -EINVAL If any of the input values are invalid.
 *           Otherwise, a negative value is returned.
 */
int aws_fota_init(struct mqtt_client *const client,
		  const char *app_version,
		  aws_fota_callback_t cb);

int aws_fota_mqtt_evt_handler(struct mqtt_client *const client,
			      const struct mqtt_evt *evt);

#ifdef __cplusplus
}
#endif

/**
 *@}
 */

#endif /*AWS_FOTA_H__ */
