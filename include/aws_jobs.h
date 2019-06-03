/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/** @file
 * @brief AWS Jobs library header.
 */

#ifndef AWS_JOBS_H__
#define AWS_JOBS_H__

/**
 * @defgroup aws_jobs AWS Jobs library
 * @{
 * @brief AWS Jobs Library.
 *
 *  The AWS Jobs library provides APIs to:
 *  - Report status
 *  - Subscribe to job topics
 *  - String templates for topics
 *  - Defines for lengths of topics, status and job IDs
 *  - Defines for Subscribe message IDs
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr.h>
#include <zephyr/types.h>
#include <net/mqtt.h>


/* @brief The max JOB_ID_LEN according to AWS docs
 * https://docs.aws.amazon.com/general/latest/gr/aws_service_limits.html
 */
#define JOB_ID_MAX_LEN (64)
#define STATUS_MAX_LEN (12)


/* @brief MQTT message IDs used for identifying subscribe message ACKs */
#define SUBSCRIBE_ID_BASE (2110)
#define SUBSCRIBE_NOTIFY (SUBSCRIBE_ID_BASE + 1)
#define SUBSCRIBE_NOTIFY_NEXT (SUBSCRIBE_ID_BASE + 2)
#define SUBSCRIBE_GET (SUBSCRIBE_ID_BASE + 3)
#define SUBSCRIBE_GET_JOB_ID (SUBSCRIBE_ID_BASE + 4)
#define SUBSCRIBE_JOB_ID_UPDATE (SUBSCRIBE_ID_BASE + 5)
#define SUBSCRIBE_EXPECTED (SUBSCRIBE_ID_BASE + 10)


/**
 * @brief String templates for constructing AWS Jobs topics and their max
 * length.
 */
#define AWS "$aws/things/"
#define AWS_LEN (sizeof(AWS) - 1)

#define NOTIFY_TOPIC_TEMPLATE AWS "%s/jobs/notify"
#define NOTIFY_TOPIC_MAX_LEN (AWS_LEN + CONFIG_CLIENT_ID_LEN + 12)

#define NOTIFY_NEXT_TOPIC_TEMPLATE AWS "%s/jobs/notify-next"
#define NOTIFY_NEXT_TOPIC_MAX_LEN (AWS_LEN + CONFIG_CLIENT_ID_LEN + 18)

#define GET_TOPIC_TEMPLATE AWS "%s/jobs/get/%s"
#define GET_TOPIC_LEN (AWS_LEN + CONFIG_CLIENT_ID_LEN + 11)

#define JOB_ID_GET_TOPIC_TEMPLATE AWS "%s/jobs/%s/get/%s"
#define JOB_ID_GET_TOPIC_MAX_LEN (AWS_LEN +\
				 CONFIG_CLIENT_ID_LEN +\
				 JOB_ID_MAX_LEN +\
				 12)

#define JOB_ID_UPDATE_TOPIC_TEMPLATE AWS "%s/jobs/%s/update"
#define JOB_ID_UPDATE_TOPIC_MAX_LEN (AWS_LEN +\
				     CONFIG_CLIENT_ID_LEN +\
				     JOB_ID_MAX_LEN +\
				     14)


/** @brief Job Execution Status. */
enum execution_status {
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
const char *execution_status_map[] = {
	[QUEUED] = "QUEUED",
	[IN_PROGRESS] = "IN_PROGRESS",
	[SUCCEEDED] = "SUCCEEDED",
	[FAILED] = "FAILED",
	[TIMED_OUT] = "TIMED_OUT",
	[REJECTED] = "REJECTED",
	[REMOVED] = "REMOVED",
	[CANCELED] = "CANCELED"
};


/**
 * @brief Constructs the get topic for accepted/rejected AWS Jobs and subscribe
 *	to it.
 *
 * @param[in] client Connected MQTT client instance.
 *
 * @retval 0 If successful otherwise the return code of mqtt_subscribe.
 */
int aws_jobs_subscribe_get(struct mqtt_client *client);


/**
 * @brief Constructs then the notify-next topic subscribes for reciving
 *	AWS IoT jobs.
 *
 * @param[in] client Connected MQTT client instance.
 *
 * @retval 0 If sucessful otherwise the return code of mqtt_subscribe.
 */
int aws_jobs_subscribe_notify_next(struct mqtt_client *client);


/**
 * @brief Constructs then subscribes to notify topic for reciving  AWS IoT jobs.
 *
 * @param[in] client Connected MQTT client instance.
 *
 * @retval 0 If sucessful otherwise the return code of mqtt_subscribe.
 */
int aws_jobs_subscribe_notify(struct mqtt_client *client);


/**@brief Subscribe to the expected topics of the AWS documentation
 * https://docs.aws.amazon.com/iot/latest/developerguide/jobs-devices.html.
 *
 * @param[in] client Connected MQTT client instance.
 * @param[in] notify_next Subscribes to notify next if true and notify if false.
 *
 * @retval 0 If sucessful otherwise the return code of mqtt_subscribe.
 */
int aws_jobs_subscribe_expected_topics(struct mqtt_client *client,
				       bool notify_next);


/**
 * @brief Constructs the get topic for accepted and rejeceted AWS jobs.
 *
 * @param[in] client Connected MQTT client instance.
 * @param[in] job_id JobId of the current accepted job.
 *
 * @retval 0 If sucessful otherwise the return code of mqtt_subscribe.
 */
int subscribe_job_id_get_topic(struct mqtt_client *client, u8_t *job_id);


/**
 * @brief AWS jobs update job execution status function.
 *
 * This implements the minimal requirements for doing an AWS job only
 * updating status and status details update are supported.
 *
 * @param[in] client Initialized and connected MQTT Client instance.
 * @param[in] job_id The ID of the job which you are updating. This will be a
 *		part of the MQTT topic used to update the job.
 * @param[in] status The Job execution status.
 * @param[in] status_details JSON object in string format containing additional
		information. This object can contain up to 10 fields according
		to the AWS IoT jobs documentation.
 * @param[in] expected_version The expected job document version this needs to
 *		be incremented by 1 for every update.
 * @param[in] client_token This can be an arbitrary value and will be reflected
 *		in the response to the update.
 *
 * @retval 0 If update is published successful otherwise return mqtt_publish
 *	     error code.
 */
int aws_update_job_execution(struct mqtt_client *client,
			     u8_t *job_id,
			     enum execution_status status,
			     u8_t *status_details,
			     int expected_version,
			     u8_t *client_token);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* AWS_JOBS_H__ */

