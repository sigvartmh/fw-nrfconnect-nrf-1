#include <zephyr.h>
#include <net/mqtt.h>

/*
 * Jobdocument structure
 * {
 *	operation: <json obj> (currently only app_fw_update)
 *	fwversion: string
 *	size: number
 *	location: string
 * }
 */

struct location_obj
{
	const char * host;
	const char * path;
};


static const struct json_obj_descr location_obj_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct location_obj, host, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct location_obj, path, JSON_TOK_STRING),
};


struct operation_obj
{
	const char * app_fw_update;
};

static const struct json_obj_descr operation_obj_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct operation_obj,
			    app_fw_update,
			    JSON_TOK_STRING),
};


struct job_document
{
	struct operation_obj operation;
	const char * fwversion;
	int size;
	struct location_obj location;
};

static const struct json_obj_descr job_document_descr[] = {
	JSON_OBJ_DESCR_OBJECT(struct job_document,
			      operation,
			      operation_obj_descr
			      ),
	JSON_OBJ_DESCR_PRIM(struct job_document, fwversion, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct job_document, size, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_OBJECT(struct job_document,
			      location,
			      location_obj_descr
			      ),
};

struct execution_obj
{
	const char * job_id;
	const char * status;
	int queued_at;
	int version_number;
	int execution_number;
	struct job_document;
}

static const struct json_obj_descr execution_obj_descr[] = {
	JSON_OBJ_DESCR_PRIM_NAMED(struct execution_obj,
				  "jobId",
				  job_id,
				  JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct execution_obj, status, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM_NAMED(struct execution_obj,
				  "queuedAt",
				  queued_at,
				  JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM_NAMED(struct execution_obj,
				  "versionNumber",
				  version_number,
				  JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM_NAMED(struct execution_obj,
				  "executionNumber",
				  execution_number,
				  JSON_TOK_STRING),
	JSON_OBJ_DESCR_OBJECT_NAMED(struct execution_obj,
			      "jobDocument",
			      job_document,
			      job_document_descr
			      ),

}

struct aws_jobs_document
{
	int timestamp;
	struct execution_obj execution;
}

static const struct json_obj_descr aws_jobs_document_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct aws_jobs_document,
			    timestamp,
			    JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_OBJECT(struct aws_jobs_document,
			      execution,
			      execution_obj_descr),
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

struct status_details_obj
{
	const char * nextState;
};

struct job_execution_document
{
	const char * jobId;
	const char * status;

}

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

enum fota_status {
	DOWNLOAD_FIRMWARE = 0,
	APPLY_FIRMWARE,
	NONE
};

char * fota_status_map [] = {
	"download_firmware",
	"apply_update",
	"none"
};

enum job_execution_status {
	QUEUED = 0,
	IN_PROGERSS,
	SUCCEEDED,
	FAILED,
	TIMED_OUT,
	REJECTED,
	REMOVED,
	CANCELED
};

char * job_execution_status_map [] = {
	"QUEUED",
	"IN_PROGESS",
	"SUCCEEDED",
	"FAILED",
	"TIMED_OUT",
	"REJECTED",
	"REMOVED",
	"CANCELED"
};

/* Enum for tracking the job exectuion state */
static enum job_execution_status job_state;
/* Global counter which contains the expected job document version,
 * this needs to match what the AWS jobs document contains if not
 * the update using that expectedVersionNumber will be rejected
 * https://docs.aws.amazon.com/iot/latest/developerguide/jobs-api.html#mqtt-updatejobexecution
 */
static int expected_document_version_number = 0;

/* Struct for mqtt client handler */
static struct mqtt_client * client;

/* This is generated for each job by the client */
static char client_token_buf[64];

#define AWS "$aws/things/"
#define AWS_LEN (sizeof(AWS) - 1)

/* The max JOB_ID_LEN according to
* https://docs.aws.amazon.com/iot/latest/apireference/API_CreateJob.html#API_CreateJob_RequestParameters
*/
#define JOB_ID_MAX_LEN 64

#define JOBS_GET_TOPIC AWS "%s/jobs/get/#"
#define JOBS_GET_TOPIC_LEN (AWS_LEN + CLIENT_ID_LEN + 11)
#define JOBS_GET_JOBID_TOPIC AWS "%s/jobs/%s/get/#"
#define JOBS_GET_JOBID_TOPIC_LEN (AWS_LEN + CLIENT_ID_LEN + JOB_ID_MAX_LEN + 12)
#define JOBS_NOTIFY_NEXT_TOPIC AWS "%s/jobs/notify-next"
#define JOBS_NOTIFY_NEXT_TOPIC_LEN (AWS_LEN + CLIENT_ID_LEN + 17)

/* For future use-cases */
#define JOBS_NOTIFY_TOPIC AWS "%s/jobs/notify"
#define JOBS_NOTIFY_TOPIC_LEN (AWS_LEN + CLIENT_ID_LEN + 12)

static u8_t jobs_notify_next_topic[JOBS_NOTIFY_NEXT_TOPIC_LEN + 1];
static u8_t job_id_buf[JOB_ID_MAX_LEN];

static void aws_jobs_handler(struct mqtt_client * c,
			     u8_t * topic,
			     u8_t * json_payload)
{
	if (!strncmp(jobs_notify_next_topic, topic, JOBS_NOTIFY_TOPIC_LEN))
	{
		/* Accept job and update job document */
	}
	else if (!strncmp(jobs_update_topic, topic, JOBS_GET_JOBID_TOPIC))
	{
		/* Update job document version counter if accepted*/
	}
}


/*
 * Dispatcher for handling job updates the job state
 */
static void aws_jobs_update_handler(struct mqtt_client * c,
				    u8_t * topic,
				    u8_t * json_payload)
{
}


/*
 * Generic method for updating the Job status with the correct information
 */
static int aws_update_job_execution(struct mqtt_client * c,
				const char * status,
				const char * job_id)
{
}


/* Topics which needs to be subscribed to for getting notfied of a job and updates
 * https://docs.aws.amazon.com/iot/latest/developerguide/jobs-devices.html */

int subscribe_to_jobs_topics(struct mqtt_client * c)
{
	/* We subscribe to both accepted and rejected so we do get/# to simplify our code */
	char jobs_get_topic[JOBS_GET_TOPIC_LEN + 1];
	int ret = snprintf(jobs_get_topic,
			   JOBS_GET_TOPIC_LEN,
			   JOBS_GET_TOPIC,
			   c->client_id.utf8);

	if (ret != JOBS_GET_LEN)
	{
		return -ENOMEM;
	}

	ret = snprintf(jobs_notify_next_topic,
		       JOBS_NOTIFY_NEXT_TOPIC_LEN,
		       JOBS_NOTIFY_NEXT_TOPIC,
		       client_id_buf);
	if (ret != JOBS_NOTIFY_TOPIC_LEN){
		return -ENOMEM;
	}

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
	}

	const struct mqtt_subscription_list subscription_list = {
		.list = (struct mqtt_topic *)&subscribe_topic,
		.list_count = 2, //TODO: Find out if ARRAY_SIZE(subscribe_topic) works?
		.message_id = 71775
	};

	return mqtt_subscribe(c, &subscription_list);
}

/*
 * Subscribe to the specific topics bounded to the current jobId this should only
 * be done once you have accepted a job
 */
int aws_jobs_subscribe_to_job_id_topic(struct mqtt_client * c)
{
	char jobs_get_jobid_topic[JOBS_GET_JOBID_LEN  + 1];
	printk("rejected topic: %s\n", jobs_get_rejected);
	int ret = snprintf(jobs_get_jobid_topic,
		       JOBS_GET_JOBID_TOPIC_LEN,
		       JOBS_GET_JOBID_TOPIC,
		       c->client_id.utf8,
		       job_id_buf);

}

struct job_document
{
	char job_id[JOB_ID_MAX_LEN],
	char status[STATUS_MAX_LEN],
	int versionNumber
}

struct job_document parse_job_document(u8_t * json_string)
{
	struct job_document job;
	cJSON * json = cJSON_Parse(json_string);
	cJSON * document = cJSON_GetObjectItemCaseSensitive(json, "execution");
	if(!document){
		cJSON_free(json);
		return;
	}

	/* If we find a job_id we store it in a global buffer to be used when subscribing */
	if (cJSON_IsString(job_id) && (job_id->valuestring != NULL))
	{
		memcpy(job.job_id, job_id->valuestring, strlen(job_id->valuestring));
	}

	return job;
}


int aws_jobs_init(struct mqtt_client * c)
{
	__ASSERT(c != NULL, "Invalid mqtt_client object");
	client = c;
	subscribe_to_jobs_topics(c);
}
