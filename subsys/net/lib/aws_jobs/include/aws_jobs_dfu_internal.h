/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef AWS_JOBS_INTERNAL_H__
#define AWS_JOBS_INTERNAL_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Enum to keep the fota status */
enum fota_status {
	DOWNLOAD_FIRMWARE = 0,
	APPLY_FIRMWARE,
	NONE
};

/* Map of fota status to report back */
const char * fota_status_map [] = {
	"download_firmware",
	"apply_update",
	"none"
};

static enum fota_status fota_state;

#ifdef __cplusplus
}
#endif

#endif /* AWS_JOBS_INTERNAL_H__ */
