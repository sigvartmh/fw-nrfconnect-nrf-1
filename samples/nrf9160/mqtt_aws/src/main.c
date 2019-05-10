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

#include "aws_cert/rootca_rsa2048.h"
#include "aws_cert/private_key.h"
#include "aws_cert/pubcert.h"

//static char client_id_buf[NRF_CLOUD_ID_LEN + 1];

static void aws_mqtt_evt_handler(struct mqtt_client *,
				 const struct mqtt_evt *evt);


static void publish(struct mqtt_client * client,
		    const char * topic,
		    const char * data)
{
}

void setup_tls(struct mqtt_client * client)
{
	struct mqtt_sec_config tls_config;
	static sec_tag_t sec_tag_list[] = {CONFIG_NRF_CLOUD_SEC_TAG};
	tls_config.peer_verify = 2;
	tls_config.cipher_count = 0;
	tls_config.cipher_list = NULL;
	tls_config.sec_tag_count = ARRAY_SIZE(sec_tag_list);
	tls_config.seg_tag_list = sec_tag_list;
	tls_config.hostname = CONFIG_AWS_HOSTNAME;
	memcpy(&tls_config,
	       &client->transport.tls.config,
	       sizeof(struct mqtt_sec_config));
}

void resolve_broker_addr(struct mqtt_client * client,
			 const char * hostname,
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
	__ASSERT(err == 0,"getaddrinfo failed %d\n\r could not resolve: %s \n\r",
	         err,
	         hostname);
	addr = result;
	err = -ENOENT;

	/* Look for address of the broker. */
	while (addr != NULL) {
		/* IPv4 Address. */
		if ((addr->ai_addrlen == sizeof(struct sockaddr_in))) {
			struct sockaddr_in *broker =
				((struct sockaddr_in *) &(client->broker));

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
				((struct sockaddr_in6 *) &(client->broker));

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

int aws_mqtt_client_connect(struct mqtt_client * client,
			    const char * hostname,
			    unsigned int port)
{
	mqtt_client_init(client);

	//client->broker = broker_addr;
	resolve_broker_addr(client, hostname, port);
	/*
	struct sockaddr_in broker = &(client->broker);
	printk("IPv4 Address 0x%08x\n\r", broker.sin_addr.s_addr);
	*/
	//printk("Resolved broker with addr 0x%08\n\r", client->broker->sin_addr.s_addr);
	client->evt_cb = aws_mqtt_evt_handler;
	client->client_id.utf8 = CONFIG_CLIENT_ID;
	client->client_id.size = strlen(CONFIG_CLIENT_ID);
	client->protocol_version = MQTT_VERSION_3_1_1;
	client->password = NULL;
	client->user_name = NULL;
	client->transport.type = MQTT_TRANSPORT_SECURE;

	setup_tls(client);
	return mqtt_connect(client);
}

/* MQTT event handler */

static void aws_mqtt_evt_handler(struct mqtt_client * const client,
				 const struct mqtt_evt *evt)
{
	switch (evt->type) {
	case MQTT_EVT_CONNACK: {
		printk("MQTT_EVT_CONNACK\n");
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


static const struct mqtt_topic test_list[] = {
	{
		.topic = {
			.utf8 = "test",
			.size = 5
		},
		.qos = MQTT_QOS_1_AT_LEAST_ONCE
	},
	{
		.topic = {
			.utf8 = "hello_world",
			.size = 12
		},
		.qos = MQTT_QOS_1_AT_LEAST_ONCE
	}
};

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

void main(void)
{
	int err;
	struct mqtt_client client;
	err = setup_tls_keys();
	if(err)
	{
		__ASSERT(err == 0, "Unable to setup TLS keys");
	}
	modem_configure();
	err = aws_mqtt_client_connect(&client, CONFIG_AWS_HOSTNAME, 8883);
	if(err)
	{
		__ASSERT(err == 0, "Unable to connect to %s",
			 CONFIG_AWS_HOSTNAME);
	}
	printk("MQTT AWS example started");

	while(true) {
		mqtt_live();
		k_sleep(K_MSEC(10));
		k_cpu_idle();
	}
}
