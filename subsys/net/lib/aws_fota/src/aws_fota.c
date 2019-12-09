/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <stdio.h>
#include <json.h>
#include <net/fota_download.h>
#include <net/aws_jobs.h>
#include <net/aws_fota.h>
#include <logging/log.h>

#include "aws_fota_json.h"

LOG_MODULE_REGISTER(aws_fota, CONFIG_AWS_FOTA_LOG_LEVEL);

/* Enum to keep the fota status */
enum fota_status {
	NONE = 0,
	DOWNLOAD_FIRMWARE,
	APPLY_UPDATE,
};

/* Map of fota status to report back */
static const char * const fota_status_strings[] = {
	[DOWNLOAD_FIRMWARE] = "download_firmware",
	[APPLY_UPDATE] = "apply_update",
	[NONE] = "none",
};

/* Pointer to initialized MQTT client instance */
static struct mqtt_client *c;

/* Enum for tracking the job exectuion state */
static enum execution_status execution_state = AWS_JOBS_QUEUED;
static enum fota_status fota_state = NONE;

/* Document version starts at 1 and is incremented with each accepted update */
static u32_t doc_version_number;

/* Buffer for reporting the current application version */
static char version[CONFIG_AWS_FOTA_VERSION_STRING_MAX_LEN];

/* Allocated strings for topics */
static u8_t notify_next_topic[AWS_JOBS_TOPIC_MAX_LEN];
static u8_t update_topic[AWS_JOBS_TOPIC_MAX_LEN];
static u8_t get_topic[AWS_JOBS_TOPIC_MAX_LEN];

/* Allocated buffers for keeping hostname, json payload and file_path */
static u8_t payload_buf[CONFIG_AWS_FOTA_PAYLOAD_SIZE];
static u8_t hostname[CONFIG_AWS_FOTA_HOSTNAME_MAX_LEN];
static u8_t file_path[CONFIG_AWS_FOTA_FILE_PATH_MAX_LEN];
static u8_t job_id[AWS_JOBS_JOB_ID_MAX_LEN];
static aws_fota_callback_t callback;

static int get_published_payload(struct mqtt_client *client, u8_t *write_buf,
				 size_t length)
{
	u8_t *buf = write_buf;
	u8_t *end = buf + length;

	if (length > sizeof(payload_buf)) {
		return -EMSGSIZE;
	}
	while (buf < end) {
		int ret = mqtt_read_publish_payload_blocking(client, buf, end - buf);

		if (ret < 0) {
			return ret;
		} else if (ret == 0) {
			return -EIO;
		}
		buf += ret;
	}
	return 0;
}


#define AWS_FOTA_STATUS_DETAILS_TEMPLATE "{\"nextState\":\"%s\","\
					 "\"progress\":%d}"
#define STATUS_DETAILS_MAX_LEN  (sizeof("{\"nextState\":\"\"}") \
				+ (sizeof("download_firmware") + 2))

static int update_job_execution(struct mqtt_client *const client,
				const u8_t *job_id,
				enum execution_status state,
				enum fota_status next_state,
				int progress,
				int version_number,
				const char *client_token)
{
	char status_details[STATUS_DETAILS_MAX_LEN + 1];
	int ret = snprintf(status_details,
			   sizeof(status_details),
			   AWS_FOTA_STATUS_DETAILS_TEMPLATE,
			   fota_status_strings[next_state],
			   progress);
	__ASSERT(ret >= 0, "snprintf returned error %d\n", ret);
	__ASSERT(ret < STATUS_DETAILS_MAX_LEN,
		"Not enough space for status, need %d bytes\n", ret+1);

	ret =  aws_jobs_update_job_execution(client, job_id, state,
					     status_details, version_number,
					     client_token, update_topic);

	if (ret < 0) {
		LOG_ERR("aws_jobs_update_job_execution failed: %d", ret);
	}

	return ret;
}

static int get_job(struct mqtt_client *const client, u32_t payload_len)
{
	int err = get_published_payload(client, payload_buf, payload_len);

	if (err) {
		LOG_ERR("error when getting the payload: %d", err);
		return err;
	}

	/* Check if message received is a job. */
	err = aws_fota_parse_notify_next_document(payload_buf,
			payload_len, job_id,
			hostname, file_path,
			&doc_version_number);
	if (err < 0) {
		LOG_ERR("Error when parsing the json: %d", err);
		return err;
	} else  if (err == 1) {
		/* Empty job execution object */
		LOG_INF("No job execution object found: %s",
				log_strdup(payload_buf));
		return 1;
	}

	LOG_DBG("Job ID: %s", job_id);
	LOG_DBG("hostname: %s", hostname);
	LOG_DBG("file_path %s", file_path);
	LOG_DBG("doc_version_number: %d ", doc_version_number);

	/* Subscribe to update topic to recive feedback on whether an update is
	 * accepted or not.
	 */

	err = aws_jobs_subscribe_topic_update(client, job_id,  update_topic);
	if (err) {
		LOG_ERR("Error when subscribing job_id_update: %d", err);
		return err;
	}

	/* Set fota_state to DOWNLOAD_FIRMWARE, when we are subscribed
	 * to job_id topics we will try to publish and if accepted we
	 * can start the download
	 */

	fota_state = DOWNLOAD_FIRMWARE;

	return 1;
}

static int update_accepted(struct mqtt_client *const client, u32_t payload_len)
{
	LOG_DBG("Job document update was accepted");
	int err = get_published_payload(client, payload_buf, payload_len);

	if (err) {
		return err;
	}
	/* Update accepted, increment document version counter. */
	doc_version_number++;

	if (fota_state == DOWNLOAD_FIRMWARE) {
		/*Job document is updated and we are ready to download
		 * the firmware.
		 */
		execution_state = AWS_JOBS_IN_PROGRESS;
		LOG_INF("Start downloading firmware from %s%s",
				log_strdup(hostname), log_strdup(file_path));
		err = fota_download_start(hostname, file_path);
		if (err) {
			LOG_ERR("Error when trying to start firmware download: "
				"%d", err);
			return err;
		}
	} else if (execution_state == AWS_JOBS_IN_PROGRESS &&
			fota_state == APPLY_UPDATE) {
		LOG_INF("Firmware download completed");
		execution_state = AWS_JOBS_SUCCEEDED;
		err = update_job_execution(client, job_id,
				execution_state, fota_state,
				doc_version_number, "");
		if (err) {
			return err;
		}
	} else if (execution_state == AWS_JOBS_SUCCEEDED &&
			fota_state == APPLY_UPDATE) {
		LOG_INF("Job document updated with SUCCEDED");
		LOG_INF("Ready to reboot");
		callback(AWS_FOTA_EVT_DONE);
	}
	return 1;
}

static int aws_fota_on_publish_evt(struct mqtt_client *const client,
				   const u8_t *topic,
				   u32_t topic_len,
				   u32_t payload_len)
{
	int err;

	LOG_INF("Received topic: %s", log_strdup(topic));

	bool is_get_next_topic = aws_jobs_cmp(get_topic, topic, topic_len, "");
	bool is_get_next_accepted = aws_jobs_cmp(get_topic, topic, topic_len,
						 "accepted");
	bool is_notify_next_topic = aws_jobs_cmp(notify_next_topic, topic,
						 topic_len, "");
	bool doc_update_accepted = aws_jobs_cmp(update_topic, topic, topic_len,
					       "accepted");
	bool doc_update_rejected = aws_jobs_cmp(update_topic, topic, topic_len,
						"rejected");

	if (is_notify_next_topic || is_get_next_topic || is_get_next_accepted) {
		/* Already job in progress */
		return get_job(client, payload_len);
	} else if (doc_update_accepted) {
		return update_accepted(client, payload_len);
	} else if (doc_update_rejected) {
		LOG_ERR("Job document update was rejected");
		err = get_published_payload(client, payload_buf, payload_len);
		if (err) {
			LOG_ERR("Error when getting the payload: %d", err);
			return err;
		}
		/* TODO: Print reason */
		callback(AWS_FOTA_EVT_ERROR);
		return -EFAULT;
	}
	LOG_DBG("Recived an unhandled MQTT publish event on topic: %s",
		log_strdup(topic));
	return 0;
}

int aws_fota_mqtt_evt_handler(struct mqtt_client *const client,
			      const struct mqtt_evt *evt)
{
	int err;

	switch (evt->type) {
	case MQTT_EVT_CONNACK:
		if (evt->result != 0) {
			return 0;
		}

		err = aws_jobs_subscribe_topic_notify_next(client,
							   notify_next_topic);
		if (err) {
			LOG_ERR("Unable to subscribe to notify-next topic");
			return err;
		}

		err = aws_jobs_subscribe_topic_get(client, "$next", get_topic);
		if (err) {
			LOG_ERR("Unable to subscribe to jobs/$next/get");
			return err;
		}

		return 0;
		/* This expects that the application's mqtt handler will handle
		 * any situations where you could not connect to the MQTT
		 * broker.
		 */

	case MQTT_EVT_DISCONNECT:
		return 0;

	case MQTT_EVT_PUBLISH: {
		const struct mqtt_publish_param *p = &evt->param.publish;

		err = aws_fota_on_publish_evt(client,
					      p->message.topic.topic.utf8,
					      p->message.topic.topic.size,
					      p->message.payload.len);
		if (err < 1) {
			return err;
		}

		if (p->message.topic.qos == MQTT_QOS_1_AT_LEAST_ONCE) {
			const struct mqtt_puback_param ack = {
				.message_id = p->message_id
			};

			/* Send acknowledgment. */
			err = mqtt_publish_qos1_ack(c, &ack);
			if (err) {
				return err;
			}
		}
		return 1;

	} break;

	case MQTT_EVT_PUBACK:
		if (evt->result != 0) {
			LOG_ERR("MQTT PUBACK error %d", evt->result);
			return 0;
		}
		return 0;

	case MQTT_EVT_SUBACK:
		if (evt->result != 0) {
			return 0;
		}
		if (evt->param.suback.message_id == SUBSCRIBE_NOTIFY_NEXT) {
			LOG_INF("subscribed to notify-next topic");
			err = aws_jobs_get_job_execution(client, "$next",
							 get_topic);
			if (err) {
				return err;
			}
			return 1;
		}

		if (evt->param.suback.message_id == SUBSCRIBE_GET) {
			LOG_INF("subscribed to get topic");
			return 0;
		}

		if ((fota_state == DOWNLOAD_FIRMWARE) &&
		   (evt->param.suback.message_id == SUBSCRIBE_JOB_ID_UPDATE)) {
			err = update_job_execution(client, job_id,
						   AWS_JOBS_IN_PROGRESS,
						   fota_state,
						   doc_version_number,
						   "");
			if (err) {
				return err;
			}
			return 1;
		}

		return 0;

	default:
		/* Handling for default case? */
		return 0;
	}
	return 0;
}

static void http_fota_handler(struct fota_download_evt evt)
{
	__ASSERT_NO_MSG(c != NULL);

	int err = 0;

	switch (evt->id) {
	case FOTA_DOWNLOAD_EVT_FINISHED:
		LOG_INF("FOTA download completed evt recived");
		fota_state = APPLY_UPDATE;
		err = update_job_execution(c, job_id, AWS_JOBS_IN_PROGRESS,
				     fota_state, doc_version_number, "");
		if (err != 0) {
			callback(AWS_FOTA_EVT_ERROR);
		}
		break;
	case FOTA_DOWNLOAD_EVT_ERROR:
		LOG_ERR("FOTA download failed, report back");
		(void) update_job_execution(c, job_id, AWS_JOBS_FAILED,
				     fota_state, doc_version_number, "");
		callback(AWS_FOTA_EVT_ERROR);
		break;
	case FOTA_DOWNLOAD_EVT_PROGRESS:
		//err = update_job_execution_progress
		//if(err != 0) {
		//LOG_ERR("Unable to report progress");
		break;
	}

}

int aws_fota_init(struct mqtt_client *const client,
		  const char *app_version,
		  aws_fota_callback_t evt_handler)
{
	int err;

	if (client == NULL || app_version == NULL || evt_handler == NULL) {
		return -EINVAL;
	}

	if (strlen(app_version) >= CONFIG_AWS_FOTA_VERSION_STRING_MAX_LEN) {
		return -EINVAL;
	}

	/* Store client to make it available in event handlers. */
	c = client;
	callback = evt_handler;

	err = fota_download_init(http_fota_handler);
	if (err != 0) {
		LOG_ERR("fota_download_init error %d", err);
		return err;
	}

	strncpy(version, app_version, CONFIG_AWS_FOTA_VERSION_STRING_MAX_LEN);

	return 0;
}
