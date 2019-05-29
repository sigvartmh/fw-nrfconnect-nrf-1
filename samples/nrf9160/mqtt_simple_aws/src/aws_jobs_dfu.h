#ifndef __APP_DFU
#define __APP_DFU
/* Enum to keep the fota status */
enum fota_status {
	DOWNLOAD_FIRMWARE = 0,
	APPLY_FIRMWARE,
	NONE
};

/* Map of fota status to report back */
const char * fota_status_map [] = {
	"download_firmware",
	"apply_update",
	"none"
};

/* Maybe status or event ?*/
/* Enum to keep the AWS job status */
enum job_execution_status {
	QUEUED = 0,
	IN_PROGRESS,
	SUCCEEDED,
	FAILED,
	TIMED_OUT,
	REJECTED,
	REMOVED,
	CANCELED
};

/* Map of enum job status enum to strings */
const char * job_execution_status_map [] = {
	"QUEUED",
	"IN_PROGRESS",
	"SUCCEEDED",
	"FAILED",
	"TIMED_OUT",
	"REJECTED",
	"REMOVED",
	"CANCELED"
};

/*
 * AWS job handler which should handle incoming mqtt publish events
 * on AWS specific topics
 */
void aws_jobs_handler(struct mqtt_client * c,
			     u8_t * topic,
			     u8_t * json_payload);
/*
 * Initializes the global client struct that is used in callbacks
 * and subscribes required topics for AWS jobs
 */
int aws_jobs_init(struct mqtt_client * c);
#endif

