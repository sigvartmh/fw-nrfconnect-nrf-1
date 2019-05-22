/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <stdio.h>
#include <uart.h>
#include <string.h>

#include <net/mqtt.h>
#include <net/socket.h>
#include <lte_lc.h>
#include <cJSON.h>

#if defined(CONFIG_BSD_LIBRARY)
#include "nrf_inbuilt_key.h"
#endif

#define CONFIG_NRF_CLOUD_SEC_TAG 16842753
#define CONFIG_AWS_HOSTNAME "a25jld2wxwm7qs-ats.iot.eu-central-1.amazonaws.com"

//#include "aws_cert/rootca_rsa2048.h"
#include "aws_cert/rootca_ec.h"
#include "aws_cert/private_key.h"
#include "aws_cert/pubcert.h"

#if !defined(CONFIG_CLIENT_ID)
#define IMEI_LEN 15
#define CLIENT_ID_LEN (IMEI_LEN + 4)
#else
#define CLIENT_ID_LEN (sizeof(CLOUD_CLIENT_ID) - 1)
#endif

#define AWS "$aws/things/"
#define AWS_LEN (sizeof(AWS) - 1)

#define JOBS_NOTIFY_TOPIC AWS "%s/jobs/notify-next"
#define JOBS_NOTIFY_TOPIC_LEN (AWS_LEN + CLIENT_ID_LEN + 17)
#define JOBS_GET AWS "%s/jobs/%sget/%s"

/* Buffer for keeping the client_id + \0 */
static char client_id_buf[CLIENT_ID_LEN + 1];

/* Buffers for MQTT client. */
static u8_t rx_buffer[CONFIG_MQTT_MESSAGE_BUFFER_SIZE];
static u8_t tx_buffer[CONFIG_MQTT_MESSAGE_BUFFER_SIZE];
static u8_t payload_buf[CONFIG_MQTT_PAYLOAD_BUFFER_SIZE];
static u8_t hostname[128];
static u8_t file_path[255];

/* MQTT Broker details. */
static struct sockaddr_storage broker;

/* File descriptor */
static struct pollfd fds;


#if defined(CONFIG_BSD_LIBRARY)

/**@brief Recoverable BSD library error. */
void bsd_recoverable_error_handler(uint32_t err)
{
	printk("bsdlib recoverable error: %u\n", err);
}

/**@brief Irrecoverable BSD library error. */
void bsd_irrecoverable_error_handler(uint32_t err)
{
	printk("bsdlib irrecoverable error: %u\n", err);

	__ASSERT_NO_MSG(false);
}

#endif /* defined(CONFIG_BSD_LIBRARY) */

/**@brief Function to print strings without null-termination
*/
static void data_print(u8_t *prefix, u8_t *data, size_t len)
{
	char buf[len + 1];

	memcpy(buf, data, len);
	buf[len] = 0;
	printk("%s%s\n", prefix, buf);
}

/**@brief Function to publish data on the configured topic
*/
static int data_publish(struct mqtt_client *c, enum mqtt_qos qos,
			u8_t *data, size_t len)
{
	struct mqtt_publish_param param;

	param.message.topic.qos = qos;
	param.message.topic.topic.utf8 = CONFIG_MQTT_PUB_TOPIC;
	param.message.topic.topic.size = strlen(CONFIG_MQTT_PUB_TOPIC);
	param.message.payload.data = data;
	param.message.payload.len = len;
	param.message_id = sys_rand32_get();
	param.dup_flag = 0;
	param.retain_flag = 0;

	data_print("publishing: ", data, len);
	/*
	printk("to topic: %s len: %u\n",
	       config_mqtt_pub_topic,
	       (unsigned int)strlen(config_mqtt_pub_topic));
	       */

	return mqtt_publish(c, &param);
}

/**@brief Function to subscribe to the configured topic
*/
#define JOB_ID_MAX_LEN (64+2)
#define JOBS_GET_LEN (AWS_LEN + CLIENT_ID_LEN + JOB_ID_MAX_LEN )
#define UPDATE_DELTA_TOPIC AWS "%s/shadow/update"
#define UPDATE_DELTA_TOPIC_LEN (AWS_LEN + CLIENT_ID_LEN + 14)
#define SHADOW_STATE_UPDATE "{\"state\":{\"reported\":{\"nrfcloud_app_fw_version\":%d}}}"
static int publish_shadow_state(struct mqtt_client *c)
{
	char update_delta_topic[UPDATE_DELTA_TOPIC_LEN + 1];
	int app_version = 1;
	u8_t data[CONFIG_MQTT_MESSAGE_BUFFER_SIZE];
	struct mqtt_publish_param param;

	int ret = snprintf(update_delta_topic, sizeof(update_delta_topic),
		       UPDATE_DELTA_TOPIC, client_id_buf);

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

	data_print("publishing: ", data, strlen(data));
	printk("to topic: %s len: %u\n",
	       update_delta_topic,
	       (unsigned int)strlen(update_delta_topic));

	return mqtt_publish(c, &param);
}
//static int jobs_topics_subscribe(struct mqtt_client * c)
static int subscribe(struct mqtt_client * c)
{
	/* Construct topics */
	char jobs_notify_next[JOBS_NOTIFY_TOPIC_LEN + 1];
	char jobs_get_accepted[JOBS_GET_LEN + 1];
	char jobs_get_rejected[JOBS_GET_LEN + 1];
	char jobs_get_jobid_accepted[JOBS_GET_LEN  + 1];
	char jobs_get_jobid_rejected[JOBS_GET_LEN + 1];

	int ret = snprintf(jobs_notify_next, ARRAY_SIZE(jobs_notify_next),
		       JOBS_NOTIFY_TOPIC, client_id_buf);
	if (ret != JOBS_NOTIFY_TOPIC_LEN){
		return -ENOMEM;
	}

	printk("notify next: %s\n", jobs_notify_next);
	ret = snprintf(jobs_get_accepted, ARRAY_SIZE(jobs_get_accepted),
		       JOBS_GET, client_id_buf, "",  "accepted");
	printk("accepted topic: %s\n", jobs_get_accepted);
	ret = snprintf(jobs_get_rejected, ARRAY_SIZE(jobs_get_rejected),
		       JOBS_GET, client_id_buf, "", "rejected");
	printk("rejected topic: %s\n", jobs_get_rejected);
	ret = snprintf(jobs_get_jobid_accepted,
		       ARRAY_SIZE(jobs_get_jobid_accepted),
		       JOBS_GET,
		       client_id_buf,
		       "jobId/",
		       "accepted");
	printk("rejected jobid accepted: %s\n", jobs_get_accepted);
	ret = snprintf(jobs_get_jobid_rejected,
		       ARRAY_SIZE(jobs_get_jobid_rejected),
		       JOBS_GET,
		       client_id_buf,
		       "jobId/",
		       "accepted");
	printk("rejected jobid topic: %s\n", jobs_get_rejected);

	struct mqtt_topic subscribe_topic []= {
		{
			.topic = {
				.utf8 = CONFIG_MQTT_SUB_TOPIC,
				.size = strlen(CONFIG_MQTT_SUB_TOPIC)
			},
			.qos = MQTT_QOS_1_AT_LEAST_ONCE
		},
		{
			.topic = {
				.utf8 = "test/topic",
				.size = strlen("test/topic")
			},
			.qos = MQTT_QOS_1_AT_LEAST_ONCE
		},
		{
			.topic = {
				.utf8 = jobs_notify_next,
				.size = strlen(jobs_notify_next)
			},
			.qos = MQTT_QOS_1_AT_LEAST_ONCE
		},
	};

	const struct mqtt_subscription_list subscription_list = {
		.list = (struct mqtt_topic *)&subscribe_topic,
		.list_count = 3,
		.message_id = 1234
	};

	printk("Subscribing to: %s len %u\n", CONFIG_MQTT_SUB_TOPIC,
	       (unsigned int)strlen(CONFIG_MQTT_SUB_TOPIC));

	return mqtt_subscribe(c, &subscription_list);
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
		int ret = mqtt_read_publish_payload(c, buf, end - buf);

		if (ret < 0) {
			int err;

			if (ret != -EAGAIN) {
				return ret;
			}
			printk("mqtt_read_publish_payload: EAGAIN\n");

			err = poll(&fds, 1, K_SECONDS(CONFIG_MQTT_KEEPALIVE));
			if (err > 0 && (fds.revents & POLLIN) == POLLIN) {
				continue;
			} else {
				return -EIO;
			}
		}

		if (ret == 0) {
			return -EIO;
		}

		buf += ret;
	}

	return 0;
}




extern void dfu_start_thread(const char * hostname, const char * resource_path);

/**@brief MQTT client event handler
*/
void mqtt_evt_handler(struct mqtt_client *const c,
		      const struct mqtt_evt *evt)
{
	int err;

	switch (evt->type) {
	case MQTT_EVT_CONNACK:
		if (evt->result != 0) {
			printk("MQTT connect failed %d\n", evt->result);
			break;
		}

		printk("[%s:%d] MQTT client connected!\n", __func__, __LINE__);
		subscribe(c);
		err = publish_shadow_state(c);
		if(err){
			printk("Unable to update shadow state\n");
		}
		break;

	case MQTT_EVT_DISCONNECT:
		printk("[%s:%d] MQTT client disconnected %d\n", __func__,
		       __LINE__, evt->result);
		break;

	case MQTT_EVT_PUBLISH: {
		const struct mqtt_publish_param *p = &evt->param.publish;

		printk("[%s:%d] MQTT PUBLISH result=%d len=%d\n", __func__,
		       __LINE__, evt->result, p->message.payload.len);
		err = publish_get_payload(c, p->message.payload.len);
		if (err >= 0) {
			data_print("Received: ", payload_buf,
				   p->message.payload.len);
		} else {
			printk("mqtt_read_publish_payload: Failed! %d\n", err);
			printk("Disconnecting MQTT client...\n");

			err = mqtt_disconnect(c);
			if (err) {
				printk("Could not disconnect: %d\n", err);
			}
		}
	} break;

	case MQTT_EVT_PUBACK:
		if (evt->result != 0) {
			printk("MQTT PUBACK error %d\n", evt->result);
			break;
		}

		printk("[%s:%d] PUBACK packet id: %u\n", __func__, __LINE__,
		       evt->param.puback.message_id);
		break;

	case MQTT_EVT_SUBACK:
		if (evt->result != 0) {
			printk("MQTT SUBACK error %d\n", evt->result);
			break;
		}

		printk("[%s:%d] SUBACK packet id: %u\n", __func__, __LINE__,
		       evt->param.suback.message_id);
		break;

	default:
		printk("[%s:%d] default: %d\n", __func__, __LINE__,
		       evt->type);
		break;
	}
}

static void start_dfu(cJSON * json)
{
	cJSON * host = cJSON_GetObjectItemCaseSensitive(json, "hostname");
	if (cJSON_IsString(host) && (host->valuestring != NULL))
	{
		memcpy(hostname, host->valuestring, strlen(host->valuestring));
	}

	cJSON * file = cJSON_GetObjectItemCaseSensitive(json, "file_path");
	if(cJSON_IsString(file) && (file->valuestring != NULL))
	{
		memcpy(file_path, file->valuestring, strlen(file->valuestring));
	}
}

static void jobs_handler(u8_t * json)
{

	dfu_start_thread(hostname, file_path);//, progress_cb);
}

/**@brief Resolves the configured hostname and
 * initializes the MQTT broker structure
 */
static void broker_init(void)
{
	int err;
	struct addrinfo *result;
	struct addrinfo *addr;
	struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM
	};

	err = getaddrinfo(CONFIG_AWS_HOSTNAME, NULL, &hints, &result);
	if (err) {
		printk("ERROR: getaddrinfo failed %d\n", err);

		return;
	}

	addr = result;
	err = -ENOENT;

	/* Look for address of the broker. */
	while (addr != NULL) {
		/* IPv4 Address. */
		if (addr->ai_addrlen == sizeof(struct sockaddr_in)) {
			struct sockaddr_in *broker4 =
				((struct sockaddr_in *)&broker);

			broker4->sin_addr.s_addr =
				((struct sockaddr_in *)addr->ai_addr)
				->sin_addr.s_addr;
			broker4->sin_family = AF_INET;
			broker4->sin_port = htons(8883);
			printk("IPv4 Address found 0x%08x\n",
			       broker4->sin_addr.s_addr);
			break;
		} else {
			printk("ai_addrlen = %u should be %u or %u\n",
			       (unsigned int)addr->ai_addrlen,
			       (unsigned int)sizeof(struct sockaddr_in),
			       (unsigned int)sizeof(struct sockaddr_in6));
		}

		addr = addr->ai_next;
		break;
	}

	/* Free the address. */
	freeaddrinfo(result);
}

static int provision(void)
{
	int err;

	err = tls_credential_add(CONFIG_NRF_CLOUD_SEC_TAG,
				 TLS_CREDENTIAL_CA_CERTIFICATE,
					AmazonRootCA3_pem,
					AmazonRootCA3_pem_len);
	if (err < 0) {
		printk("Failed to register ca certificate: %d",
		       err);
		return err;
	}
	err = tls_credential_add(CONFIG_NRF_CLOUD_SEC_TAG,
				 TLS_CREDENTIAL_PRIVATE_KEY,
				 private_pem_key,
				 private_pem_key_len);
	if (err < 0) {
		printk("Failed to register private key: %d",
		       err);
		return err;
	}
	err = tls_credential_add(CONFIG_NRF_CLOUD_SEC_TAG,
				 TLS_CREDENTIAL_SERVER_CERTIFICATE,
				 public_pem,
				 public_pem_len);
	if (err < 0) {
		printk("Failed to register public certificate: %d",
		       err);
		return err;
	}

	return 0;
}

static int provision_modem_key(void)
{
	static sec_tag_t sec_tag_list[] = {CONFIG_NRF_CLOUD_SEC_TAG};

#if defined(CONFIG_BSD_LIBRARY)
	{
		int err;

		/* Delete certificates */
		nrf_sec_tag_t sec_tag = CONFIG_NRF_CLOUD_SEC_TAG;

		for (nrf_key_mgnt_cred_type_t type = 0; type < 3; type++) {
			err = nrf_inbuilt_key_delete(sec_tag, type);
			printk("nrf_inbuilt_key_delete(%lu, %d) => result=%d",
				sec_tag, type, err);
		}

		/* Provision CA Certificate. */
		err = nrf_inbuilt_key_write(CONFIG_NRF_CLOUD_SEC_TAG,
					NRF_KEY_MGMT_CRED_TYPE_CA_CHAIN,
					AmazonRootCA3_pem,
					AmazonRootCA3_pem_len);
		if (err) {
			printk("NRF_CLOUD_CA_CERTIFICATE err: %d", err);
			return err;
		}

		/* Provision Private Certificate. */
		err = nrf_inbuilt_key_write(
			CONFIG_NRF_CLOUD_SEC_TAG,
			NRF_KEY_MGMT_CRED_TYPE_PRIVATE_CERT,
			private_pem_key,
			private_pem_key_len);
		if (err) {
			printk("NRF_CLOUD_CLIENT_PRIVATE_KEY err: %d", err);
			return err;
		}

		/* Provision Public Certificate. */
		err = nrf_inbuilt_key_write(
			CONFIG_NRF_CLOUD_SEC_TAG,
			NRF_KEY_MGMT_CRED_TYPE_PUBLIC_CERT,
				 public_pem,
				 public_pem_len);
		if (err) {
			printk("NRF_CLOUD_CLIENT_PUBLIC_CERTIFICATE err: %d",
				err);
			return err;
		}
	}
#else
  #error "Missing CONFIG_BSD_LIB"
#endif
}

/* Function to get the client id */
static int client_id_get(char *id)
{
#if !defined(NRF_CLOUD_CLIENT_ID)
#if defined(CONFIG_BSD_LIBRARY)
	int at_socket_fd;
	int bytes_written;
	int bytes_read;
	char imei_buf[IMEI_LEN + 1];
	int ret;

	at_socket_fd = nrf_socket(NRF_AF_LTE, 0, NRF_PROTO_AT);
	__ASSERT_NO_MSG(at_socket_fd >= 0);

	bytes_written = nrf_write(at_socket_fd, "AT+CGSN", 7);
	__ASSERT_NO_MSG(bytes_written == 7);

	bytes_read = nrf_read(at_socket_fd, imei_buf, IMEI_LEN);
	__ASSERT_NO_MSG(bytes_read == IMEI_LEN);
	imei_buf[IMEI_LEN] = 0;

	snprintf(id, CLIENT_ID_LEN + 1, "nrf-%s", imei_buf);

	ret = nrf_close(at_socket_fd);
	__ASSERT_NO_MSG(ret == 0);
#else
#error Missing NRF_CLOUD_CLIENT_ID
#endif /* defined(CONFIG_BSD_LIBRARY) */
#else
	memcpy(id, CONFIG_CLOUD_CLIENT_ID, CLOUD_CLIENT_ID_LEN + 1);
#endif /* !defined(NRF_CLOUD_CLIENT_ID) */

	return 0;
}

/**@brief Initialize the MQTT client structure
*/
static int client_init(struct mqtt_client *client)
{
	mqtt_client_init(client);
	provision();
	broker_init();

	int ret = client_id_get(client_id_buf);
	if (ret != 0) {
		return ret;
	}

	/* MQTT client configuration */
	client->broker = &broker;
	client->evt_cb = mqtt_evt_handler;
	client->client_id.utf8 = client_id_buf;
	client->client_id.size = CLIENT_ID_LEN;
	client->password = NULL;
	client->user_name = NULL;
	client->protocol_version = MQTT_VERSION_3_1_1;

	/* MQTT buffers configuration */
	client->rx_buf = rx_buffer;
	client->rx_buf_size = sizeof(rx_buffer);
	client->tx_buf = tx_buffer;
	client->tx_buf_size = sizeof(tx_buffer);

	/* MQTT transport configuration */
	client->transport.type = MQTT_TRANSPORT_SECURE;

	static sec_tag_t sec_tag_list[] = {CONFIG_NRF_CLOUD_SEC_TAG};
	struct mqtt_sec_config * tls_config = &(client->transport).tls.config;

	tls_config->peer_verify = 2;
	tls_config->cipher_list = NULL;
	tls_config->cipher_count = 0;
	tls_config->sec_tag_count = ARRAY_SIZE(sec_tag_list);
	tls_config->sec_tag_list = sec_tag_list;
	tls_config->hostname = CONFIG_AWS_HOSTNAME;

	return 0;

}

/**@brief Initialize the file descriptor structure used by poll.
*/
static int fds_init(struct mqtt_client *c)
{
	if (c->transport.type == MQTT_TRANSPORT_NON_SECURE) {
		fds.fd = c->transport.tcp.sock;
	} else {
#if defined(CONFIG_MQTT_LIB_TLS)
		fds.fd = c->transport.tls.sock;
#else
		return -ENOTSUP;
#endif
	}

	fds.events = POLLIN;

	return 0;
}

/**@brief Configures modem to provide LTE link. Blocks until link is
 * successfully established.
 */
static void modem_configure(void)
{
#if defined(CONFIG_LTE_LINK_CONTROL)
	if (IS_ENABLED(CONFIG_LTE_AUTO_INIT_AND_CONNECT)) {
		/* Do nothing, modem is already turned on
		 * and connected.
		 */
	} else {
		int err;

		printk("LTE Link Connecting ...\n");
		err = lte_lc_init_and_connect();
		__ASSERT(err == 0, "LTE link could not be established.");
		printk("LTE Link Connected!\n");
	}
#endif
}

void main(void)
{
	int err;
	/* The mqtt client struct */
	struct mqtt_client client;

	printk("The MQTT fota\n");

	modem_configure();

	client_init(&client);

	err = mqtt_connect(&client);
	if (err != 0) {
		printk("ERROR: mqtt_connect %d\n", err);
		return;
	}

	err = fds_init(&client);
	if (err != 0) {
		printk("ERROR: fds_init %d\n", err);
		return;
	}

	while (1) {
		err = poll(&fds, 1, K_SECONDS(CONFIG_MQTT_KEEPALIVE));
		if (err < 0) {
			printk("ERROR: poll %d\n", errno);
			break;
		}

		err = mqtt_live(&client);
		if (err != 0) {
			printk("ERROR: mqtt_live %d\n", err);
			break;
		}

		if ((fds.revents & POLLIN) == POLLIN) {
			err = mqtt_input(&client);
			if (err != 0) {
				printk("ERROR: mqtt_input %d\n", err);
				break;
			}
		}

		if ((fds.revents & POLLERR) == POLLERR) {
			printk("POLLERR\n");
			break;
		}

		if ((fds.revents & POLLNVAL) == POLLNVAL) {
			printk("POLLNVAL\n");
			break;
		}

	}

	printk("Disconnecting MQTT client...\n");

	err = mqtt_disconnect(&client);
	if (err) {
		printk("Could not disconnect MQTT client. Error: %d\n", err);
	}
}
