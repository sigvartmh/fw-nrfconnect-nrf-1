
#include <fota_download.h>
/* Enum for tracking the job exectuion state */
static enum execution_status execution_state;

/* Global local counter which contains the expected job document version,
 * this needs to match what the AWS jobs document contains if not
 * the update using that expectedVersionNumber will be rejected
 * https://docs.aws.amazon.com/iot/latest/developerguide/jobs-api.html#mqtt-updatejobexecution
 */
static int document_version_number = 0;

/* Create a sensible max (maybe 255?) */
static u8_t hostname[CONFIG_MQTT_MESSAGE_BUFFER_SIZE];
static u8_t file_path[CONFIG_MQTT_MESSAGE_BUFFER_SIZE];

//TODO: Switch this back
//#define SHADOW_STATE_UPDATE "{\"state\":{\"reported\":{\"nrfcloud__fota_v1__app_v\":\"%s\"}}}"
#define SHADOW_STATE_UPDATE "{\"state\":{\"reported\":{\"nrfcloud__fota_v1__app_v\":%d}}}"
/* Topic for updating shadow topic with version number */
#define UPDATE_DELTA_TOPIC AWS "%s/shadow/update"
#define UPDATE_DELTA_TOPIC_LEN (AWS_LEN + CLIENT_ID_LEN + 14)

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

err = fota_download_start(hostname, file_path);


parse_job_document
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


init{
	err = publish_version_to_device_shadow(c,"1");
	if (err) {
		return err; //ERR
	}
	err = fota_download_init(aws_fota_dl_handler);
	if (err) {
		return err; //ERR
	}
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
