#include "aws_jobs.h"

/**
 * @brief Client struct pointer for MQTT
 */
struct mqtt_client * c;

static bool connected;

/**
 * @brief Global local counter which contains the expected job document version,
 * this needs to match what the AWS jobs document contains if not
 * the update using that expectedVersionNumber will be rejected
 * https://docs.aws.amazon.com/iot/latest/developerguide/jobs-api.html#mqtt-updatejobexecution
 */
static int expected_document_version_number = 0;

/**
 * @brief Constructs then subscribes to get topic for AWS IoT jobs for accepted
 *	  and rejected get topic.
 *
 * @param[in] client Initialized MQTT client instance.
 *
 * @retval 0 If successful otherwise the return code of mqtt_subscribe.
 *
 */
int aws_jobs_fota_init(struct mqtt_client *client)
{
	__ASSERT(client != NULL,
		 "Recived an uninitialized MQTT client instance");
	c = client;
}

int aws_jobs_fota_process(const struct mqtt_evt *evt)
{
	switch (evt->type) {
	case MQTT_EVT_CONNACK: {
		//subscribe_job_topic
	}
	case MQTT_EVT_PUBLISH: {
		//filter_topic
		//aws_jobs_fota_handler
}

static int download_file_handler(void)
{
	//if download complete
	//send event to application that you can restart
	//when the success update document has been accepted

//subscribe_job_topic
