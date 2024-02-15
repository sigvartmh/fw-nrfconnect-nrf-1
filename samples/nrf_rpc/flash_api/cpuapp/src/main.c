/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <nrf_rpc.h>
#include <nrf_rpc/nrf_rpc_ipc.h>
#include <nrf_rpc_cbor.h>

#include "entropy_ser.h"

#define BUFFER_LENGTH 10
LOG_MODULE_REGISTER(appcore, 4);

static uint8_t buffer[BUFFER_LENGTH];

static void entropy_print(const uint8_t *buffer, size_t length)
{
	for (size_t i = 0; i < length; i++) {
		printk("  0x%02x", buffer[i]);
	}

	printk("\n");
}


static void result_callback(int result, uint8_t *buffer, size_t length)
{
	if (result) {
		LOG_ERR("Entropy remote get failed: %d\n", result);
		return;
	}

	entropy_print(buffer, length);
}

int main(void)
{
	int err;

	LOG_INF("Flash API.");

	err = flash_remote_api_init();
	if (err) {
		LOG_ERR("Remote flash driver initialization failed");
		return 0;
	}

	LOG_INF("Flash API initialized.");

	char buff[32] = "this is a 2 recompiled-test";


	err = flash_remote_write(0x10000, buff, 32);
	if (err) {
		LOG_ERR("Remote flash write failed");
		return 0;
	}

	char buff2[32] = "herpa derpa du";

	LOG_INF("Buff addr: 0x%x", (uint32_t)buff2);
	err = flash_remote_read(0x10000, buff2, 32);
	if (err) {
		LOG_ERR("Remote flash read failed");
		return 0;
	}
	LOG_INF("buff2: %s", buff2);

	err = flash_remote_erase(0x10000, 0x1000);
	if (err) {
		LOG_ERR("Remote flash erase failed");
		return 0;
	}

	err = flash_remote_read(0x10000, buff2, 32);
	if (err) {
		LOG_ERR("Remote flash read failed");
		return 0;
	}
	LOG_INF("buff2: %s", buff2);


	while (true) {
		k_sleep(K_MSEC(2000));
	}
}
