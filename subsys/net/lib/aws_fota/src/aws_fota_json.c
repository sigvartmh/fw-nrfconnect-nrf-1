/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <string.h>
#include <json.h>
#include <cJSON.h>
#define JSMN_HEADER
#include <jsmn.h>
#include <sys/util.h>
#include <net/aws_jobs.h>

#include "aws_fota_json.h"

/**@brief Copy max maxlen bytes from src to dst. Insert null-terminator.
 */
static void strncpy_nullterm(char *dst, const char *src, size_t maxlen)
{
	size_t len = strlen(src) + 1;

	memcpy(dst, src, MIN(len, maxlen));
	if (len > maxlen) {
		dst[maxlen - 1] = '\0';
	}
}

int aws_fota_parse_UpdateJobExecution_rsp(const char *update_rsp_document,
					  size_t payload_len, char *status_buf)
{
	if (update_rsp_document == NULL || status_buf == NULL) {
		return -EINVAL;
	}

	cJSON * update_response = cJSON_Parse(update_rsp_document);

	if (update_response == NULL) {
		cJSON_Delete(update_response);
		return -ENODATA;
	}

	cJSON * status = cJSON_GetObjectItemCaseSensitive(update_response,
							  "status");
	if (cJSON_IsString(status) && status->valuestring != NULL) {
		strncpy_nullterm(status_buf, status->valuestring,
				 STATUS_MAX_LEN);
	} else {
		cJSON_Delete(update_response);
		return -ENODATA;
	}

	cJSON_Delete(update_response);
	return 0;
}

static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
  if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
      strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
    return 0;
  }
  return -1;
}


jsmntok_t tokens[128];
char * json_token_tostr(char *js, jsmntok_t *t) {
}

int aws_fota_parse_DescribeJobExecution_rsp(const char *job_document,
					   u32_t payload_len,
					   char *job_id_buf,
					   char *hostname_buf,
					   char *file_path_buf,
					   int *execution_version_number)
{
	if (job_document == NULL
	    || job_id_buf == NULL
	    || hostname_buf == NULL
	    || file_path_buf == NULL
	    || execution_version_number == NULL) {
		return -EINVAL;
	}

	jsmn_parser parser;
	jsmn_init(&parser);
	int result = jsmn_parse(&parser, job_document, payload_len, tokens,
				ARRAY_SIZE(tokens));
	if (result < 0) {
		switch(result):
		case JSMN_ERROR_NOMEM:
			return -ENOMEM;
		case JSMN_ERROR_INVAL:
			return -EINVAL;
		case JSMN_ERROR_PART:


		if(result == JSMN_ERROR_NOMEM) {
		}
		printk("Failed to parse JSON: %d\n", result);
	}

	if (result < 1 || tokens[0].type != JSMN_OBJECT) {
		return -ENODATA;
	}
	for (int i = 1; i < result; i++) {
		if(jsoneq(job_document, &tokens[i], "execution") == 0) {
			printk("Found execution object");
		}
		if(jsoneq(job_document, &tokens[i], "jobId") == 0){
			printk("Found jobId");
		}
		if(jsoneq(job_document, &tokens[i], "jobDocument") == 0){
		}
		if(jsoneq(job_document, &tokens[i], "location") == 0){
		}
		if(jsoneq(job_document, &tokens[i], "host") == 0){
		}
		if(jsoneq(job_document, &tokens[i], "path") == 0){
		}
		if(jsoneq(job_document, &tokens[i], "versionNumber") == 0){
			printk("Found versionNumber");
		}
	}



	cJSON * json_data = cJSON_Parse(job_document);

	if (json_data == NULL) {
		const char *err = cJSON_GetErrorPtr();
		/* LOG_ERR("Unable to parse Job Execution err before: %s", err); */
		cJSON_Delete(json_data);
		return -ENODATA;
	}

	cJSON * execution = cJSON_GetObjectItemCaseSensitive(json_data,
							    "execution");
	if (execution == NULL) {
		return 0;
	}

	cJSON * job_id = cJSON_GetObjectItemCaseSensitive(execution, "jobId");

	if (cJSON_IsString(job_id) && job_id->valuestring != NULL) {
		strncpy_nullterm(job_id_buf, job_id->valuestring,
				AWS_JOBS_JOB_ID_MAX_LEN);
	} else {
		cJSON_Delete(json_data);
		return -ENODATA;
	}

	cJSON * job_data = cJSON_GetObjectItemCaseSensitive(execution,
							   "jobDocument");

	if (!cJSON_IsObject(job_data) && job_data == NULL) {
		cJSON_Delete(json_data);
		return -ENODATA;
	}

	cJSON * location = cJSON_GetObjectItemCaseSensitive(job_data,
							    "location");

	if (!cJSON_IsObject(location) && location == NULL) {
		cJSON_Delete(json_data);
		return -ENODATA;
	}


	cJSON * hostname = cJSON_GetObjectItemCaseSensitive(location, "host");
	cJSON *path = cJSON_GetObjectItemCaseSensitive(location, "path");

	if ((cJSON_IsString(hostname) && hostname->valuestring != NULL)
	    && (cJSON_IsString(path) && path->valuestring != NULL)) {
		strncpy_nullterm(hostname_buf, hostname->valuestring,
				CONFIG_AWS_FOTA_HOSTNAME_MAX_LEN);
		strncpy_nullterm(file_path_buf, path->valuestring,
				CONFIG_AWS_FOTA_FILE_PATH_MAX_LEN);
	} else {
		cJSON_Delete(json_data);
		return -ENODATA;
	}

	cJSON *version_number = cJSON_GetObjectItemCaseSensitive(execution,
					"versionNumber");

	if (cJSON_IsNumber(version_number)) {
		*execution_version_number = version_number->valueint;
	} else {
		cJSON_Delete(json_data);
		return -ENODATA;
	}

	cJSON_Delete(json_data);

	return 1;
}
