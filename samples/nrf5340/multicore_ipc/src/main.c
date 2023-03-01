/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/ipc/ipc_service.h>
#include <string.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(CONFIG_BOARD, LOG_LEVEL_INF);

#define DT_DRV_COMPAT	zephyr_ipc_icmsg

struct data_packet {
	unsigned long cnt;
	unsigned long size;
	unsigned char data[0];
};

static struct ipc_ept_cfg endpoint_cfg = {
	.cb = {
		.bound    = ep_bound,
		.received = ep_recv,
	},
};

K_SEM_DEFINE(bound_sem, 0, 1);
static void endpoint_recive_callback(const void *data, size_t len, void *priv)
{
	int ret;
	ret = ipc_service_hold_rx_buffer(&ept0, (void *)data);
	struct data_packet *packet = (struct data_packet *)data;
	ret = ipc_service_release_rx_buffer(&ept0, (void *)data);
}

static void endpoint_bound_callback(void *priv)
{
	k_sem_give(&bound_sem);
	LOG_INF("Ep bounded");
}

int main(void)
{
	const struct device *ipc0_instance;
	struct ipc_ept endpoint;
	int ret;

	ipc0_instance = DEVICE_DT_GET(DT_NODELABEL(ipc0));
	ret = ipc_service_open_instance(ipc0_instance);
	if ((ret < 0) && (ret != -EALREADY)) {
		LOG_ERR("ipc_service_open_instance() failure");
		return ret;
	}

	ret = ipc_service_register_endpoint(ipc0_instance, &endpoint, &endpoint_cfg);
	if (ret < 0) {
		LOG_ERR("ipc_service_register_endpoint() failure");
		return ret;
	}
	
	k_sem_take(&bound_sem, K_FOREVER);

	return 0;
}
