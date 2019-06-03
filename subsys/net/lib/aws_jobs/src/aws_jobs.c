/*
 *copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */
#include <zephyr.h>
#include <stdio.h>
#include <net/mqtt.h>
#include <logging/log.h>
#include "aws_jobs.h"

LOG_MODULE_REGISTER(aws_jobs, CONFIG_AWS_JOBS_LOG_LEVEL);


/**
 * @brief Local function used to error check snprintf outputs.
 *
 * @param[in] ret Return value of snpritnf.
 * @param[in] size Max size of what would have been the final string.
 * @param[in] entry Name of the function it was called from.
 *
 */
static void report_snprintf_err(u32_t ret, u32_t size, char *entry)
{
	if (ret >= size) {
		LOG_ERR("Unable to allocate memory for %s", entry);
	} else if (ret < 0) {
		LOG_ERR("Output error for %s was encountered with return value"
			" %d", entry, ret);
	}
}


/**
 * @brief Constructs the get topic for accepted and rejeceted AWS jobs.
 *
 * @param[in] client_id Client ID of the current connected MQTT session.
 *
 */
static struct mqtt_topic construct_get_topic(u8_t *client_id)
{
	char get_topic[GET_TOPIC_LEN + 1];
	int ret = snprintf(get_topic,
			   GET_TOPIC_LEN,
			   GET_TOPIC_TEMPLATE,
			   client_id,
			   "#");

	report_snprintf_err(ret, GET_TOPIC_LEN, "get_topic");

	struct mqtt_topic topic = {
		.topic = {
			.utf8 = get_topic,
			.size = strlen(get_topic)
		},
		.qos = MQTT_QOS_1_AT_LEAST_ONCE
	};

	return topic;
}


/**
 * @brief Constructs then subscribes to get topic for AWS IoT jobs for accepted
 *	  and rejected get topic.
 *
 * @param[in] client Connected MQTT client instance.
 *
 * @retval 0 If successful otherwise the return code of mqtt_subscribe.
 *
 */
int aws_jobs_subscribe_get(struct mqtt_client *client)
{


	struct mqtt_topic subscribe_topic = construct_get_topic(
						client->client_id.utf8
						);
	const struct mqtt_subscription_list subscription_list = {
		.list = (struct mqtt_topic *)&subscribe_topic,
		.list_count = 1,
		.message_id = SUBSCRIBE_GET
	};
	LOG_DBG("Subscribe: %s\n", subscribe_topic.topic.utf8);

	return mqtt_subscribe(client, &subscription_list);
}


/**
 * @brief Constructs the notify-next topic for reciving AWS IoT jobs.
 *
 * @param[in] client_id Client ID of the current connected MQTT session.
 *
 */
static struct mqtt_topic construct_notify_next_topic(u8_t *client_id)
{
	char notify_next_topic[NOTIFY_NEXT_TOPIC_MAX_LEN  + 1];
	int ret = snprintf(notify_next_topic,
			   NOTIFY_NEXT_TOPIC_MAX_LEN,
			   NOTIFY_NEXT_TOPIC_TEMPLATE,
			   client_id);
	report_snprintf_err(ret,
			    NOTIFY_NEXT_TOPIC_MAX_LEN,
			    "notify_next_topic");

	struct mqtt_topic topic = {
		.topic = {
			.utf8 = notify_next_topic,
			.size = strlen(notify_next_topic)
		},
		.qos = MQTT_QOS_1_AT_LEAST_ONCE
	};

	return topic;
}


/**
 * @brief Constructs then subscribes to notify-next topic for reciving
 * AWS IoT jobs.
 *
 * @param[in] client Connected MQTT client instance.
 *
 * @retval 0 If successful otherwise the return code of mqtt_subscribe.
 *
 */
int aws_jobs_subscribe_notify_next(struct mqtt_client *client)
{


	struct mqtt_topic subscribe_topic = construct_notify_next_topic(
						client->client_id.utf8
						);
	const struct mqtt_subscription_list subscription_list = {
		.list = (struct mqtt_topic *)&subscribe_topic,
		.list_count = 1,
		.message_id = SUBSCRIBE_NOTIFY_NEXT
	};
	LOG_DBG("Subscribe: %s\n", subscribe_topic.topic.utf8);

	return mqtt_subscribe(client, &subscription_list);
}


/**
 * @brief Constructs the notifyt topic for reciving AWS IoT jobs.
 *
 * @param[in] client_id Client ID of the current connected MQTT session.
 *
 */
static struct mqtt_topic construct_notify_topic(u8_t *client_id)
{
	char notify_topic[NOTIFY_TOPIC_MAX_LEN  + 1];
	int ret = snprintf(notify_topic,
			   NOTIFY_TOPIC_MAX_LEN,
			   NOTIFY_TOPIC_TEMPLATE,
			   client_id);

	report_snprintf_err(ret, NOTIFY_TOPIC_MAX_LEN, "notify_topic");

	struct mqtt_topic topic = {
		.topic = {
			.utf8 = notify_topic,
			.size = strlen(notify_topic)
		},
		.qos = MQTT_QOS_1_AT_LEAST_ONCE
	};

	return topic;
}


/**
 * @brief Constructs then subscribes to notify topic for reciving  AWS IoT jobs.
 *
 * @param[in] client Connected MQTT client instance.
 *
 * @retval 0 If successful otherwise the return code of mqtt_subscribe.
 *
 */
int aws_jobs_subscribe_notify(struct mqtt_client *client)
{
	struct mqtt_topic subscribe_topic = construct_notify_topic(
						client->client_id.utf8
						);
	const struct mqtt_subscription_list subscription_list = {
		.list = (struct mqtt_topic *)&subscribe_topic,
		.list_count = 1,
		.message_id = SUBSCRIBE_NOTIFY
	};
	LOG_DBG("Subscribe: %s\n", subscribe_topic.topic.utf8);

	return mqtt_subscribe(client, &subscription_list);
}


/**@brief Subscribe to the expected topics of the AWS documentation
 * https://docs.aws.amazon.com/iot/latest/developerguide/jobs-devices.html.
 *
 * @param[in] client Connected MQTT client instance.
 * @param[in] notify_next Subscribes to notify next if true and notify if false.
 */
int aws_jobs_subscribe_expected_topics(struct mqtt_client *client,
				       bool notify_next)
{
	struct mqtt_topic subscribe_topics[2];
	u8_t *client_id = client->client_id.utf8;

	subscribe_topics[0] = construct_get_topic(client_id);
	subscribe_topics[1] = notify_next ?
				construct_notify_next_topic(client_id) :
				construct_notify_topic(client_id);

	LOG_DBG("Subscribe: %s\n", subscribe_topics[0].topic.utf8);
	LOG_DBG("Subscribe: %s\n", subscribe_topics[1].topic.utf8);

	const struct mqtt_subscription_list subscription_list = {
		.list = (struct mqtt_topic *)&subscribe_topics,
		.list_count = ARRAY_SIZE(subscribe_topics),
		.message_id = SUBSCRIBE_EXPECTED
	};

	return mqtt_subscribe(client, &subscription_list);
}


/**
 * @brief Constructs the get topic for accepted and rejeceted AWS jobs.
 *
 * @param[in] client_id Client ID of the current connected MQTT session.
 * @param[in] job_id Job ID of the current accepted job.
 *
 */
static struct mqtt_topic construct_job_id_get_topic(u8_t *client_id,
						    u8_t *job_id)
{
	char job_id_get_topic[JOB_ID_GET_TOPIC_MAX_LEN  + 1];
	int ret = snprintf(job_id_get_topic,
			  JOB_ID_GET_TOPIC_MAX_LEN,
			  JOB_ID_GET_TOPIC_TEMPLATE,
			  client_id,
			  job_id,
			  "#");
	report_snprintf_err(ret, JOB_ID_GET_TOPIC_MAX_LEN, "job_id_get_topic");

	struct mqtt_topic topic = {
		.topic = {
			.utf8 = job_id_get_topic,
			.size = strlen(job_id_get_topic)
		},
		.qos = MQTT_QOS_1_AT_LEAST_ONCE
	};

	return topic;
}


/**
 * @brief Constructs the get topic for accepted and rejeceted AWS jobs.
 *
 * @param[in] client Connected MQTT client instance.
 * @param[in] job_id JobId of the current accepted job.
 *
 */
int subscribe_job_id_get_topic(struct mqtt_client *client, u8_t *job_id)
{
	struct mqtt_topic subscribe_topic = construct_job_id_get_topic(
						client->client_id.utf8,
						job_id
						);
	const struct mqtt_subscription_list subscription_list = {
		.list = (struct mqtt_topic *)&subscribe_topic,
		.list_count = 1,
		.message_id = SUBSCRIBE_NOTIFY
	};
	LOG_DBG("Subscribe: %s\n", subscribe_topic.topic.utf8);

	return mqtt_subscribe(client, &subscription_list);
}


/**
 * @brief Constructs the update topic for AWS jobs.
 *
 * @param[in] client_id Client ID of the current connected MQTT session.
 * @param[in] job_id The JobId of the current recived job.
 *
 */
static struct mqtt_topic construct_job_id_update_topic(u8_t *client_id,
						       u8_t *job_id)
{
	char job_id_update_topic[JOB_ID_UPDATE_TOPIC_MAX_LEN + 1];
	int ret = snprintf(job_id_update_topic,
			   JOB_ID_UPDATE_TOPIC_MAX_LEN,
			   JOB_ID_UPDATE_TOPIC_TEMPLATE,
			   client_id,
			   job_id);

	report_snprintf_err(ret,
			    JOB_ID_UPDATE_TOPIC_MAX_LEN,
			    "job_id__update_topic");

	struct mqtt_topic topic = {
		.topic = {
			.utf8 = job_id_update_topic,
			.size = strlen(job_id_update_topic)
		},
		.qos = MQTT_QOS_1_AT_LEAST_ONCE
	};

	return topic;
}


/**
 * @brief AWS jobs update job execution status function.
 *
 * This implements the minimal requirements for doing an AWS job only
 * updating status and status details update are supported.
 *
 * @param[in] client Initialized and connected MQTT Client instance.
 * @param[in] job_id The ID of the job which you are updating this will be a
 *		part of the MQTT topic used to update the job.
 * @param[in] status The Job execution status.
 * @param[in] status_details JSON object in stringformat containing additional
		information. This object can contain up to 10 fields according
		to the AWS IoT jobs documentation.
 * @param[in] expected_version The expected job document version this needs to
 *		be incremented by 1 for every update.
 * @param[in] client_token This can be an arbitrary value and will be reflected
 *		in the response to the update.
 *
 * @retval 0 If update is published successful otherwise return mqtt_publish
 *	     error code.
 *
 */
#define UPDATE_JOB_PAYLOAD "{\"status\":\"%s\",\
			    \"statusDetails\":\"%s\",\
			    \"expectedVersion\": \"%d\",\
			    \"clientToken\": \"%s\"}"
int aws_update_job_execution(struct mqtt_client *client,
			     u8_t *job_id,
			     enum execution_status status,
			     u8_t *status_details,
			     int expected_version,
			     u8_t *client_token)
{
	/* Max size document is 1350 char but the max size json document is
	 * actually 32kb set it to what is the limiting factor which is the MQTT
	 * buffer size for reception end
	 */
	u8_t update_job_payload[CONFIG_MQTT_MESSAGE_BUFFER_SIZE];

	int ret = snprintf(update_job_payload,
		       ARRAY_SIZE(update_job_payload),
		       UPDATE_JOB_PAYLOAD,
		       execution_status_map[status],
		       status_details,
		       expected_version,
		       client_token);

	report_snprintf_err(ret,
			    ARRAY_SIZE(update_job_payload),
			    "update_job_payload");

	struct mqtt_topic job_id_update_topic = construct_job_id_update_topic(
							client->client_id.utf8,
							job_id);

	struct mqtt_publish_param param = {
		.message.topic = job_id_update_topic,
		.message.payload.data = update_job_payload,
		.message.payload.len = strlen(update_job_payload),
		.message_id = sys_rand32_get(),
		.dup_flag = 0,
		.retain_flag = 0,
	};

	return mqtt_publish(client, &param);
}
