#include <zephyr.h>
#include <stdio.h>
#include <net/mqtt_socket.h>
#include <net/socket.h>
#include <dk_buttons_and_leds.h>
#include <misc/util.h>
#include <lte_lc.h>

#if defined(CONFIG_BSD_LIBRARY)
#include "nrf_inbuilt_key.h"
#endif

#define CONFIG_NRF_CLOUD_SEC_TAG 16842753
#define CONFIG_CLIENT_ID "custom-upgradable-client"
#define CONFIG_AWS_HOSTNAME "a25jld2wxwm7qs-ats.iot.eu-central-1.amazonaws.com"
#define CONFIG_AWS_PORT 8883

#include "aws_cert/rootca_rsa2048.h"
#include "aws_cert/private_key.h"
#include "aws_cert/pubcert.h"

struct mqtt_client client;
struct sockaddr_storage sbroker;

/* Forward decleration of event handler */
static void aws_mqtt_evt_handler(struct mqtt_client * client,
				 const struct mqtt_evt *evt);

static int init_broker(const char * hostname,
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
			struct sockaddr_in * broker =(struct sockaddr_in *) &sbroker;

			broker->sin_addr.s_addr =
				((struct sockaddr_in *)addr->ai_addr)
				->sin_addr.s_addr;
			broker->sin_family = AF_INET;
			broker->sin_port = htons(port);

			printk("IPv4 Address 0x%08x\n\r", broker->sin_addr.s_addr);
			return 0;
			break;
		} else if ((addr->ai_addrlen == sizeof(struct sockaddr_in6))) {
			/* IPv6 Address. */
			struct sockaddr_in6 *broker =
				((struct sockaddr_in6 *) &sbroker);

			memcpy(broker->sin6_addr.s6_addr,
				((struct sockaddr_in6 *)addr->ai_addr)
				->sin6_addr.s6_addr,
				sizeof(struct in6_addr));
			broker->sin6_family = AF_INET6;
			broker->sin6_port = htons(port);

			printk("IPv6 Address");
			return 0;
			break;
		} else {
			printk("ai_addrlen = %u should be %u or %u",
				(unsigned int)addr->ai_addrlen,
				(unsigned int)sizeof(struct sockaddr_in),
				(unsigned int)sizeof(struct sockaddr_in6));
			return -1;
		}

		addr = addr->ai_next;
		break;
	}
	return err;
}

struct mqtt_sec_config init_tls_config(const char * hostname)
{

	static sec_tag_t sec_tag_list[] = {CONFIG_NRF_CLOUD_SEC_TAG};
	struct mqtt_sec_config tls_config = {
		.peer_verify = 2,
		.cipher_count = 0,
		.cipher_list = NULL,
		.sec_tag_count = ARRAY_SIZE(sec_tag_list),
		.seg_tag_list = sec_tag_list,
		.hostname = (char *)hostname
	};
	return tls_config;

}

static int init_mqtt_client(struct mqtt_client * client,
			    const char * hostname,
			    unsigned int port)
{
	int err = init_broker(hostname, port);
	__ASSERT(err == 0, "Unable to resolve hostname.");

	struct mqtt_sec_config tls_config = init_tls_config(hostname);

	mqtt_client_init(client);
	client->broker = (struct sockaddr *)&sbroker;
	client->evt_cb = aws_mqtt_evt_handler;
	client->client_id.utf8 = CONFIG_CLIENT_ID;
	client->client_id.size = strlen(CONFIG_CLIENT_ID);
	client->protocol_version = MQTT_VERSION_3_1_1;
	client->password = NULL;
	client->user_name = NULL;
	client->transport.type = MQTT_TRANSPORT_SECURE;

	memcpy(&tls_config, &client->transport.tls.config, sizeof(struct mqtt_sec_config));
	
	return err;

}


static int init_aws(struct mqtt_client * client, const char * hostname, unsigned int port)
{
	int err = init_mqtt_client(client,
			 	   hostname,
			           port);
	__ASSERT(err == 0, "Unable to initialize MQTT Client.");
	err = mqtt_init();
	__ASSERT(err == 0, "Unable to initialize MQTT.");
	return err;
}

int write_tls_keys(void)
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

#define CONFIG_JOBS_TOPIC "test/topic"
/**@brief Function to subscribe to the configured topic
 */
static int subscribe(void)
{
	struct mqtt_topic subscribe_topic = {
		.topic = {
			.utf8 = CONFIG_JOBS_TOPIC,
			.size = strlen(CONFIG_JOBS_TOPIC)
		},
		.qos = MQTT_QOS_1_AT_LEAST_ONCE
	};

	const struct mqtt_subscription_list subscription_list = {
		.list = &subscribe_topic,
		.list_count = 1,
		.message_id = 1234
	};

	printk("Subscribing to: %s len %u\n", CONFIG_MQTT_SUB_TOPIC,
		(unsigned int)strlen(CONFIG_MQTT_SUB_TOPIC));

	return mqtt_subscribe(&client, &subscription_list);
}

/* MQTT event handler */
static void aws_mqtt_evt_handler(struct mqtt_client * const client,
				 const struct mqtt_evt * evt)
{
	switch (evt->type) {
	case MQTT_EVT_CONNACK: {
		printk("MQTT_EVT_CONNACK\n");
		if (evt->result != 0) {
			printk("MQTT connection failed %d\n\r", evt->result);
			break;
		}
		printk("MQTT client connected!\n\r");
		subscribe();
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

#define LEDS_ON_INTERVAL	        K_MSEC(500)
#define LEDS_OFF_INTERVAL	        K_MSEC(500)
#define BUTTON_1			BIT(0)
#define BUTTON_2			BIT(1)
#define SWITCH_1			BIT(2)
#define SWITCH_2			BIT(3)

#define LED_ON(x)			(x)
#define LED_BLINK(x)			((x) << 8)
#define LED_GET_ON(x)			((x) & 0xFF)
#define LED_GET_BLINK(x)		(((x) >> 8) & 0xFF)
/**@brief Initializes buttons and LEDs, using the DK buttons and LEDs
 * library.
 */
static void buttons_leds_init(void)
{
#if defined(CONFIG_DK_LIBRARY)
	int err;

	/*
	err = dk_buttons_init(button_handler);
	if (err) {
		printk("Could not initialize buttons, err code: %d\n", err);
	}
	*/

	err = dk_leds_init();
	if (err) {
		printk("Could not initialize leds, err code: %d\n", err);
	}

	err = dk_set_leds_state(0x00, DK_ALL_LEDS_MSK);
	if (err) {
		printk("Could not set leds state, err code: %d\n", err);
	}
#endif
}

void main(void)
{
	int err;
	static struct mqtt_client client;
	u8_t led_on_mask;
	buttons_leds_init();
	led_on_mask = LED_ON(DK_LED1_MSK);
	#if defined(CONFIG_DK_LIBRARY)
		dk_set_leds(led_on_mask);
	#endif
	err = write_tls_keys();
	led_on_mask = LED_ON(DK_LED2_MSK);
	#if defined(CONFIG_DK_LIBRARY)
		dk_set_leds(led_on_mask);
	#endif
	__ASSERT(err == 0, "Unable to setup TLS keys");
	modem_configure();
	led_on_mask = LED_ON(DK_LED3_MSK);
	#if defined(CONFIG_DK_LIBRARY)
		dk_set_leds(led_on_mask);
	#endif
	init_aws(&client, CONFIG_AWS_HOSTNAME, CONFIG_AWS_PORT);
	led_on_mask = LED_ON(DK_LED4_MSK);
	#if defined(CONFIG_DK_LIBRARY)
		dk_set_leds(led_on_mask);
	#endif
	
	while(true) {
		mqtt_live();
		k_sleep(K_MSEC(10));
		k_cpu_idle();
	}
}
