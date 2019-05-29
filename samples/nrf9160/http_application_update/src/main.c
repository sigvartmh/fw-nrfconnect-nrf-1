/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */
#include <stdint.h>
#include <zephyr.h>
#include <flash.h>
#include <bsd.h>
#include <nrf_socket.h>
#include <gpio.h>
#include <fota_download.h>
#include <dfu/mcuboot.h>

#define LED_PORT	LED0_GPIO_CONTROLLER

static struct		device *gpiob;
static struct		gpio_callback gpio_cb;
static struct k_work	fota_work;


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

/**@brief Start transfer of the file. */
static void app_dfu_transfer_start(struct k_work *unused)
{
	int retval;

	retval = fota_download_start(CONFIG_DOWNLOAD_HOST,
				     CONFIG_DOWNLOAD_FILE);
	if (retval != 0) {
		printk("fota_download_start() failed, err %d",
			retval);
	}

	return;
}
/**@brief Turn on LED0 and LED1 if CONFIG_APPLICATION_VERSION
 * is 2 and LED0 otherwise.
 */
static int led_app_version(void)
{
	struct device *dev;

	dev = device_get_binding(LED_PORT);
	if (dev == 0) {
		printk("Nordic nRF GPIO driver was not found!\n");
		return 1;
	}

	gpio_pin_configure(dev, LED0_GPIO_PIN, GPIO_DIR_OUT);
	gpio_pin_write(dev, LED0_GPIO_PIN, 1);

#if CONFIG_APPLICATION_VERSION == 2
	gpio_pin_configure(dev, LED1_GPIO_PIN, GPIO_DIR_OUT);
	gpio_pin_write(dev, LED1_GPIO_PIN, 1);
#endif
	return 0;
}

void dfu_button_pressed(struct device *gpiob, struct gpio_callback *cb,
			u32_t pins)
{
	k_work_submit(&fota_work);
	gpio_pin_disable_callback(gpiob, SW0_GPIO_PIN);
}

static int dfu_button_init(void)
{
	int err;

	gpiob = device_get_binding(SW0_GPIO_CONTROLLER);
	if (gpiob == 0) {
		printk("Nordic nRF GPIO driver was not found!\n");
		return 1;
	}
	err = gpio_pin_configure(gpiob, SW0_GPIO_PIN,
				 GPIO_DIR_IN | GPIO_INT | GPIO_PUD_PULL_UP |
					 GPIO_INT_EDGE | GPIO_INT_ACTIVE_LOW);
	if (err == 0) {
		gpio_init_callback(&gpio_cb, dfu_button_pressed,
			BIT(SW0_GPIO_PIN));
		err = gpio_add_callback(gpiob, &gpio_cb);
	}
	if (err == 0) {
		err = gpio_pin_enable_callback(gpiob, SW0_GPIO_PIN);
	}
	if (err != 0) {
		printk("Unable to configure SW0_GPIO_PIN!\n");
		return 1;
	}
	return 0;
}


void fota_dl_handler(const struct fota_download_evt *evt)
{
	if (evt->id == FOTA_DOWNLOAD_EVT_DOWNLOAD_CLIENT &&
		evt->dlc_evt->id == DOWNLOAD_CLIENT_EVT_DONE) {
		/* Re-enable button callback */
		gpio_pin_enable_callback(gpiob, SW0_GPIO_PIN);
	}
}

static int application_init(void)
{
	int err;

	k_work_init(&fota_work, app_dfu_transfer_start);

	err = dfu_button_init();
	if (err != 0) {
		return err;
	}

	err = led_app_version();
	if (err != 0) {
		return err;
	}

	err = fota_download_init(fota_dl_handler);
	if (err != 0) {
		return err;
	}

	return 0;
}

void main(void)
{
	int err;

	boot_write_img_confirmed();

	err = application_init();
	if (err != 0) {
		return;
	}

	printk("Press Button 1 to start the FOTA download\n");

	return;
}
