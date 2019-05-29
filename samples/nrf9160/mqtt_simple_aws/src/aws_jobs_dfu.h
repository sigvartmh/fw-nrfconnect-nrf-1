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
#endif

