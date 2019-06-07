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

LOG_MODULE_REGISTER(aws_fota, CONFIG_AWS_JOBS_FOTA_LOG_LEVEL);

/* Enum to keep the fota status */
enum fota_status {
	NONE = 0,
	DOWNLOAD_FIRMWARE,
	APPLY_FIRMWARE,
};

/* Map of fota status to report back */
static const char * const fota_status_strings[] = {
	[DOWNLOAD_FIRMWARE] = "download_firmware",
	[APPLY_FIRMWARE] = "apply_update",
	[NONE] = "none",
};

/* Pointer to initialized MQTT client instance */
static struct mqtt_client *c;

/* Enum for tracking the job exectuion state */
static enum execution_status execution_state = AWS_JOBS_QUEUED;
static enum fota_status fota_state = NONE;
static u32_t doc_version_number = 1;

/* Buffer for reporting the current application version */
static char version[CONFIG_VERSION_STRING_MAX_LEN + 1];

/* Allocated strings for checking topics */
static u8_t notify_next_topic[NOTIFY_NEXT_TOPIC_MAX_LEN + 1];
static u8_t job_id_update_accepted_topic[JOB_ID_UPDATE_TOPIC_MAX_LEN + 1];
static u8_t job_id_update_rejected_topic[JOB_ID_UPDATE_TOPIC_MAX_LEN + 1];
/* Allocated buffers for keeping hostname, json payload and file_path */
static u8_t payload_buf[CONFIG_AWS_FOTA_PAYLOAD_SIZE + 1];
static u8_t hostname[CONFIG_AWS_FOTA_HOSTNAME_MAX_LEN + 1];
static u8_t file_path[CONFIG_AWS_FOTA_FILE_PATH_MAX_LEN + 1];
static u8_t job_id[JOB_ID_MAX_LEN + 1];
static aws_fota_callback_t callback;

/**@brief Find key corresponding to val in map.
 */
static int val_to_key(const char **map, size_t num_keys,
		const char *val, size_t val_len)
{
	for (int i = 0; i < num_keys; ++i) {
		if (val_len != strlen(map[i])) {
			continue;
		} else if (strncmp(val, map[i], val_len) == 0) {
			return i;
		}
	}
	return -1;
}

/**@brief Function to read the published payload.
 */
static int publish_get_payload(struct mqtt_client *c, size_t length)
{
	u8_t *buf = payload_buf;
	u8_t *end = buf + length;

	if (length > sizeof(payload_buf)) {
		return -EMSGSIZE;
	}
	while (buf < end) {
		int ret = mqtt_read_publish_payload_blocking(c, buf, end - buf);

		if (ret < 0 && ret != -EAGAIN) {
			return ret;
		} else if (ret == 0) {
			return -EIO;
		}
		buf += ret;
	}
	return 0;
}

static int construct_notify_next_topic(const u8_t *client_id, u8_t *topic_buf)
{
	__ASSERT_NO_MSG(client_id != NULL);
	__ASSERT_NO_MSG(topic_buf != NULL);

	int ret = snprintf(topic_buf,
			   NOTIFY_NEXT_TOPIC_MAX_LEN,
			   NOTIFY_NEXT_TOPIC_TEMPLATE,
			   client_id);
	if (ret >= NOTIFY_NEXT_TOPIC_MAX_LEN) {
		LOG_ERR("Unable to fit formated string into to allocate "
			"memory for notify_next_topic");
		return -ENOMEM;
	} else if (ret < 0) {
		LOG_ERR("Formatting error for notify_next_topic %d", ret);
		return ret;
	}
	return 0;
}

static int construct_job_id_update_topic(const u8_t *client_id,
		const u8_t *job_id, const u8_t *suffix, u8_t *topic_buf)
{
	__ASSERT_NO_MSG(client_id != NULL);
	__ASSERT_NO_MSG(job_id != NULL);
	__ASSERT_NO_MSG(suffix != NULL);
	__ASSERT_NO_MSG(topic_buf != NULL);

	int ret = snprintf(topic_buf,
			   JOB_ID_UPDATE_TOPIC_MAX_LEN,
			   JOB_ID_UPDATE_TOPIC_TEMPLATE,
			   client_id,
			   job_id,
			   suffix);
	if (ret >= JOB_ID_UPDATE_TOPIC_MAX_LEN) {
		LOG_ERR("Unable to fit formated string into to allocate "
			"memory for construct_job_id_update_topic");
		return -ENOMEM;
	} else if (ret < 0) {
		LOG_ERR("Formatting error for job_id_update topic: %d", ret);
		return ret;
	}
	return 0;
}

/* Topic for updating shadow topic with version number */
#define UPDATE_DELTA_TOPIC AWS "%s/shadow/update"
#define UPDATE_DELTA_TOPIC_LEN (AWS_LEN +\
				CONFIG_CLIENT_ID_MAX_LEN +\
				(sizeof("/shadow/update") - 1))
#define SHADOW_STATE_UPDATE \
	"{\"state\":{\"reported\":{\"nrfcloud__fota_v1__app_v\":\"%s\"}}}"

static int update_device_shadow_version(struct mqtt_client *const client)
{
	struct mqtt_publish_param param;
	char update_delta_topic[UPDATE_DELTA_TOPIC_LEN + 1];
	u8_t shadow_update_payload[CONFIG_DEVICE_SHADOW_PAYLOAD_SIZE];

	int ret = snprintf(update_delta_topic,
			   sizeof(update_delta_topic),
			   UPDATE_DELTA_TOPIC,
			   client->client_id.utf8);
	u32_t update_delta_topic_len = ret;

	if (ret >= UPDATE_DELTA_TOPIC_LEN) {
		return -ENOMEM;
	} else if (ret < 0) {
		return ret;
	}

	ret = snprintf(shadow_update_payload,
		       CONFIG_DEVICE_SHADOW_PAYLOAD_SIZE,
		       SHADOW_STATE_UPDATE,
		       version);
	u32_t shadow_update_payload_len = ret;

	if (ret >= UPDATE_DELTA_TOPIC_LEN) {
		return -ENOMEM;
	} else if (ret < 0) {
		return ret;
	}

	param.message.topic.qos = MQTT_QOS_1_AT_LEAST_ONCE;
	param.message.topic.topic.utf8 = update_delta_topic;
	param.message.topic.topic.size = update_delta_topic_len;
	param.message.payload.data = shadow_update_payload;
	param.message.payload.len = shadow_update_payload_len;
	param.message_id = sys_rand32_get();
	param.dup_flag = 0;
	param.retain_flag = 0;

	return mqtt_publish(client, &param);
}

#define STATUS_DETAILS_TEMPLATE "{\"nextState\":\"%s\"}"
/*
 * #define STATUS_DETAILS_MAX_LEN  ((sizeof("{\"nextState\":\"\"}") - 1) +\
 *  (sizeof("download_firmware") - 1))
 */
#define STATUS_DETAILS_MAX_LEN 255

static int update_job_execution(struct mqtt_client *const client,
				const u8_t *job_id,
				enum execution_status state,
				enum fota_status next_state,
				int version_number,
				const char *client_token)
{
		char status_details[STATUS_DETAILS_MAX_LEN + 1];
		int ret = snprintf(status_details,
				   STATUS_DETAILS_MAX_LEN,
				   STATUS_DETAILS_TEMPLATE,
				   fota_status_strings[next_state]);
		if (ret >= STATUS_DETAILS_MAX_LEN) {
			return -ENOMEM;
		} else if (ret < 0) {
			return ret;
		}

		ret =  aws_jobs_update_job_execution(client,
					      job_id,
					      AWS_JOBS_IN_PROGRESS,
					      status_details,
					      version_number,
					      client_token);

		if (ret < 0) {
			LOG_ERR("aws_jobs_update_job_execution failed: %d",
				ret);
		}

		return ret;
}


static int aws_fota_on_publish_evt(struct mqtt_client *const client,
				   const u8_t *topic,
				   u32_t topic_len,
				   u8_t *json_payload,
				   u32_t payload_len)
{
	printk("fota_published_to_sub_topic_evt handler\n");
	int err;

	/* If not processign job */
	/* Reciving an publish on notify_next_topic could be a job */
	if (!strncmp(notify_next_topic, topic,
		    MIN(NOTIFY_NEXT_TOPIC_MAX_LEN, topic_len))) {
		err = publish_get_payload(client, payload_len);
		if (err) {
			return err;
		}
		/*
		 * Check if the current message recived on notify-next is a
		 * job.
		 */
		printk("Parse document: %s", json_payload);
		err = aws_fota_parse_notify_next_document(json_payload,
							  payload_len, job_id,
							  hostname, file_path);
		if (err < 0) {
			printk("Error when parsing the json %d\n", err);
			return err;
		}

		/* Unsubscribe from notify_next_topic to not recive more jobs
		 * while processing the current job.
		 */
		/* err = aws_jobs_unsubscribe_expected_topics(client, true); */
		err = aws_jobs_unsubscribe_notify_next(client);
		if (err) {
			printk("Error when unsubscribing notify_next_topic:"
				"%d\n", err);
			return err;
		}

		/* Subscribe to update topic to recive feedback on wether an
		 * update is accepted or not.
		 */
		err = aws_jobs_subscribe_job_id_update(client, job_id);
		if (err) {
			printk("Error when subscribing job_id_update:"
			       "%d\n", err);
			return err;
		}

		/* Construct job_id topics to be used for filtering publish
		 * messages.
		 */
		err = construct_job_id_update_topic(client->client_id.utf8,
			job_id, "/accepted", job_id_update_accepted_topic);
		if (err) {
			printk("Error when constructing_job_id_update_accepted:"
				"%d\n", err);
			return err;
		}

		err = construct_job_id_update_topic(client->client_id.utf8,
			job_id, "/rejected", job_id_update_rejected_topic);
		if (err) {
			printk("Error when constructing_job_id_update_rejected:"
			       "%d\n", err);
			return err;
		}

		/* Set fota_state to DOWNLOAD_FIRMWARE, when we are subscribed
		 * to job_id topics we will try to publish and if accepted we
		 * can start the download
		 */
		fota_state = DOWNLOAD_FIRMWARE;

		/* Handled by the library*/
		return 1;

	} else if (!strncmp(job_id_update_accepted_topic, topic,
			    MIN(JOB_ID_UPDATE_TOPIC_MAX_LEN, topic_len))) {
		err = publish_get_payload(client, payload_len);
		if (err) {
			return err;
		}
		/*
		 * err = parse_accepted_topic_payload(json_payload, status,
		 * &doc_version_number);
		 * Set state to what was in the response payload
		 * execution_state = status;
		 */

		if (fota_state == DOWNLOAD_FIRMWARE) {
			/* TODO: Get this state from the update document */
			execution_state = AWS_JOBS_IN_PROGRESS;
			printk("Download firmware");
			err = fota_download_start(hostname, file_path);
			if (err) {
				printk("Error when trying to start firmware"
				       "download");
				return err;
			}
		} else if (execution_state == AWS_JOBS_IN_PROGRESS &&
		    fota_state == APPLY_FIRMWARE) {
			/* clean up and report status
			 * Maybe keep applying firmware as we haven't rebooted
			 */
			fota_state = NONE;
			update_job_execution(client, job_id, AWS_JOBS_SUCCEEDED,
					fota_state, doc_version_number, "");
		} else if (execution_state == AWS_JOBS_SUCCEEDED &&
		    fota_state == APPLY_FIRMWARE) {
			/*TODO: callback_emit(AWS_FOTA_EVT_FINISHED); */
		}
		return 1;
	} else if (!strncmp(job_id_update_rejected_topic, topic,
			    MIN(JOB_ID_UPDATE_TOPIC_MAX_LEN, topic_len))) {
		printk("Update was rejected\n");
		/*TODO: emit aws_fota failure our
		 * update to the job document was rejected
		 */
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
			return 0;
		}

		err = aws_jobs_subscribe_notify_next(client);
		if (err) {
			return err;
		}

		err = update_device_shadow_version(client);
		if (err) {
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
					      payload_buf,
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
		/* check evt->param.puback.message_id */
		/* We expect that the client pubacks*/
		return 0;

	case MQTT_EVT_SUBACK:
		if (evt->result != 0) {
			return 0;
		}
		if ((fota_state == DOWNLOAD_FIRMWARE) &&
		   (evt->param.suback.message_id == SUBSCRIBE_JOB_ID_UPDATE)) {
			/* Client token are not used by this library */
			err = update_job_execution(client, job_id,
						   AWS_JOBS_IN_PROGRESS,
						   fota_state,
						   doc_version_number,
						   "");
			if (err) {
				printk("Error when updating "
				       "job_execution_status : %d\n", err);
			return err;
			}
		}

		return 0;

	default:
		/* Handling for default case? */
		return 0;
	}
	return 0;
}

static void http_fota_handler(enum fota_download_evt_id evt)
{
	__ASSERT_NO_MSG(c != NULL);

	switch (evt) {
	case FOTA_DOWNLOAD_EVT_FINISHED:
		fota_state = APPLY_FIRMWARE;
		update_job_execution(c, job_id, AWS_JOBS_SUCCEEDED,
				     fota_state, doc_version_number, "");
		/* TODO: Emit AWS_FOTA_DONE when update_job_execution is done*/

		break;
	case FOTA_DOWNLOAD_EVT_ERROR:
		/*
		update_job_execution(c, job_id, AWS_JOBS_FAILED, fota_state,
				     doc_version_number, "");
				     */
		printk("Download error\n");
		/* TODO: Emit AWS_FOTA_ERR */
		break;
	}

}

int aws_fota_init(struct mqtt_client *const client,
		  const char *app_version,
		  aws_fota_callback_t cb)
{
	int err;

	if (client == NULL || app_version == NULL || cb == NULL) {
		return -EINVAL;
	} else if (CONFIG_AWS_FOTA_PAYLOAD_SIZE > client->rx_buf_size) {
		LOG_ERR("The expected message size is larger than the "
			"allocated rx_buffer in the MQTT client");
		return -EMSGSIZE;
	} else if (client->tx_buf_size < CONFIG_DEVICE_SHADOW_PAYLOAD_SIZE) {
		LOG_ERR("The expected update_payload size is larger than the"
			"allocated tx_buffer");
		return -EMSGSIZE;
	}

	/* Client is only used to make the MQTT client available from the
	 * http_fota_handler.
	 */
	c = client;
	callback = cb;

	err = construct_notify_next_topic(client->client_id.utf8,
					  notify_next_topic);
	if (err) {
		LOG_ERR("construct_notify_next_topic error %d", err);
		return err;
	}

	err = fota_download_init(http_fota_handler);
	if (err != 0) {
		LOG_ERR("fota_download_init error %d", err);
		return err;
	}

	memcpy(version,
	       app_version,
	       MIN(strlen(app_version), CONFIG_VERSION_STRING_MAX_LEN));

	return 0;
}
