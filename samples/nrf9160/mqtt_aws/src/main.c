#include <zephyr.h>
#include <stdio.h>
#include <net/mqtt_socket.h>
#include <net/socket.h>
#include <misc/util.h>
#include <lte_lc.h>

#if defined(CONFIG_BSD_LIBRARY)
#include "nrf_inbuilt_key.h"
#endif

#define CONFIG_NRF_CLOUD_SEC_TAG 16842753
#define CONFIG_CLIENT_ID "custom-upgradable-client"
#define CONFIG_AWS_HOSTNAME "a25jld2wxwm7qs-ats.iot.eu-central-1.amazonaws.com"
#define client_name "test-client-23"

#include "aws_cert/rootca_rsa2048.h"
#include "aws_cert/private_key.h"
#include "aws_cert/pubcert.h"

//static char client_id_buf[NRF_CLOUD_ID_LEN + 1];


static void aws_mqtt_evt_handler(struct mqtt_client * client,
				 const struct mqtt_evt *evt);


static struct aws {
	struct mqtt_sec_config tls_config;
	struct mqtt_client client;
	struct sockaddr_storage broker;
} aws;

static void publish(struct mqtt_client * client,
		    const char * topic,
		    const char * data)
{
}

static int setup_tls()
{
	static sec_tag_t sec_tag_list[] = {CONFIG_NRF_CLOUD_SEC_TAG};

	aws.tls_config.peer_verify = 2;
	aws.tls_config.cipher_count = 0;
	aws.tls_config.cipher_list = NULL;
	aws.tls_config.sec_tag_count = ARRAY_SIZE(sec_tag_list);
	aws.tls_config.seg_tag_list = sec_tag_list;
	aws.tls_config.hostname = CONFIG_AWS_HOSTNAME;

	return mqtt_init();
}

int resolve_broker_addr(const char * hostname,
			unsigned int port)
{
	int err;
	struct addrinfo *result;
	struct addrinfo *addr;
	struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM
	};

	err = getaddrinfo(hostname, NULL, &hints, &result);

	if (err) {
		printk("getaddrinfo failed %d\n\r could not resolve: %s \n\r",
		       err,
		       hostname);

		return err;
	}

	addr = result;
	err = -ENOENT;

	/* Look for address of the broker. */
	while (addr != NULL) {
		/* IPv4 Address. */
		if ((addr->ai_addrlen == sizeof(struct sockaddr_in))) {
			struct sockaddr_in * broker =(struct sockaddr_in *) &aws.broker;

			broker->sin_addr.s_addr =
				((struct sockaddr_in *)addr->ai_addr)
				->sin_addr.s_addr;
			broker->sin_family = AF_INET;
			broker->sin_port = htons(port);

			printk("IPv4 Address 0x%08x\n\r", broker->sin_addr.s_addr);
			break;
		} else if ((addr->ai_addrlen == sizeof(struct sockaddr_in6))) {
			/* IPv6 Address. */
			struct sockaddr_in6 *broker =
				((struct sockaddr_in6 *) &aws.broker);

			memcpy(broker->sin6_addr.s6_addr,
				((struct sockaddr_in6 *)addr->ai_addr)
				->sin6_addr.s6_addr,
				sizeof(struct in6_addr));
			broker->sin6_family = AF_INET6;
			broker->sin6_port = htons(port);

			printk("IPv6 Address");
			break;
		} else {
			printk("ai_addrlen = %u should be %u or %u",
				(unsigned int)addr->ai_addrlen,
				(unsigned int)sizeof(struct sockaddr_in),
				(unsigned int)sizeof(struct sockaddr_in6));
		}

		addr = addr->ai_next;
		break;
	}
}
int aws_mqtt_client_connect(const char * hostname,
			    unsigned int port)
{
	mqtt_client_init(&aws.client);
	aws.client.broker = (struct sockaddr *)&aws.broker;
	aws.client.evt_cb = aws_mqtt_evt_handler;
	aws.client.client_id.utf8 = CONFIG_CLIENT_ID;
	aws.client.client_id.size = strlen(CONFIG_CLIENT_ID);
	aws.client.protocol_version = MQTT_VERSION_3_1_1;
	aws.client.password = NULL;
	aws.client.user_name = NULL;
	aws.client.transport.type = MQTT_TRANSPORT_SECURE;

	struct mqtt_sec_config *tls_config = &aws.client.transport.tls.config;
	memcpy(tls_config, &aws.tls_config, sizeof(struct mqtt_sec_config));

	return mqtt_connect(&aws.client);
}

static int aws_provision(void)
{
	static sec_tag_t sec_tag_list[] = {CONFIG_NRF_CLOUD_SEC_TAG};

	aws.tls_config.peer_verify = 2;
	aws.tls_config.cipher_count = 0;
	aws.tls_config.cipher_list = NULL;
	aws.tls_config.sec_tag_count = ARRAY_SIZE(sec_tag_list);
	aws.tls_config.seg_tag_list = sec_tag_list;
	aws.tls_config.hostname = CONFIG_AWS_HOSTNAME;
	return 0;
}

int aws_mqtt_connect(void)
{
	mqtt_client_init(&aws.client);
	aws.client.broker = (struct sockaddr *)&aws.broker;
	aws.client.evt_cb = aws_mqtt_evt_handler;
	aws.client.client_id.utf8 = client_name;
	aws.client.client_id.size = strlen(client_name);
	aws.client.protocol_version = MQTT_VERSION_3_1_1;
	aws.client.password = NULL;
	aws.client.user_name = NULL;
	aws.client.transport.type = MQTT_TRANSPORT_SECURE;

	struct mqtt_sec_config *tls_config = &aws.client.transport.tls.config;

	memcpy(tls_config, &aws.tls_config, sizeof(struct mqtt_sec_config));

	return mqtt_connect(&aws.client);
}

#define AWS_PORT 8883
#define NRF_CLOUD_AF_FAMILY AF_INET
int aws_connect(void)

{
	printk("Connecting to AWS\n\r");
	int err;
	struct addrinfo *result;
	struct addrinfo *addr;
	struct addrinfo hints = {
		.ai_family = NRF_CLOUD_AF_FAMILY,
		.ai_socktype = SOCK_STREAM
	};

	err = getaddrinfo(CONFIG_AWS_HOSTNAME, NULL, &hints, &result);
	if (err) {
		printk("getaddrinfo failed %d\n\r", err);

		return err;
	}

	addr = result;
	err = -ENOENT;

	/* Look for address of the broker. */
	while (addr != NULL) {
		/* IPv4 Address. */
		if ((addr->ai_addrlen == sizeof(struct sockaddr_in)) &&
		    (NRF_CLOUD_AF_FAMILY == AF_INET)) {
			struct sockaddr_in *broker =
				((struct sockaddr_in *)&aws.broker);

			broker->sin_addr.s_addr =
				((struct sockaddr_in *)addr->ai_addr)
				->sin_addr.s_addr;
			broker->sin_family = AF_INET;
			broker->sin_port = htons(AWS_PORT);

			printk("IPv4 Address 0x%08x\n\r", broker->sin_addr.s_addr);
			err = aws_mqtt_connect();
			return err;
			break;
		} else if ((addr->ai_addrlen == sizeof(struct sockaddr_in6)) &&
			   (NRF_CLOUD_AF_FAMILY == AF_INET6)) {
			/* IPv6 Address. */
			struct sockaddr_in6 *broker =
				((struct sockaddr_in6 *)&aws.broker);

			memcpy(broker->sin6_addr.s6_addr,
				((struct sockaddr_in6 *)addr->ai_addr)
				->sin6_addr.s6_addr,
				sizeof(struct in6_addr));
			broker->sin6_family = AF_INET6;
			broker->sin6_port = htons(AWS_PORT);

			printk("IPv6 Address");
			err = aws_mqtt_connect();
			return err;
			break;
		} else {
			printk("ai_addrlen = %u should be %u or %u",
				(unsigned int)addr->ai_addrlen,
				(unsigned int)sizeof(struct sockaddr_in),
				(unsigned int)sizeof(struct sockaddr_in6));
		}

		addr = addr->ai_next;
		break;
	}

	/* Free the address. */
	freeaddrinfo(result);

	return err;
}
#define CONFIG_SUB_TOPIC "/test/"


int please_subscribe(void){
	struct mqtt_topic test_list = {
		.topic = {
			.utf8 = CONFIG_SUB_TOPIC,
			.size = strlen(CONFIG_SUB_TOPIC) 
		},
		.qos = MQTT_QOS_1_AT_LEAST_ONCE
	};
	const struct mqtt_subscription_list subscription_list = {
		.list = &test_list,
		.list_count = 1,
		.message_id = 1234
	};
	printk("Subscribing to: %s len %u\n", CONFIG_SUB_TOPIC,
		(unsigned int)strlen(CONFIG_SUB_TOPIC));

	return mqtt_subscribe(&aws.client, &subscription_list);
}
/* MQTT event handler */

static void aws_mqtt_evt_handler(struct mqtt_client * const client,
				 const struct mqtt_evt * evt)
{
	int err;
	switch (evt->type) {
	case MQTT_EVT_CONNACK: {
		printk("MQTT_EVT_CONNACK\n");
		if (evt->result != 0) {
			printk("MQTT connection failed %d\n\r", evt->result);
			break;
		}
		printk("MQTT client connected!\n\r");
		please_subscribe();
		break;
	}
	case MQTT_EVT_PUBLISH: {
		const struct mqtt_publish_param *p = &evt->param.publish;

		printk("MQTT_EVT_PUBLISH: id=%d, len=%d",
		       p->message_id,
		       p->message.payload.len);

		if (p->message.topic.qos == MQTT_QOS_1_AT_LEAST_ONCE) {
			const struct mqtt_puback_param ack = {
				.message_id = p->message_id
			};
			mqtt_publish_qos1_ack(client, &ack);
		}
	}
	case MQTT_EVT_SUBACK: {
		printk("MQTT_EVT_SUBACK: id=%d, result=%d",
		       evt->param.suback.message_id,
		       evt->result);
		break;
	}
	case MQTT_EVT_UNSUBACK: {
		printk("MQTT_EVT_UNSUBACK");
		break;
	}
	case MQTT_EVT_DISCONNECT: {
		printk("MQTT_EVT_DISCONNECT: result=%d", evt->result);
		break;
	}
	default: {
		break;
	}
	}
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

int setup_tls_keys(void)
{
	int err;
	#if defined(CONFIG_BSD_LIBRARY)
	nrf_sec_tag_t sec_tag = CONFIG_NRF_CLOUD_SEC_TAG;
	for (nrf_key_mgnt_cred_type_t type = 0; type < 5; type++) {
		err = nrf_inbuilt_key_delete(sec_tag, type);
		printk("nrf_inbuilt_key_delete(%u, %d) => result=%d\n\r",
		       sec_tag, type, err);
	}

	/* Add CA certificate */
	err = nrf_inbuilt_key_write(CONFIG_NRF_CLOUD_SEC_TAG,
					NRF_KEY_MGMT_CRED_TYPE_CA_CHAIN,
					AmazonRootCA1_pem,
					AmazonRootCA1_pem_len);
	if(err) {
		printk("Could not add CA certificate err :%d\n\r", err);
		return err;
	}
	/* Provision Private Certificate. */
	err = nrf_inbuilt_key_write(CONFIG_NRF_CLOUD_SEC_TAG,
				    NRF_KEY_MGMT_CRED_TYPE_PRIVATE_CERT,
				    private_pem_key,
				    private_pem_key_len);
	if (err) {
		printk("Could not add private key err :%d\n\r", err);
		return err;
	}
	/* Provision Public Certificate. */
	err = nrf_inbuilt_key_write(CONFIG_NRF_CLOUD_SEC_TAG,
				    NRF_KEY_MGMT_CRED_TYPE_PUBLIC_CERT,
				    public_pem,
				    public_pem_len);
	if (err) {
		printk("Could not add public cert err :%d\n\r", err);
		return err;
	}
	#endif

	return 0;
}
int aws_init(void)
{
	int err;
	err = aws_provision();
	if (err) {
		return err;
	}

	return mqtt_init();
}


static u32_t pub_data(const char * data, u8_t data_len, u8_t qos)
{
	struct mqtt_publish_param publish = {
		.message.topic.qos = qos,
		.message.topic.topic.size = strlen("hello_world/") -1,
		.message.topic.topic.utf8 = "hello_world/",
		.message.payload.data = (u8_t *) data,
		.message.payload.len = data_len,
		.message_id = 1234
	};
	return mqtt_publish(&aws.client, &publish);
}

void main(void)
{
	int err;
	err = setup_tls_keys();
	if(err)
	{
		__ASSERT(err == 0, "Unable to setup TLS keys");
	}
	modem_configure();
	/*
	err = setup_tls();
	if(err) {
		__ASSERT(err == 0, "Unable to setup mqtt");
	}

	err = aws_mqtt_client_connect(CONFIG_AWS_HOSTNAME, 8883);
	if(err)
	{
		__ASSERT(err == 0, "Unable to connect to %s",
			 CONFIG_AWS_HOSTNAME);
	}
	*/
	aws_init();
	err = aws_connect();
	if(err)
	{
		__ASSERT(err == 0, "Unable to connect to %s",
			 CONFIG_AWS_HOSTNAME);
	}


	while(true) {
		mqtt_live();
		k_sleep(K_MSEC(10));
		k_cpu_idle();
	}
}
