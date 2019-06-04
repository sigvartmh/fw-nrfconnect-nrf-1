/*
 *Copyright (c) 2019 Nordic Semiconductor ASA
 *
 *SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/**@file
 *@brief AWS FOTA library header.
 */

#ifndef AWS_FOTA_INTERNAL_H__
#define AWS_FOTA_INTERNAL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr.h>
#include <zephyr/types.h>
#include <net/mqtt.h>

struct location_obj {
	const char *protocol;
	const char *host;
	const char *path;
};

struct job_document_obj {
	const char *operation;
	const char *fw_version;
	int size;
	struct location_obj location;
};

struct execution_obj {
	const char *job_id;
	const char *status;
	int queued_at;
	int last_update_at;
	int version_number;
	int execution_number;
	struct job_document_obj job_document;
};

struct notify_next_obj {
	int timestamp;
	struct execution_obj execution;
};

struct status_details_obj {
	const char *next_state;
};

struct execution_state_obj {
	const char *status;
	struct status_details_obj status_details;
	int version_number;
};

struct update_response_obj {
	struct execution_state_obj execution_state;
	const char *job_document;
	int timestamp;
	const char *client_token;
};


#ifdef __cplusplus
}
#endif

/**
 *@}
 */

#endif /*AWS_FOTA_INTERNAL_H__ */
