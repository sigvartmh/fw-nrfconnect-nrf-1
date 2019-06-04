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

/**
 *Example payload
{
  "timestamp": 1559626627,
  "execution": {
    "jobId": "8467e3b5-c8b3-4145-8124-eb4c95b21538",
    "status": "QUEUED",
    "queuedAt": 1559626593,
    "lastUpdatedAt": 1559626593,
    "versionNumber": 1,
    "executionNumber": 1,
    "jobDocument": {
      "operation": "app_fw_update",
      "fwversion": 293692,
      "size": 181124,
      "location": {
        "protocol": "https:",
        "host": "fota-update-bucket.s3.eu-central-1.amazonaws.com",
        "path": "/update.bin?X-Amz-Algorithm=AWS4-HMAC-SHA256&X-Amz-Credential=AKIAWXEL53DXIU7W72AE%2F20190604%2Feu-central-1%2Fs3%2Faws4_request&X-Amz-Date=20190604T053631Z&X-Amz-Expires=604800&X-Amz-Signature=cd2fb78a66645abce44d851041000f2ece49b01c3298e09d450e09c707726804&X-Amz-SignedHeaders=host"
      }
    }
  }
}
*/

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


#ifdef __cplusplus
}
#endif

/**
 *@}
 */

#endif /*AWS_FOTA_INTERNAL_H__ */
