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
/* Pointer to initialized mount point for file system*/
static fs_mount_t * mp;

/* Enum for tracking the job exectuion state */
static enum execution_status execution_state = AWS_JOBS_QUEUED;
static enum fota_status fota_state = NONE;

/* Document version starts at 1 and is incremented with each accepted update */
static u32_t doc_version_number = 1;

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


#define AWS_FOTA_STATUS_DETAILS_TEMPLATE "{\"nextState\":\"%s\"}"
#define STATUS_DETAILS_MAX_LEN  (sizeof("{\"nextState\":\"\"}") \
				+ (sizeof("download_firmware") + 2))

static int update_job_execution(struct mqtt_client *const client,
				const u8_t *job_id,
				enum execution_status state,
				enum fota_status next_state,
				int version_number,
				const char *client_token)
{
	char status_details[STATUS_DETAILS_MAX_LEN + 1];
	int ret = snprintf(status_details, sizeof(status_details),
			   AWS_FOTA_STATUS_DETAILS_TEMPLATE,
			   fota_status_strings[next_state]);
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


static int aws_fota_on_publish_evt(struct mqtt_client *const client,
				   const u8_t *topic,
				   u32_t topic_len,
				   u32_t payload_len)
{
	int err;

	LOG_INF("Received topic: %s", log_strdup(topic));

	bool is_get_next_topic = aws_jobs_cmp(get_topic, topic, topic_len, "");
	bool is_notify_next_topic = aws_jobs_cmp(notify_next_topic, topic,
						 topic_len, "");

	if (is_notify_next_topic || is_get_next_topic) {
		err = get_published_payload(client, payload_buf, payload_len);
		if (err) {
			LOG_ERR("Error when getting the payload: %d", err);
			return err;
		}
		/* Check if message received is a job. */
		err = aws_fota_parse_notify_next_document(payload_buf,
							  payload_len, job_id,
							  hostname, file_path);

		if (err < 0) {
			LOG_ERR("Error when parsing the json: %d", err);
			return err;
		} else  if (err == 1) {
			LOG_INF("Got only one field: %s",
				log_strdup(payload_buf));
			return 0;
		}

		/* Unsubscribe from notify_next_topic to not recive more jobs
		 * while processing the current job.
		 */
		err = aws_jobs_unsubscribe_topic_notify_next(client,
							     notify_next_topic);
		if (err) {
			LOG_ERR("Error when unsubscribing notify_next_topic: "
			       "%d", err);
			return err;
		}

		/* Subscribe to update topic to recive feedback on wether an
		 * update is accepted or not.
		 */
		err = aws_jobs_subscribe_topic_update(client, job_id,
						      update_topic);
		if (err) {
			LOG_ERR("Error when subscribing job_id_update: "
				"%d", err);
			return err;
		}
		fs_file_t file_handler;
		fs_open(file_handler, FILE_JOB_ID);
		fs_write(file_handler, job_id, sizeof(job_id));
		fs_close(file_handler);	

		/* Set fota_state to DOWNLOAD_FIRMWARE, when we are subscribed
		 * to job_id topics we will try to publish and if accepted we
		 * can start the download
		 */
		fota_state = DOWNLOAD_FIRMWARE;
		
		/* Handled by the library */
		return 0;

	} else if (aws_jobs_cmp(update_topic, topic, topic_len, "accepted")) {
		LOG_DBG("Job document update was accepted");
		err = get_published_payload(client, payload_buf, payload_len);
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
				LOG_ERR("Error when trying to start firmware"
				       "download: %d", err);
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
	} else if (aws_jobs_cmp(update_topic, topic, topic_len, "rejected")) {
		LOG_ERR("Job document update was rejected");
		err = get_published_payload(client, payload_buf, payload_len);
		if (err) {
			LOG_ERR("Error when getting the payload: %d", err);
			return err;
		}
		callback(AWS_FOTA_EVT_ERROR);
		return -EFAULT;
	}
	LOG_INF("Recived an unhandled MQTT publish event on topic: %s",
		log_strdup(topic));
	return 0;

}

static int connack_evt(struct mqtt_client *const client)
{
	int err;

	err = aws_jobs_subscribe_topic_notify_next(client, notify_next_topic);
	if (err) {
		LOG_ERR("mqtt_subscribe with topic notify-next failed");
		return err;
	}

	err = aws_jobs_subscribe_topic_get(client, "$next", get_topic);
	if (err) {
		LOG_ERR("mqtt_subscribe with topic jobs/$next/get failed");
		return err;
	}

	return 0;
}

static int suback_notify_next(struct mqtt_client *const client)
{
	LOG_DBG("Subscribed to notify-next topic");
	int err = aws_jobs_get_job_execution(client, "$next", get_topic);

	if (err) {
		return err;
	}

	return 0;
}

static int suback_job_id_update(struct mqtt_client *const client)
{
	LOG_DBG("Subscribed to jobs/%s/update update job document version:%d" 
		log_strdup(job_id), doc_version_number);
	int err = 0;
	if (fota_state == DOWNLOAD_FIRMWARE) {
		err = update_job_execution(client, job_id, AWS_JOBS_IN_PROGRESS,
					   fota_state, doc_version_number, "");
	}

	if (err) {
		return err;
	}

	return 0;
}

int aws_fota_mqtt_evt_handler(struct mqtt_client *const client,
			      const struct mqtt_evt *evt)
{
	int err;

	switch (evt->type) {
	case MQTT_EVT_CONNACK:
		if (evt->result != 0) {
			return evt->result;
		}

		return connack_evt(client);
		/* This expects that the application's mqtt handler will handle
		 * any situations where you could not connect to the MQTT
		 * broker.
		 */

	case MQTT_EVT_DISCONNECT:
		//TODO: should we disconnect or just abort mqtt_disconnect(client);
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
		return 0;

	} break;

	case MQTT_EVT_PUBACK:
		if (evt->result != 0) {
			LOG_ERR("MQTT PUBACK error %d", evt->result);
			return 0;
		}
		//TODO: Add PUBACK check for publish
		return 1;

	case MQTT_EVT_SUBACK:
		if (evt->result != 0) {
			return 0;
		}

		if (evt->param.suback.message_id == SUBSCRIBE_NOTIFY_NEXT) {
			suback_notify_next(client);
		}

		if (evt->param.suback.message_id == SUBSCRIBE_GET) {
			LOG_INF("subscribed to get topic");
			return 0;
		}

		if ( && (evt->param.suback.message_id == SUBSCRIBE_JOB_ID_UPDATE)) {
			suback_job_id_update(client);
			return 0;
		}

		return 1;

	}
	return 1;
}

static void http_fota_handler(enum fota_download_evt_id evt)
{
	__ASSERT_NO_MSG(c != NULL);

	int err = 0;

	switch (evt) {
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
	}

}

#define FILE_JOB_ID "job_id"
#define FILE_FOTA_STATE "fota_state" 
#define FILE_DOC_VER "doc_version_number"
int aws_fota_init(struct mqtt_client *const client, fs_mount_t *mount_point
		  aws_fota_callback_t evt_handler)
{
	int err;

	if (client == NULL || evt_handler == NULL || mount_point == NULL) {
		return -EINVAL;
	}
	
	mp = mount_point;
	/* TODO: Append mnt point prefix */
	//err = snprintf(dir, "%s/aws_iot_jobs", mount_point.mnt_point);
	
	/* Store mount point to make it available in event handlers. */
	mp.mnt_point = "/aws_iot_jobs";
	/* Store client to make it available in event handlers. */
	c = client;
	callback = evt_handler;

	err = fota_download_init(http_fota_handler);
	if (err != 0) {
		LOG_ERR("fota_download_init error %d", err);
		return err;
	}
	
	err = fs_mount(&mp);
	if (err < 0) {
		LOG_ERR("Error mounting file system [err: %d, mnt_point: %s, "
			"type: %d]", err, mp.mnt_point, mp.type);
		return -EFAULT;
	}

	fs_file_t file_handler;
	fs_open(file_handler, FILE_JOB_ID);
	fs_read(FILE_JOB_ID, &job_id, sizeof(job_id));
	fs_close(FILE_JOB_ID);
	LOG_INF("Previously stored job_id %s", job_id);

	fs_open(file_handler, FILE_DOC_VER);
	fs_read(FILE_DOC_VER, &doc_version_number, sizeof(doc_version_number));
	fs_close(FILE_DOC_VER);
	
	fs_open(file_handler, FILE_FOTA_STATE);
	fs_read(FILE_FOTA_STATE, &doc_version_number, sizeof(doc_version_number));
	fs_close(FILE_DOC_VER);

	return 0;
}
