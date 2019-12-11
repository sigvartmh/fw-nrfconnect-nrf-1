/*
 *Copyright (c) 2019 Nordic Semiconductor ASA
 *
 *SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/**@file aws_fota_json.h
 *
 * @defgroup aws_fota_json
 * @{
 * @brief  Library for parsing AWS Jobs json payloads.
 *
 */

#ifndef AWS_FOTA_JSON_H__
#define AWS_FOTA_JSON_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr.h>
#include <zephyr/types.h>

/** @brief The max JOB_ID_LEN according to AWS docs
 * https://docs.aws.amazon.com/general/latest/gr/aws_service_limits.html
 */
#define STATUS_MAX_LEN (12)

/**
 * @brief Parse a given AWS IoT DescribeJobExecution response JSON object.
 *	  More information on this object can be found at https://docs.aws.amazon.com/iot/latest/developerguide/jobs-api.html#mqtt-describejobexecution
 *
 * @param[in] job_document JSON formated string of an AWS Job execution
 * @param[in] payload_len The length of the provided string.
 * @param[out] job_id_buf Output buffer for the Job ID string read from the JSON
 * @param[out] hostname_buf Output buffer for the hostname read from the JSON
 * @param[out] file_path_buf Output buffer for the file path read from the JSON
 * @param[out] version_number Job document version number.
 *
 * @return 1 if the Job Execution object is empty, 0 if parsed correctly,
 *	   otherwise a negative error code is returned identicating reason of
 *	   failure.
 **/
int aws_fota_parse_job_execution(char *job_document, u32_t payload_len,
		char *job_id_buf, char *hostname_buf, char *file_path_buf,
		int *version_number);

/**
 * @brief Parse a Job Execution accepted response object. Returned by a call to
 *	  https://docs.aws.amazon.com/iot/latest/developerguide/jobs-api.html#mqtt-updatejobexecution
 *
 * @param[in] job_document JSON formated string of an AWS Job execution update.
 * @param[in] payload_len The length of the provided string.
 * @param[out] status String with the AWS Job status after an update.

 * @return 0 for an successful parsing or a negative error code identicating
 *	   reason of failure.
 */
int aws_fota_parse_update_job_exec_state_rsp(char *update_rsp_document,
		size_t payload_len, char *status);

#ifdef __cplusplus
}
#endif

/**
 *@}
 */

#endif /*AWS_FOTA_JSON_H__ */
