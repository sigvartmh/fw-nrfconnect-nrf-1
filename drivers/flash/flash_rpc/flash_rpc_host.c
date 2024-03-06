/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/logging/log.h>
#include <zephyr/ipc/ipc_service.h>
#include <drivers/flash/flash_rpc.h>

LOG_MODULE_DECLARE(FLASH_RPC, CONFIG_FLASH_RPC_LOG_LEVEL);

const struct device *ipc0_dev = DEVICE_DT_GET(DT_NODELABEL(ipc0));
struct k_event ept_bonded;
struct ipc_ept flash_api_ept0;
struct ipc_ept_cfg flash_api_ept0_;

static void ept_bound_cb(void *priv)
{
	k_event_set(&ept_bonded, 0x01);
   /* Endpoint bounded */
}

static const struct device *const flash_controller =
	DEVICE_DT_GET_OR_NULL(DT_CHOSEN(zephyr_flash_controller));

static void flash_rpc_init_handler(void *handler_data)
{
	ARG_UNUSED(handler_data);

	if (!device_is_ready(flash_controller)) {
		LOG_DBG("Flash RPC init failed");
		//rsp_error_code_send(group, -ENODEV);
		return;
	}

	//rsp_error_code_send(group, 0);
}

static void flash_read_handler(const void *handler_data)
{
	int err;
	const struct flash_rpc_data *data = (struct flash_rpc_msg *)handler_data;

	LOG_DBG("buffer_ptr: %p offset: 0x%lu size: %u", data->data, data->offset, data->len);
	err = flash_read(flash_controller, data->offset, data->data, data->len);
	LOG_INF("flash_read returned %d", err);

error_exit:
	//rsp_error_code_send(group, err);
}

static void flash_write_handler(void *handler_data)
{
	int err;
	off_t offset;
	size_t len;
	const void *data = NULL;

	LOG_DBG("data_ptr: %p offset: 0x%"PRIu32", size: %"PRIx32, data, (uint32_t)offset, len);
	err = flash_write(flash_controller, offset, data, len);

error_exit:
	//rsp_error_code_send(group, err);
}

static void flash_erase_handler(void *handler_data)
{
	int err;
	size_t size;
	off_t offset;
	void *buffer = NULL;

	LOG_DBG("Offset: 0x%"PRIu32", size: %"PRIx32, (uint32_t)offset, size);
	err = flash_erase(flash_controller, offset, size);

error_exit:
	//rsp_error_code_send(group, err);
}

static void recv_data_cb(const void *data, size_t len, void *priv)
{
	if(len != sizeof(struct flash_rpc_msg)) {
		LOG_ERR("Invalid data received");
	}
	const struct flash_rpc_msg *msg = data;
	LOG_INF("Flash cmd: 0x%x", msg->cmd);
	switch (msg->cmd) {
	case RPC_COMMAND_FLASH_INIT:
		LOG_INF("Flash init cmd");
		flash_rpc_init_handler(NULL);
		break;
	case RPC_COMMAND_FLASH_READ:
		LOG_INF("Flash read cmd");
		flash_read_handler(&msg->msg);
		break;
	default:
		LOG_INF("Unknown flash cmd: 0x%x", msg->cmd);
		break;
}
}

struct ipc_ept_cfg flash_api_ept0_cfg = {
   .name = "flash_api_ept0",
   .cb = {
      .bound    = ept_bound_cb,
      .received = recv_data_cb,
   },
};
