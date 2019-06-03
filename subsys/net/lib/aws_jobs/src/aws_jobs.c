#include <zephyr.h>
#include <stdio.h>
#include <net/mqtt.h>
#include <cJSON.h>
#include <aws_jobs.h>
#include <logging/log.h>

LOG_MODULE_REGISTER(aws_jobs, CONFIG_AWS_JOBS_LOG_LEVEL);


/* The max JOB_ID_LEN according to AWS docs
 * https://docs.aws.amazon.com/general/latest/gr/aws_service_limits.html#job-limits
 */
#define JOB_ID_MAX_LEN 64
#define STATUS_MAX_LEN 12

#define AWS_JOBS_SUB_BASE 2110
#define SUB_NOTIFIY_NEXT (AWS_JOBS_SUB_BASE + 0)
#define SUB_UPDATE (AWS_JOBS_SUB_BASE + 1)

#define AWS "$aws/things/"
#define AWS_LEN (sizeof(AWS) - 1)

#define NOTIFY_NEXT_TOPIC AWS "%s/jobs/notify-next"
#define NOTIFY_NEXT_TOPIC_LEN (AWS_LEN + CONFIG_CLIENT_ID_LEN + 18)
#define NOTIFY_TOPIC AWS "%s/jobs/notify"
#define NOTIFY_TOPIC_LEN (AWS_LEN + CONFIG_CLIENT_ID_LEN + 12)

#define GET_TOPIC AWS "%s/jobs/get/#"
#define GET_TOPIC_LEN (AWS_LEN + CONFIG_CLIENT_ID_LEN + 11)

#define GET_JOBID_TOPIC AWS "%s/jobs/%s/get/#"
#define GET_JOBID_TOPIC_MAX_LEN (AWS_LEN +\
				 CONFIG_CLIENT_ID_LEN +\
				 JOB_ID_MAX_LEN +\
				 12)

#define UPDATE_TOPIC AWS "%s/jobs/%s/update/%s"
#define UPDATE_TOPIC_LEN (AWS_LEN + CONFIG_CLIENT_ID_LEN + JOB_ID_MAX_LEN + 14)
/* Job execution update document
 * {
 * 	jobId: string
 * 	status:string<job_execution_status>
 * 	queuedAt: number<time>
 * 	lastUpdateAt: number<time>
 * 	versionNumber: number<document version number>
 * 	executionNumber: number<num of executions>
 * 	jobsDocument: JSON<job document>
 *
 */


/* Job execution update document
 * {
 * 	jobId: string
 * 	status:string<job_execution_status>
 * 	statusDetails: {
 * 	}
 * 	queuedAt: number<time>
 *
 */
#define UPDATE_PAYLOAD "{\"status\":\"%s\",\"statusDetails\":%s},\
			\"expectedVersion\": %d, \"includeJobExecutionState\": true,\
			\"clientToken\": \"%s\"}"

/* This is generated for each job by the client can be ignored */
static char client_token_buf[64];

/* Buffer to keep the current job_id */
static u8_t job_id_buf[JOB_ID_MAX_LEN];

/* Staticly allocated buffers for keeping the topics needed for AWS jobs */
static u8_t notify_next_topic[NOTIFY_NEXT_TOPIC_LEN + 1];
static u8_t job_id_update_topic[UPDATE_TOPIC_LEN + 1];
static u8_t job_id_update_accepted_topic[GET_JOBID_TOPIC_MAX_LEN+ 1];
static u8_t job_id_update_rejected_topic[GET_JOBID_TOPIC_MAX_LEN+ 1];

/* Client struct pointer for MQTT */
struct mqtt_client * client = NULL;

static int publish_data_to_topic(u8_t * topic,
			  size_t topic_len,
			  u8_t * data,
			  size_t data_len)
{
	__ASSERT_NO_MSG(client != NULL);
	__ASSERT_NO_MSG(topic != NULL);
	__ASSERT_NO_MSG(data != NULL);

	struct mqtt_publish_param param = {
		.message.topic.qos = 1,
		.message.topic.topic.utf8 = topic,
		.message.topic.topic.size = topic_len,
		.message.payload.data = data,
		.message.payload.len = data_len,
		.message_id = sys_rand32_get(),
		.dup_flag = 0,
		.retain_flag = 0,
	};

	LOG_INF("Publishing job document: %s", data);
	LOG_INF("To topic: %s\n", topic);
	int err = mqtt_publish(client, &param);
	if(err) {
		LOG_ERR("Unable to publish\n");
		return -1;// -ERR;
	}

	return 0;//expected_version;
}


int update_execution_status(enum job_execution_status status,
			    char * job_id,
			    char * status_details,
			    int expected_version)
{
	__ASSERT(client != NULL, "MQTT client not initialized");
	__ASSERT(job_id != NULL, "Have not recived job ID");

	/* Max size document is 1350 char but the max size json document is actually 32kb
	 * set it to what is the limiting factor which is the MQTT buffer size on the
	 * reception end
	 */

	u8_t job_document[CONFIG_MQTT_MESSAGE_BUFFER_SIZE];

	int ret = snprintf(job_id_update_topic,
			   UPDATE_TOPIC_LEN,
		           UPDATE_TOPIC,
			   client->client_id.utf8,
			   job_id,
			   "");

	ret = snprintf(job_document,
		       ARRAY_SIZE(job_document),
		       UPDATE_PAYLOAD,
		       job_execution_status_map[status],
		       status_details,
		       expected_version,
		       /*TODO: Place holder until I know what to do with it */
		       "");

	/* TODO: Error check ret from sn printfs */
	LOG_INF("execution status update document: %s\n", job_document);

	ret = publish_data_to_topic(
		      job_id_update_topic,
		      strlen(job_id_update_topic),
		      job_document,
		      strlen(job_document));
	/* TODO: Not sure you can do error checking for this case */
	/*
	if (ret < ARRAY_SIZE(update_job_document)) {
		return -ENOMEM;
	}
	*/

	return 0;
}

int aws_jobs_process(u8_t * topic,
	             u8_t * json_payload)
{
	__ASSERT_NO_MSG(topic != NULL);
	__ASSERT_NO_MSG(json_payload != NULL);
	int err;
	if (!strncmp(notify_next_topic, topic, MIN(NOTIFY_NEXT_TOPIC_LEN, strlen(topic))))
	{
		/* Accepting update job */
		err = parse_job_document(json_payload);
		subscribe_job_id_topics();

		//execution_state = IN_PROGRESS;
		/*
		fota_state = DOWNLOAD_FIRMWARE;

		err = fota_download_start(hostname, file_path);
		*/

		if (err) {
			/*
			execution_state = FAILED;
			fota_state = NONE;
			*
			update_job_execution_status(c,
						    execution_state,
						    job_id_buf,
					            fota_state,
					            document_version_number);
			return;
			*/
		}

		/*
		update_job_execution_status(c,
					    execution_state,
					    job_id_buf,
					    fota_state,
					    document_version_number);
					    */
		/* Handle error state where you can't start the fota
		}*/


		//expected_document_version_number = accept_job(job_id);
		//unsubscribe_notify_next_topic(c);
	}
	else if (!strncmp(job_id_update_accepted_topic,
			  topic,
			  strlen(topic)
			  )
		)
	{
		/*
		if (execution_state == IN_PROGRESS)
		{

		}
		else if (execution_state == SUCCEEDED &&
			 fota_state == APPLY_FIRMWARE)
		{
			// Download completed and reported initiate system reset
			//NVIC_SystemReset();
		}
		 Update document version number */
	}
	else
	{
	}
}

int parse_job_document(u8_t * json_payload)
{
	cJSON * json = cJSON_Parse(json_payload);
	cJSON * document = cJSON_GetObjectItemCaseSensitive(json, "execution");
	if(!document){
		cJSON_free(json);
		return -EINVAL;
	}

	cJSON * job_id = cJSON_GetObjectItemCaseSensitive(document, "jobId");
	if (cJSON_IsString(job_id) && (job_id->valuestring != NULL))
	{
		printk("\r\nJobId: %s \n", job_id->valuestring);
		memcpy(job_id_buf,
		       job_id->valuestring,
		       strlen(job_id->valuestring));
	}

	cJSON * version_number = cJSON_GetObjectItemCaseSensitive(document, "versionNumber");
	if (cJSON_IsNumber(version_number))
	{
		printk("versionNumber: %d \n", version_number->valueint);
		//document_version_number = version_number->valueint;
	}
	else
	{
		return -EINVAL;
	}


	cJSON_free(json);

	return 0;
}

/* Function which returns the JobDocument found in the job execution document
int get_job_document(u8_t * output_buffer)
{
}*/

static int construct_notify_next_topic(void)
{
	int ret = snprintf(notify_next_topic,
			   NOTIFY_NEXT_TOPIC_LEN,
		           NOTIFY_NEXT_TOPIC,
			   client->client_id.utf8);
	//TODO: Find a way to error check this
	return 0;
}

int subscribe_notify_next(void)
{
	__ASSERT(jobs_notify_next != NULL, "Jobs notify next has not been constructed");
	__ASSERT(client != NULL, "MQTT client not initialized");

	struct mqtt_topic subscribe_topic [] =
	{
		{
			.topic = {
				.utf8 = notify_next_topic,
				.size = strlen(notify_next_topic)
			},
			.qos = MQTT_QOS_1_AT_LEAST_ONCE
		},
	};

	LOG_INF("Subscribe: %s\n", notify_next_topic);

	const struct mqtt_subscription_list subscription_list = {
		.list = (struct mqtt_topic *)&subscribe_topic,
		.list_count = 1,
		.message_id = SUB_NOTIFIY_NEXT
	};

	return mqtt_subscribe(client, &subscription_list);
}


/* Topics which needs to be subscribed to for getting notfied of a job and updates
 * https://docs.aws.amazon.com/iot/latest/developerguide/jobs-devices.html */
int subscribe_job_id_topics(void)
{
	//RETURN_IF_NOT_CONNECTED
	/* We subscribe to both accepted and rejected so we do get/# to simplify our code */
	char get_job_id_topic[GET_JOBID_TOPIC_MAX_LEN + 1];
	int ret = snprintf(get_job_id_topic,
			   GET_JOBID_TOPIC_MAX_LEN,
			   GET_JOBID_TOPIC,
			   client->client_id.utf8,
			   job_id_buf);

	/*
	if (ret != JOBS_GET_TOPIC_LEN)
	{
		return -ENOMEM;
	}
	*/

	LOG_DBG("jobid topic: %s\n", get_job_id_topic);

	/* Generate subscription list, if you update this update the list_count */
	struct mqtt_topic subscribe_topic []= {
		{
			.topic = {
				.utf8 = get_job_id_topic,
				.size = strlen(get_job_id_topic)
			},
			.qos = MQTT_QOS_1_AT_LEAST_ONCE
		},
	};

	const struct mqtt_subscription_list subscription_list = {
		.list = (struct mqtt_topic *)&subscribe_topic,
		.list_count = 1, //TODO: Find out if ARRAY_SIZE(subscribe_topic) works?
		.message_id = 111  //TODO: Find a good message_id number?
	};

	return mqtt_subscribe(client, &subscription_list);
}

/*
int unsubscribe_notify_next(void)
{
	return mqtt_unsubscribe(client, &subscription_list);
}
*/

int construct_job_id_update_topic(void)
{
	int ret = snprintf(job_id_update_accepted_topic,
			  UPDATE_TOPIC_LEN,
		          UPDATE_TOPIC,
		          client->client_id.utf8,
		          job_id_buf,
			  "accepted");

	ret = snprintf(job_id_update_rejected_topic,
			  UPDATE_TOPIC_LEN,
		          UPDATE_TOPIC,
		          client->client_id.utf8,
		          job_id_buf,
			  "rejected");
	//TODO: Handle error
	return 0;
}

/*
 * Subscribe to the specific topics bounded to the current jobId this should only
 * be done once you have accepted a job
 */
int subscribe_job_id_update(void)
{
	__ASSERT_NO_MSG(client != NULL);
	__ASSERT_NO_MSG(job_id_buf != NULL);

	/* Constructing job_id_update_topic */
	construct_job_id_update_topic();

	/* Subscribe to both rejected and accepted */
	int ret = snprintf(job_id_update_topic,
			  UPDATE_TOPIC_LEN,
		          UPDATE_TOPIC,
		          client->client_id.utf8,
		          job_id_buf,
			  "#");


	__ASSERT_NO_MSG(job_id_update_topic != NULL);

	struct mqtt_topic subscribe_topic []= {
		{
			.topic = {
				.utf8 = job_id_update_topic,
				.size = strlen(job_id_update_topic)
			},
			.qos = MQTT_QOS_1_AT_LEAST_ONCE
		},
	};

	const struct mqtt_subscription_list subscription_list = {
		.list = (struct mqtt_topic *)&subscribe_topic,
		.list_count = 1,
		.message_id = SUB_UPDATE
	};

	return mqtt_subscribe(client, &subscription_list);
}

/*
 * @brief Initialize AWS job library with the current MQTT client in use
 * for AWS IoT.
 *
 * @param[in] c Pointer to initialized MQTT Client instance
 */
int aws_jobs_init(struct mqtt_client * c)
{
	/* Initialize the pointer to the MQTT instance */
	client = c;
	__ASSERT_NO_MSG(client != NULL);

	int err = construct_notify_next_topic();

	if (err) {
		return err; //ERR
	}

	return 0;

}
