/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/drivers/flash.h>
#include <zephyr/logging/log.h>
#include <zephyr/ipc/ipc_service.h>
#include <drivers/flash/flash_rpc.h>

#ifndef CONFIG_FLASH_RPC_SYS_INIT
LOG_MODULE_REGISTER(FLASH_RPC, CONFIG_FLASH_RPC_LOG_LEVEL);
#else
LOG_MODULE_DECLARE(FLASH_RPC, CONFIG_FLASH_RPC_LOG_LEVEL);
#endif

#define PROTOCOL_ID 0x5140

struct k_event ept_bonded;

const struct device *ipc0_dev = DEVICE_DT_GET(DT_NODELABEL(ipc0));
struct ipc_ept flash_api_ept0;

static void ept_bound_cb(void *priv)
{
	k_event_set(&ept_bonded, 0x01);
   /* Endpoint bounded */
}

static void recv_data_cb(const void *data, size_t len, void *priv)
{
	if(len != sizeof(int)) {
		LOG_ERR("Invalid data received");
	}
	int *ret = data;
	LOG_INF("Return value %d", ret);
}

static struct ipc_ept_cfg flash_api_ept0_cfg = {
   .name = "flash_api_ept0",
   .cb = {
      .bound    = ept_bound_cb,
      .received = recv_data_cb,
   },
};

#if DT_NODE_HAS_STATUS(DT_INST(0, nordic_rpc_flash_controller), okay)
#define DT_DRV_COMPAT nordic_rpc_flash_controller
#else
#error "Node is not available"
#endif

#define FLASH_RPC_CONTROLLER_NODE DT_INST(0, nordic_rpc_flash_controller)
#define FLASH_RPC_NODE DT_INST_CHILD(0, flash_rpc_0)
#define FLASH_RPC_FLASH_SIZE DT_REG_SIZE(FLASH_RPC_NODE)
#define FLASH_RPC_ERASE_UNIT DT_PROP(FLASH_RPC_NODE, erase_block_size)
#define FLASH_RPC_PROG_UNIT DT_PROP(FLASH_RPC_NODE, write_block_size)
#define FLASH_RPC_FLASH_SIZE DT_REG_SIZE(FLASH_RPC_NODE)

#define FLASH_RPC_PAGE_COUNT (FLASH_RPC_FLASH_SIZE/FLASH_RPC_ERASE_UNIT)

static const struct flash_parameters flash_rpc_parameters = {
	.write_block_size = FLASH_RPC_PROG_UNIT,
	.erase_value = 0xff,
};

#ifndef CONFIG_FLASH_RPC_SYS_INIT
static void err_handler(void *priv)
{
	LOG_ERR("nRF RPC error %d ocurred. See nRF RPC logs for more details.",
	       0);
	k_oops();
}
#endif

#define EPT_BIND_TIMEOUT K_MSEC(100)

int flash_rpc_init(const struct device *dev)
{
	int err;
	int result;
	const struct rpc_flash_config *dev_config = dev->config;

	ARG_UNUSED(dev_config);

#ifndef CONFIG_FLASH_RPC_SYS_INIT
	err = ipc_service_open_instance(ipc0_dev);
	if (err && err != -EALREADY) {
		LOG_ERR("Unable to open IPC instance: %d", err);
		return err;
	}
#endif
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
	
	struct flash_rpc_msg message = {
		.magic = PROTOCOL_ID,
		.cmd = RPC_COMMAND_FLASH_INIT,
	};

	err = ipc_service_send(&flash_api_ept0, &message, sizeof(message));
	if (err < 0) {
		LOG_ERR("Could not send RPC command: %d", err);
		return err;
	}

	return 0;
}

int flash_rpc_read(const struct device *dev, off_t offset, void *buffer, size_t len)
{
	ARG_UNUSED(dev);
	int err;
	int result;

	if (len == 0) {
		return 0;
	}

	if (buffer == NULL) {
		return -EINVAL;
	}
	struct flash_rpc_msg msg = {
		.magic = PROTOCOL_ID,
		.cmd = RPC_COMMAND_FLASH_READ,
		.msg.offset = offset,
		.msg.data = buffer,
		.msg.len = len,
	};

	LOG_DBG("buffer_ptr: %p offset: 0x%lu size: %u", msg.msg.data, msg.msg.offset, msg.msg.len);
	
	err = ipc_service_send(&flash_api_ept0, &msg, sizeof(msg));
	if (err < 0) {
		LOG_ERR("Could not send RPC command: %d", err);
		return err;
	}

	return 0;
}

int flash_rpc_write(const struct device *dev, off_t offset, const void *data, size_t len)
{
	ARG_UNUSED(dev);
	int err;
	int result;

	if (len == 0) {
		return 0;
	}

	if (data == NULL) {
		return -EINVAL;
	}

	LOG_DBG("data_ptr: %p offset: 0x%"PRIu32", size: %"PRIx32, data, (uint32_t)offset, len);
	if (err) {
		LOG_ERR("Failed to send RPC write command: %d", err);
		return -EIO;
	}

	return result;
}

int flash_rpc_erase(const struct device *dev, off_t offset, size_t size)
{
	ARG_UNUSED(dev);
	int err;
	int result;

	if (err) {
		LOG_ERR("Failed to send command: %d", err);
		return -EIO;
	}

	return result;
}

static const struct flash_parameters *flash_rpc_get_parameters(const struct device *dev)
{
	ARG_UNUSED(dev);

	return &flash_rpc_parameters;
}

#ifdef CONFIG_FLASH_PAGE_LAYOUT
static const struct flash_pages_layout flash_rpc_pages_layout = {
	.pages_count = FLASH_RPC_PAGE_COUNT,
	.pages_size = FLASH_RPC_ERASE_UNIT,
};

static void flash_rpc_page_layout(const struct device *dev,
				  const struct flash_pages_layout **layout,
				  size_t *layout_size)
{
	*layout = &flash_rpc_pages_layout;
	*layout_size = 1;
}
#endif

static const struct flash_driver_api flash_driver_rpc_api = {
	.read = flash_rpc_read,
	.write = flash_rpc_write,
	.erase = flash_rpc_erase,
	.get_parameters = flash_rpc_get_parameters,
#ifdef CONFIG_FLASH_PAGE_LAYOUT
	.page_layout = flash_rpc_page_layout,
#endif
};

DEVICE_DT_INST_DEFINE(0, flash_rpc_init, NULL, NULL, NULL, POST_KERNEL,
		      CONFIG_FLASH_RPC_DRIVER_INIT_PRIORITY, &flash_driver_rpc_api);
