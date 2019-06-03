/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/**@file aws_jobs_dfu.h
 *
 * @defgroup aws_jobs_client Client to interface with the AWS jobs API.
 * @{
 * @brief Client to download an object.
 */
/* The AWS jobs client provides APIs to:
 *  - report status
 *  - subscribe to job topics
 *  - accept/reject jobs
 *
 * Currently, only the HTTP protocol is supported for download.
 */

#ifndef AWS_JOBS_H__
#define AWS_JOBS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr.h>
#include <zephyr/types.h>
#include <net/mqtt.h>

/** @brief Job Execution Status. */
enum job_execution_status {
	QUEUED = 0,
	IN_PROGRESS,
	SUCCEEDED,
	FAILED,
	TIMED_OUT,
	REJECTED,
	REMOVED,
	CANCELED
};

/** @brief Mapping of enum to strings for Job Execution Status. */
const char * job_execution_status_map [] = {
	"QUEUED",
	"IN_PROGRESS",
	"SUCCEEDED",
	"FAILED",
	"TIMED_OUT",
	"REJECTED",
	"REMOVED",
	"CANCELED"
};


/** @brief Initialize the AWS jobs handler.
 *
 *  @param[in] c	The mqtt client instance.
 *
 *  @retval 0 If initialzed successfully.
 */
int aws_jobs_init(struct mqtt_client * c);

/**@brief Handeling of AWS jobs topics and states
 *
 *  @retval 0 If the event was handled successfully.
 */
int aws_jobs_process(u8_t * topic, u8_t * json_payload);

/**@brief Set the state of the mqtt connection
 *  @param[in] client	The mqtt client instance.
 */
int aws_jobs_connected(bool state);

#ifdef __cplusplus
}
#endif

#endif /* AWS_JOBS_H__ */

