/* Enum to keep the fota status */
enum fota_status {
	NONE = 0,
	DOWNLOAD_FIRMWARE,
	APPLY_FIRMWARE,
};

/* Map of fota status to report back */
const char * fota_status_strings [] = {
	[NONE] = "none",
	[DOWNLOAD_FIRMWARE] = "download_firmware",
	[APPLY_FIRMWARE] = "apply_update"
};


/** @brief Initialize the AWS jobs handler. 
 *
 *  @param[in] c	The mqtt client instance.
 *
 *  @retval 0 If initialzed successfully.
 */
int aws_jobs_init(struct mqtt_client c);

/**@brief Handeling of AWS jobs topics and states
 *
 *  @retval 0 If the event was handled successfully.
 */
int aws_jobs_process(u8_t * topic, u8_t * json_payload);

/**@brief Set the state of the mqtt connection 
 *  @param[in] client	The mqtt client instance.
 */
int aws_jobs_connected(bool state);
#include "aws_jobs.h"
/* Global local counter which contains the expected job document version,
 * this needs to match what the AWS jobs document contains if not
 * the update using that expectedVersionNumber will be rejected
 * https://docs.aws.amazon.com/iot/latest/developerguide/jobs-api.html#mqtt-updatejobexecution
 */
static int expected_document_version_number = 0;

/*
 * Jobdocument structure
 * {
 *	operation: <json obj> (currently only app_fw_update)
 *	fwversion: string
 *	size: number
 *	location: string
 * }
 */

/* Simple state flow overview
 * Queue job
 * device notfied of job
 * device update job with in progress
 * device initiates download from http
 * update fota status with download firmware
 * firmware downloaded
 * update fota status with apply firmware
 * update with success
 * reboot
 */
#define IMEI_LEN 15
#define CLIENT_ID_LEN (IMEI_LEN + 4)
static u8_t notify_next_topic[JOBS_NOTIFY_NEXT_TOPIC_LEN + 1] = { 0 };
/* Staticly allocated buffers for keeping the topics needed for AWS jobs */
static u8_t get_jobid_updated_accepted_topic[JOBS_GET_JOBID_TOPIC_MAX_LEN+ 1];
static u8_t get_jobid_updated_rejected_topic[JOBS_GET_JOBID_TOPIC_MAX_LEN+ 1];

static u8_t hostname[CONFIG_MQTT_MESSAGE_BUFFER_SIZE];
static u8_t file_path[CONFIG_MQTT_MESSAGE_BUFFER_SIZE];

/* 
 * Dispatcher for handling job updates the FSM state 
 */
static void aws_jobs_update_handler(struct mqtt_client * c,
				    u8_t * topic,
				    u8_t * json_payload)
{
}

/* Enum for tracking the job exectuion state */
static enum execution_status execution_state;

/* Topic for updating shadow topic with version number */
#define UPDATE_DELTA_TOPIC AWS "%s/shadow/update"
#define UPDATE_DELTA_TOPIC_LEN (AWS_LEN + CONFIG_CLIENT_ID_LEN + 14)
#define SHADOW_STATE_UPDATE "{\"state\":{\"reported\":{\"nrfcloud__fota_v1__app_v\":%s}}}"
static int publish_version_to_device_shadow(struct mqtt_client *c,
					    const char * app_version)
{
	char update_delta_topic[UPDATE_DELTA_TOPIC_LEN + 1];
	u8_t data[CONFIG_MQTT_MESSAGE_BUFFER_SIZE];

	struct mqtt_publish_param param;

	int ret = snprintf(update_delta_topic, sizeof(update_delta_topic),
		       UPDATE_DELTA_TOPIC, c->client_id.utf8);

	if (ret != UPDATE_DELTA_TOPIC_LEN) {
		return -ENOMEM;
	}

	snprintf(data, ARRAY_SIZE(data), SHADOW_STATE_UPDATE, app_version);

	param.message.topic.qos = 1;
	param.message.topic.topic.utf8 = update_delta_topic;
	param.message.topic.topic.size = UPDATE_DELTA_TOPIC_LEN;
	param.message.payload.data = data;
	param.message.payload.len = strlen(data);
	param.message_id = sys_rand32_get();
	param.dup_flag = 0;
	param.retain_flag = 0;

	return mqtt_publish(c, &param);
}

int aws_fota_init(struct mqtt_client *c, char * current_version)
{
	int err = publish_version_to_device_shadow(client,"v1.0.0");
}
/* Job execution document
 * {
 * 	jobId: string
 * 	status:string<job_execution_status>
 * 	statusDetails: {
 * 		//nrfcloud specific
 * 		nextState: string<FotaStatus> 
 * 	}
 * 	queuedAt: number<time>
 *
 */

void aws_jobs_handler(struct mqtt_client * c,
			     u8_t * topic,
			     u8_t * json_payload)
{
	printk("Entered aws_jobs hander with topic: %s\n\r paylod: %s", topic, json_payload);
	if (!strncmp(jobs_notify_next_topic, topic, strlen(topic)))
	{	
		//parse_job_document(json_payload);
		//subscribe_to_jobid_topics(job_id);
		//expected_document_version_number = accept_job(job_id);
		//unsubscribe_notify_next_topic(c);
	}
	else if (!strncmp(jobs_get_jobid_updated_accepted_topic,
			  topic,
			  strlen(topic)
			  )
		)
	{
		if (execution_state == IN_PROGRESS)
		{
		}
	}
	else
	{
	}
}

/* Topics which needs to be subscribed to for getting notfied of a job and updates
 * https://docs.aws.amazon.com/iot/latest/developerguide/jobs-devices.html */
int subscribe_ex_topics(char * client_id)
{
	/* We subscribe to both accepted and rejected so we do get/# to simplify our code */
	/* Generate subscription list, if you update this update the list_count */
	struct mqtt_topic subscribe_topic []= {
		{
			.topic = {
				.utf8 = jobs_get_topic,
				.size = JOBS_GET_TOPIC_LEN 
			},
			.qos = MQTT_QOS_1_AT_LEAST_ONCE
		},
		{
			.topic = {
				.utf8 = jobs_notify_next_topic,
				.size = JOBS_NOTIFY_NEXT_TOPIC_LEN 
			},
			.qos = MQTT_QOS_1_AT_LEAST_ONCE
		},
	};

	const struct mqtt_subscription_list subscription_list = {
		.list = (struct mqtt_topic *)&subscribe_topic,
		.list_count = 2, //TODO: Find out if ARRAY_SIZE(subscribe_topic) works?
		.message_id = 111  //TODO: Find a good message_id number?
	};
	
	return mqtt_subscribe(client, &subscription_list);
}

int aws_jobs_init(struct mqtt_client *client)
{
	if (err) {
		return -1; //ERR
	}
	err = construct_notify_next_topic(client);
	if (err) {
		return -1; //ERR
	}
	err = subscribe_to_jobs_notify_next(client);
	if (err) {
		return -1; //ERR
	}

}

