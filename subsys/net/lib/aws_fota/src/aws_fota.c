#include <zephyr.h>
#include <stdio.h>
#include <json.h>
#include <net/fota_download.h>
#include <net/aws_jobs.h>
#include <net/aws_fota.h>
#include <logging/log.h>

#include "aws_fota_internal.h"

LOG_MODULE_REGISTER(aws_jobs_fota, CONFIG_AWS_JOBS_FOTA_LOG_LEVEL);

static const struct json_obj_descr location_obj_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct location_obj, protocol, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct location_obj, host, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct location_obj, path, JSON_TOK_STRING),
};


static const struct json_obj_descr job_document_obj_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct job_document_obj,
			    operation,
			    JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM_NAMED(struct job_document_obj,
				  "fwversion",
				  fw_version,
				  JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct job_document_obj, size, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_OBJECT(struct job_document_obj,
			      location,
			      location_obj_descr),

};

static const struct json_obj_descr execution_obj_descr[] = {
	JSON_OBJ_DESCR_PRIM_NAMED(struct execution_obj,
				  "jobId",
				  job_id,
				  JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct execution_obj, status, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM_NAMED(struct execution_obj,
				  "queuedAt",
				  queued_at,
				  JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM_NAMED(struct execution_obj,
				  "lastUpdatedAt",
				  last_update_at,
				  JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM_NAMED(struct execution_obj,
				  "versionNumber",
				  version_number,
				  JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM_NAMED(struct execution_obj,
				  "executionNumber",
				  execution_number,
				  JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_OBJECT_NAMED(struct execution_obj,
				    "jobDocument",
				    job_document,
				    job_document_obj_descr),
};

static const struct json_obj_descr notify_next_obj_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct notify_next_obj, timestamp, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_OBJECT(struct notify_next_obj,
			      execution,
			      execution_obj_descr),
};

/* Enum for tracking the job exectuion state */
static enum execution_status execution_state = QUEUED;
/* Buffer for reporting the current application version */
static char version[CONFIG_VERSION_STRING_MAX_LEN + 1];

/* Allocated strings for checking topics */
static u8_t notify_next_topic[NOTIFY_NEXT_TOPIC_MAX_LEN + 1];
static u8_t job_id_update_accepted_topic[JOB_ID_UPDATE_TOPIC_MAX_LEN + 1];
static u8_t job_id_update_rejected_topic[JOB_ID_UPDATE_TOPIC_MAX_LEN + 1];
static u8_t payload_buf[CONFIG_AWS_IOT_JOBS_MESSAGE_SIZE];

static int construct_job_id_update_topic(const u8_t *client_id,
			   const u8_t *job_id,
			   const u8_t *suffix,
			   u8_t *topic_buf)
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
			"memory for job_id_update_topic");
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
#define SHADOW_STATE_UPDATE "{\"state\":{\"reported\":{\"nrfcloud__fota_v1__app_v\":%s}}}"
static int update_device_shadow_version(struct mqtt_client *const client)
{
	struct mqtt_publish_param param;
	char update_delta_topic[UPDATE_DELTA_TOPIC_LEN + 1];
	u8_t shadow_update_payload[CONFIG_DEVICE_SHADOW_PAYLOAD_SIZE];

	int ret = snprintf(update_delta_topic,
			   sizeof(update_delta_topic),
			   UPDATE_DELTA_TOPIC,
			   client->client_id.utf8);
	u32_t update_delta_topic_len = ret + 1;

	if (ret >= UPDATE_DELTA_TOPIC_LEN) {
		return -ENOMEM;
	} else if (ret < 0) {
		return ret;
	}

	ret = snprintf(shadow_update_payload,
		       CONFIG_DEVICE_SHADOW_PAYLOAD_SIZE,
		       SHADOW_STATE_UPDATE,
		       version);
	u32_t shadow_update_payload_len = ret + 1;

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

/**
 * Return 0 for events which should be further processed by the client
 * Return >= 1 for events which can be skipped/events handled by the
 * library.
 */
int aws_fota_mqtt_evt_handler(struct mqtt_client *const client,
			      const struct mqtt_evt *evt)
{
	int err;
	switch (evt->type) {
	case MQTT_EVT_CONNACK:
		if (evt->result != 0) {
			return 0;
		}

		err = construct_notify_next_topic(client->client_id.utf8,
						  notify_next_topic);
		if (err) {
			return err;
		}

		err = aws_jobs_subscribe_expected_topics(client, true);
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
		//get_payload
		err = aws_fota_on_publish_evt(client,
					      topic,
					      topic_len,
					      payload_buf);

	} break;

	case MQTT_EVT_PUBACK:
		if (evt->result != 0) {
			LOG_ERR("MQTT PUBACK error %d\n", evt->result);
			return 0;
		}
		/* check evt->param.puback.message_id */
		/* We expect that the client*/
		break;

	case MQTT_EVT_SUBACK:
		if (evt->result != 0) {
			break;
		}

		/* check evt->param.suback.message_id against SUB_BASE */
		break;

	default:
		/* Handling for default case? */
		break;
	}
}

#define ERR_CHECK(err)\
	do {\
		if (err) {\
			return err;\
		}\
	} while(0)

static int aws_fota_on_publish_evt(struct mqtt_client *const client,
				   const u8_t * topic,
				   u32_t topic_len,
				   const u8_t * json_payload)
{
	int err;
	char job_id[JOB_ID_MAX_LEN];

	/* If not processign job */
	/* Reciving an publish on notify_next_topic could be a job */
	if (!strncmp(notify_next_topic,
		    topic,
		    MIN(NOTIFY_NEXT_TOPIC_MAX_LEN, topic_len)
		    )
	    ) {
		/*
		 * Check if the current message recived on notify-next is a
		 * job.
		 */
		err = parse_notify_next_topic(json_payload,
					      job_id,
					      hostname,
					      file_path);
		ERR_CHECK(err);
		/* Unsubscribe from notify_next_topic to not recive more jobs
		 * while processing the current job.
		 */
		err = aws_jobs_unsubscribe_expected_topics(client, true);
		ERR_CHECK(err);
		/* Subscribe to update topic to recive feedback on wether an
		 * update is accepted or not.
		 */
		err = aws_jobs_subscribe_job_id_update(client, job_id);
		ERR_CHECK(err);

		err = construct_job_id_update_topic(client->client_id.utf8,
					      job_id,
					      "/accepted",
					      job_id_update_accepted_topic);
		ERR_CHECK(err);
		err = construct_job_id_update_topic(client->client_id.utf8,
					      job_id,
					      "/rejected",
					      job_id_update_rejected_topic);

		fota_status = DOWNLOAD_FIRMWARE;
		char status_details [] = "{\"nextState\":\"download_firmware\"}"
		aws_jobs_update_job_execution(client,
					 job_id,
					 IN_PROGRESS,
					 status_details,
					 expected_version,
					 /* Client token are not used by this
					  * library*/
					 "");

		/* Handled by the library*/
		return 1;

	} else if (!strncmp(job_id_update_accepted_topic,
			    topic,
			    MIN(JOB_ID_UPDATE_TOPIC_MAX_LEN, topic_len))) {
		err = parse_accepted_topic_payload(json_payload, status);
		/* Set state to IN_PROGRES */
		execution_state = status;
		if (execution_state == IN_PROGRESS &&
		    fota_state == DOWNLOAD_FIRMWARE) {
			fota_download_start(hostname, file_path);
		}
		else if (execution_state == IN_PROGRESS &&
		    fota_state == APPLY_FIRMWARE) {
			//clean up and report status
			fota_state = NONE;
			aws_jobs_update_job_execution(client,
						      job_id,
						      SUCCEEDED,
						      status_details,
						      expected_version,
						      "");
		}
		else if (execution_state == SUCCEEDED &&
		    fota_state == APPLY_FIRMWARE) {
			callback_emit(AWS_FOTA_EVT_FINISHED);
		}
	}

}

/* Pointer to initialized MQTT client instance */
// TODO: A better awy of doing this?
static struct mqtt_client * c;

fota_download_callback_t http_fota_handler(fota_download_evt_id evt)
{
	__ASSERT_NO_MSG(c != NULL);

	switch(evt) {
		case FOTA_DOWNLOAD_EVT_FINISHED:
			fota_state = APPLY_FIRMWARE;

			aws_jobs_update_job_execution(c,
						      job_id,
						      IN_PROGRESS,
						      status_details,
						      expected_version,
						      "");
			break;
		case FOTA_DOWNLOAD_EVT_ERROR:
			aws_jobs_update_job_execution(c,
						      job_id,
						      FAILED,
						      status_details,
						      expected_version,
						      "");
			break;
	}

}

static int construct_notify_next_topic(const u8_t * client_id, u8_t * topic_buf)
{
	__ASSERT_NO_MSG(client_id != NULL);
	__ASSERT_NO_MSG(topic_buf != NULL);

	int ret = snprintf(topic_buf,
			   NOTIFY_NEXT_TOPIC_MAX_LEN,
			   NOTIFY_NEXT_TOPIC_TEMPLATE,
			   client_id);
	if (ret >= size) {
		LOG_ERR("Unable to fit formated string into to allocate "
			"memory for notify_next_topic");
		return -ENOMEM;
	} else if (ret < 0) {
		LOG_ERR("Formatting error for notify_next_topic");
			"%d", entry, ret);
		return ret;
	}
	return 0;
}



/**@brief Initialize the AWS Firmware Over the Air library.
 *
 * @param app_version Current version number of the application as a \0
 *	terminated ASCII string.
 * @param callback Callback for events generated.
 *
 * @retval 0 If successfully initialized.
 * @retval -EINVAL If any of the input values are invalid.
 *           Otherwise, a negative value is returned.
 */
int aws_fota_init(struct mqtt_client *const client,
		  const char *app_version,
		  aws_fota_callback_t cb)
{
	if (client ==NULL || app_version == NULL || cb == NULL) {
		return -EINVAL;

	}

	if (CONFIG_AWS_IOT_JOBS_MESSAGE_SIZE > client->rx_buf_size) {
		LOG_ERR("The expected message size is larger than the "
			"allocated rx_buffer in the MQTT client");
		return -EMSGSIZE;
	}
	if (CONFIG_DEVICE_SHADOW_PAYLOAD_SIZE > client->tx_buf_size) {
		LOG_ERR("The expected update_payload size is larger than the"
			"allocated tx_buffer");
		return -EMSGSIZE;
	}

	c = client;
	callback = cb;
	int err = fota_download_init(http_fota_handler);
	if (err != 0) {
		LOG_ERR("fota_download_init error %d", err);
		return err;
	}

	memcpy(version,
	       app_version,
	       MIN(strlen(app_version), CONFIG_VERSION_STRING_MAX_LEN));

	return 0;
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

#define STATUS_DETAILS_TEMPLATE "{\"nextState\":\"%s\"}"
#define STATUS_DETAILS_MAX_LEN  ((sizeof("{\"nextState\":\"\"}") - 1) +\
				(sizeof(fota_status_strings[DOWNLOAD_FIRMWARE]\
				) - 1))

static int update_job_execution(struct mqtt_client *const client,
				const u8_t * job_id,
				enum execution_status state,
				enum fota_status next_state,
				int version_number,
				const char * client_token)
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
					      IN_PROGRESS,
					      status_details,
					      version_number,
					      client_token);
		return ret;
}

/*
static int parse_job_document(const u8_t *job_document)
{
	struct notif_next_obj job;
	int ret = json_obj_parse(job_document,
				 ARRAY_SIZE(job_document),
				 notify_next_obj_descr,
				 &job);
	if (ret < 0) {
		return -EFAULT;
	}
	printk("jobs status: %s", job.execution.status);
	return 0;
}
*/
