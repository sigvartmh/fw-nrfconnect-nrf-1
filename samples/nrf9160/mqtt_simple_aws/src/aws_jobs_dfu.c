#include <zephyr.h>
#include <stdio.h>
#include <net/mqtt.h>
#include <fota_download.h>
#include "aws_jobs_dfu.h"
#include <cJSON.h>

/*
 * Jobdocument structure
 * {
 *	operation: <json obj> (currently only app_fw_update)
 *	fwversion: string
 *	size: number
 *	location: string
 * }
 */

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

/* The max JOB_ID_LEN according to AWS docs
 * https://docs.aws.amazon.com/general/latest/gr/aws_service_limits.html#job-limits
 */

#define JOB_ID_MAX_LEN 64
#define STATUS_MAX_LEN 12
#define IMEI_LEN 15
#define CLIENT_ID_LEN (IMEI_LEN + 4)

#define AWS "$aws/things/"
#define AWS_LEN (sizeof(AWS) - 1)

#define JOBS_NOTIFY_NEXT_TOPIC AWS "%s/jobs/notify-next"
#define JOBS_NOTIFY_NEXT_TOPIC_LEN (AWS_LEN + CLIENT_ID_LEN + 18)

#define JOBS_GET_TOPIC AWS "%s/jobs/get/#"
#define JOBS_GET_TOPIC_LEN (AWS_LEN + CLIENT_ID_LEN + 11)

#define JOBS_GET_JOBID_TOPIC AWS "%s/jobs/%s/get/#"
#define JOBS_GET_JOBID_TOPIC_MAX_LEN (AWS_LEN +\
				      CLIENT_ID_LEN +\
				      JOB_ID_MAX_LEN +\
				      13)
#define JOBS_GET_JOBID_TOPIC_SPECIFIED AWS "%s/jobs/%s/get/%s"
#define JOBS_GET_JOBID_TOPIC_SPECIFIED_MAX_LEN (AWS_LEN +\
						CLIENT_ID_LEN +\
						JOB_ID_MAX_LEN + 12 + 9 + 2)
//#define JOBS_GET_JOBID_TOPIC_LEN (AWS_LEN + CLIENT_ID_LEN + JOB_ID_MAX_LEN + 12)

/* For future use-cases */
#define JOBS_NOTIFY_TOPIC AWS "%s/jobs/notify"
#define JOBS_NOTIFY_TOPIC_LEN (AWS_LEN + CLIENT_ID_LEN + 12)

/* Topic for updating shadow topic with version number */
#define UPDATE_DELTA_TOPIC AWS "%s/shadow/update"
#define UPDATE_DELTA_TOPIC_LEN (AWS_LEN + CLIENT_ID_LEN + 14)

#define JOBS_UPDATE_TOPIC AWS "%s/jobs/%s/update"
#define JOBS_UPDATE_TOPIC_LEN (AWS_LEN + CLIENT_ID_LEN + JOB_ID_MAX_LEN + 14)
//TODO: Switch this back
//#define SHADOW_STATE_UPDATE "{\"state\":{\"reported\":{\"nrfcloud__fota_v1__app_v\":\"%s\"}}}"
#define SHADOW_STATE_UPDATE "{\"state\":{\"reported\":{\"nrfcloud__fota_v1__app_v\":%d}}}"

#define JOBS_UPDATE_PAYLOAD "{\"status\":\"%s\",\"statusDetails\":{\"nextState\":\"%s\"},\
			    \"expectedVersion\": %d, \"includeJobExecutionState\": true,\
			    \"clientToken\": \"%s\"}"

/* Enum for tracking the job exectuion state */
static enum job_execution_status execution_state;
static enum fota_status fota_state;

/* Global local counter which contains the expected job document version,
 * this needs to match what the AWS jobs document contains if not
 * the update using that expectedVersionNumber will be rejected
 * https://docs.aws.amazon.com/iot/latest/developerguide/jobs-api.html#mqtt-updatejobexecution
 */
static int document_version_number = 0;

/* Client struct pointer for MQTT */
struct mqtt_client * client;

/* This is generated for each job by the client */
static char client_token_buf[64];

/* Staticly allocated buffers for keeping the topics needed for AWS jobs */
static u8_t jobs_notify_next_topic[JOBS_NOTIFY_NEXT_TOPIC_LEN + 1];
static u8_t jobs_get_jobid_updated_accepted_topic[JOBS_GET_JOBID_TOPIC_MAX_LEN+ 1];
static u8_t jobs_get_jobid_updated_rejected_topic[JOBS_GET_JOBID_TOPIC_MAX_LEN+ 1];

/* Buffer to keep the current job_id */
static u8_t job_id_buf[JOB_ID_MAX_LEN];
/* Create a sensible max (maybe 255?) */
static u8_t hostname[CONFIG_MQTT_MESSAGE_BUFFER_SIZE];
static u8_t file_path[CONFIG_MQTT_MESSAGE_BUFFER_SIZE];

static u8_t jobs_jobid_update_topic[JOBS_UPDATE_TOPIC_LEN + 1];
static u8_t jobs_jobid_update_accepted_topic[JOBS_UPDATE_TOPIC_LEN + 1];

static struct k_work fota_work;

int publish_data_to_topic(struct mqtt_client * c,
			  char * topic,
			  int topic_len,
			  char * data,
			  int data_len)
{
	struct mqtt_publish_param param;
	param.message.topic.qos = 1;
	param.message.topic.topic.utf8 = topic;
	param.message.topic.topic.size = topic_len;
	param.message.payload.data = data;
	param.message.payload.len = data_len;
	param.message_id = sys_rand32_get();
	param.dup_flag = 0;
	param.retain_flag = 0;

	printk("Publishing document\n");
	int err = mqtt_publish(c, &param);
	if(err) {
		printk("unable to publish update document\n");
		return -1;// -ERR;
	}

	return 0;//expected_version;
}

static int update_job_execution_status(struct mqtt_client * c,
		      		       enum job_execution_status status,
		      		       char * job_id,
		      		       enum fota_status next_state,
		      		       int expected_version)
{
	/* Max size document is 1350 char but the max size json document is actually 32kb
	 * set it to what is the limiting factor which is the MQTT buffer size on the
	 * reception end
	 */
	u8_t update_job_document[CONFIG_MQTT_MESSAGE_BUFFER_SIZE];

	int ret = snprintf(jobs_jobid_update_topic,
			   JOBS_UPDATE_TOPIC_LEN,
		           JOBS_UPDATE_TOPIC,
			   c->client_id.utf8,
			   job_id);

	/* TODO: Add error check
	if (ret < JOBS_UPDATE_TOPIC_LEN) {
		return -ENOMEM;
	}*/
	printk("Publishing to topic: %s\n", jobs_jobid_update_topic);

	ret = snprintf(update_job_document,
		       ARRAY_SIZE(update_job_document),
		       JOBS_UPDATE_PAYLOAD,
		       job_execution_status_map[status],
		       fota_status_map[next_state],
		       expected_version,
		       /*TODO: Place holder until I know what to do with it */
		       "SomeClientToken");

	printk("document: %s\n", update_job_document);

	/* TODO: Not sure you can do error checking for this case */
	/*
	if (ret < ARRAY_SIZE(update_job_document)) {
		return -ENOMEM;
	}
	*/

	/* Increment version number by 1 as expected by AWS */
	//expected_version = expected_version + 1;

	ret = publish_data_to_topic(c,
		      jobs_jobid_update_topic,
		      strlen(jobs_jobid_update_topic),
		      update_job_document,
		      strlen(update_job_document)
		     );
	if(ret)
	{
		return ret;
	}

	document_version_number = expected_version + 1;
	return ret;


}

static void app_start_dfu(struct k_work * unused)
{
	int retval;
	retval = fota_download_start(hostname, file_path);
	if(retval != 0) {
		printk("fota_download_start () failed, err %d",
		       retval);
	}
	return;
}

void aws_jobs_handler(struct mqtt_client * c,
			     u8_t * topic,
			     u8_t * json_payload)
{
	int err;
	printk("Entered aws_jobs hander with topic: %s\n\r payload: %s", topic, json_payload);
	if (!strncmp(jobs_notify_next_topic, topic, strlen(topic)))
	{
		/* Accepting update job */
		err = parse_job_document(json_payload);
		subscribe_to_job_id_topics(c, job_id_buf);

		execution_state = IN_PROGRESS;
		fota_state = DOWNLOAD_FIRMWARE;

		k_work_submit(&fota_work);

		if (err) {
			execution_state = FAILED;
			fota_state = NONE;
			update_job_execution_status(c,
						    execution_state,
						    job_id_buf,
					            fota_state,
					            document_version_number);
			return;
		}

		update_job_execution_status(c,
					    execution_state,
					    job_id_buf,
					    fota_state,
					    document_version_number);
		/* Handle error state where you can't start the fota
		}*/


		//expected_document_version_number = accept_job(job_id);
		//unsubscribe_notify_next_topic(c);
	}
	else if (!strncmp(jobs_jobid_update_accepted_topic,
			  topic,
			  strlen(topic)
			  )
		)
	{
		if (execution_state == IN_PROGRESS)
		{
			/* Update document version number */

		}
		else if (execution_state == SUCCEEDED &&
			 fota_state == APPLY_FIRMWARE)
		{
			/* Download completed and reported initiate system reset */
			//NVIC_SystemReset();
		}
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
		document_version_number = version_number->valueint;
	}
	else
	{
		return -EINVAL;
	}


	cJSON * job_document = cJSON_GetObjectItemCaseSensitive(document, "jobDocument");
	cJSON * fw_version = cJSON_GetObjectItemCaseSensitive(job_document, "fwversion");
	if (cJSON_IsNumber(fw_version))
	{
		printk("fw_version: %d \n", fw_version->valueint);
	}
	cJSON * location_document = cJSON_GetObjectItemCaseSensitive(job_document, "location");
	cJSON * host = cJSON_GetObjectItemCaseSensitive(location_document, "host");
	if (cJSON_IsString(host) && (host->valuestring != NULL))
	{
		memcpy(hostname, host->valuestring, strlen(host->valuestring));
	}

	cJSON * path = cJSON_GetObjectItemCaseSensitive(location_document, "path");
	if(cJSON_IsString(path) && (path->valuestring != NULL))
	{
		memcpy(file_path, path->valuestring, strlen(path->valuestring));
	}
	printk("host: %s, path:%s \n", hostname, file_path);
	cJSON_free(json);


	return 0;
}

/*
static int notify_next_handler(struct mqtt_client *c, u8_t * json_string)
{
	int job_version_number = 0;
	cJSON * json = cJSON_Parse(json_string);
	cJSON * document = cJSON_GetObjectItemCaseSensitive(json, "execution");
	if(!document){
		cJSON_free(json);
		return -EFAULT;
	}
	cJSON * job_id = cJSON_GetObjectItemCaseSensitive(document, "jobId");
	if (cJSON_IsString(job_id) && (job_id->valuestring != NULL))
	{
		printk("JobId: %s \n", job_id->valuestring);
		memcpy(g_job_id,
		       job_id->valuestring,
		       strlen(job_id->valuestring));
	}
	cJSON * status = cJSON_GetObjectItemCaseSensitive(document, "status");
	if (cJSON_IsString(status) && (status->valuestring != NULL))
	{
		printk("status: %s \n", status->valuestring);
	}
	cJSON * document_version_number = cJSON_GetObjectItemCaseSensitive(document, "versionNumber");
	if (cJSON_IsNumber(document_version_number))
	{
		printk("versionNumber: %d \n", document_version_number->valueint);
		job_version_number = document_version_number->valueint;
	}

	return job_version_number;
}
*/


/*
 * Dispatcher for handling job updates the FSM state
 */
static void aws_jobs_update_handler(struct mqtt_client * c,
				    u8_t * topic,
				    u8_t * json_payload)
{
}


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

	return publish_data_to_topic(c,
			      update_delta_topic,
			      UPDATE_DELTA_TOPIC_LEN,
			      data,
			      strlen(data)
			     );
}


/*
 * Generic method for updating the Job status with the correct information
static int update_job_execution(struct mqtt_client * c,
				const char * status,
				const char * job_id)
{
}
 */

int construct_notify_next_topic(struct mqtt_client * c)
{
	int ret = snprintf(jobs_notify_next_topic,
			   JOBS_NOTIFY_NEXT_TOPIC_LEN,
		           JOBS_NOTIFY_NEXT_TOPIC,
			   c->client_id.utf8);
	//TODO: Find a way to error check this
	return 0;
}

int subscribe_to_jobs_notify_next(struct mqtt_client * c)
{
	__ASSERT(strlen(jobs_notify_next) != NULL,
			"Jobs notify next has not been constructed")
	struct mqtt_topic subscribe_topic [] =
	{
		{
			.topic = {
				.utf8 = jobs_notify_next_topic,
				.size = strlen(jobs_notify_next_topic)
			},
			.qos = MQTT_QOS_1_AT_LEAST_ONCE
		},
	};

	printk("Subscribing to: %s\r\n", jobs_notify_next_topic);

	const struct mqtt_subscription_list subscription_list = {
		.list = (struct mqtt_topic *)&subscribe_topic,
		.list_count = 1,
		.message_id = 1234
	};

	return mqtt_subscribe(c, &subscription_list);
}

fota_download_callback_t aws_fota_dl_handler(const struct fota_download_evt * evt)
{
	printk("=?=Fota callback recived===\n");
	if (evt->id == FOTA_DOWNLOAD_EVT_DOWNLOAD_CLIENT &&
		evt->dlc_evt->id == DOWNLOAD_CLIENT_EVT_DONE) {
		execution_state = SUCCEEDED;
		fota_state = APPLY_FIRMWARE;
		update_job_execution_status(client,
					    execution_state,
					    job_id_buf,
					    fota_state,
					    document_version_number);
	}
	//TODO: Handle fragments to give updates on progress to cloud
}

/* Topics which needs to be subscribed to for getting notfied of a job and updates
 * https://docs.aws.amazon.com/iot/latest/developerguide/jobs-devices.html */
int subscribe_to_job_id_topics(struct mqtt_client * c, const char * job_id)
{
	/* We subscribe to both accepted and rejected so we do get/# to simplify our code */
	char jobs_get_jobid_topic[JOBS_GET_JOBID_TOPIC_MAX_LEN + 1];
	int ret = snprintf(jobs_get_jobid_topic,
			   JOBS_GET_JOBID_TOPIC_MAX_LEN,
			   JOBS_GET_JOBID_TOPIC,
			   c->client_id.utf8,
			   job_id);

	/*
	if (ret != JOBS_GET_TOPIC_LEN)
	{
		return -ENOMEM;
	}
	*/

	printk("Jobs get jobid: %s\n", jobs_get_jobid_topic);

	/* Generate subscription list, if you update this update the list_count */
	struct mqtt_topic subscribe_topic []= {
		{
			.topic = {
				.utf8 = jobs_get_jobid_topic,
				.size = strlen(jobs_get_jobid_topic)
			},
			.qos = MQTT_QOS_1_AT_LEAST_ONCE
		},
	};

	const struct mqtt_subscription_list subscription_list = {
		.list = (struct mqtt_topic *)&subscribe_topic,
		.list_count = 1, //TODO: Find out if ARRAY_SIZE(subscribe_topic) works?
		.message_id = 111  //TODO: Find a good message_id number?
	};

	return mqtt_subscribe(c, &subscription_list);
}

/*
 * Subscribe to the specific topics bounded to the current jobId this should only
 * be done once you have accepted a job
 */
int subscribe_to_job_id_topic(struct mqtt_client * c)
{
	char jobs_get_jobid_topic[JOBS_GET_JOBID_TOPIC_SPECIFIED_MAX_LEN  + 1];
	int ret = snprintf(jobs_get_jobid_topic,
			  JOBS_GET_JOBID_TOPIC_SPECIFIED_MAX_LEN,
		          JOBS_GET_JOBID_TOPIC_SPECIFIED,
		          c->client_id.utf8,
		          job_id_buf,
			  "accepted"
			  );
	return ret;
}

//int aws_jobs_init(struct mqtt_client * c, const char * app_version)
int aws_jobs_init(struct mqtt_client * c)
{
	k_work_init(&fota_work, app_start_dfu);
	client = c;
	int err = construct_notify_next_topic(c);
	if (err) {
		return err; //ERR
	}

	err = subscribe_to_jobs_notify_next(c);
	if (err) {
		return err; //ERR
	}

	err = publish_version_to_device_shadow(c,"1");
	if (err) {
		return err; //ERR
	}


	err = fota_download_init(aws_fota_dl_handler);
	if (err) {
		return err; //ERR
	}

	return 0;

}

