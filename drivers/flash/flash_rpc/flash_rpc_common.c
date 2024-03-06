/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <zephyr/ipc/ipc_service.h>

LOG_MODULE_REGISTER(FLASH_RPC, CONFIG_FLASH_RPC_LOG_LEVEL);
#define EPT_BIND_TIMEOUT K_MSEC(100)

extern const struct device *ipc0_dev;
extern struct ipc_ept flash_api_ept0;
extern struct ipc_ept_cfg flash_api_ept0_cfg;
extern struct k_event ept_bonded;

static void err_handler(void *priv)
{
	LOG_ERR("nRF RPC error %d ocurred. See nRF RPC logs for more details.",
	       0);
	k_oops();
}

static int serialization_init(void)
{
	int err;

	err = ipc_service_open_instance(ipc0_dev);
	if (err && err != -EALREADY) {
		LOG_ERR("Unable to open IPC instance: %d", err);
		return err;
	}

	k_event_init(&ept_bonded);
	err = ipc_service_register_endpoint(ipc0_dev, &flash_api_ept0, &flash_api_ept0_cfg);
	if (err) {
		LOG_ERR("Registering endpoint failed with %d", err);
		return err;
	}

	if(!k_event_wait(&ept_bonded, 0x01, false, EPT_BIND_TIMEOUT)) {
		LOG_ERR("IPC endpoint bond timeout");
		return -EPIPE;
	}

	return 0;
}

SYS_INIT(serialization_init, POST_KERNEL, CONFIG_FLASH_RPC_SYS_INIT_PRIORITY);
