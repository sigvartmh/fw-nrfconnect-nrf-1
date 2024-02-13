/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/drivers/flash.h>
#include <zephyr/logging/log.h>
#include <drivers/flash/flash_rpc.h>

#include <nrf_rpc/nrf_rpc_ipc.h>
#include <nrf_rpc_cbor.h>

#include <zcbor_common.h>
#include <zcbor_decode.h>
#include <zcbor_encode.h>

LOG_MODULE_REGISTER(FLASH_RPC, CONFIG_FLASH_RPC_LOG_LEVEL);

#define CBOR_BUF_FLASH_MSG_SIZE (sizeof(void *) + sizeof(size_t) + sizeof(off_t))

NRF_RPC_IPC_TRANSPORT(flash_rpc_api_tr, DEVICE_DT_GET(DT_NODELABEL(ipc0)), "flash_rpc_api_ept");
NRF_RPC_GROUP_DEFINE(flash_rpc_api, "flash_rpc_api", &flash_rpc_api_tr, NULL, NULL, NULL);

#if DT_NODE_HAS_STATUS(DT_INST(0, nordic_rpc_flash_controller), okay)
#define DT_DRV_COMPAT nordic_rpc_flash_controller
#else
#error "Node is not available"
#endif

#define FLASH_RPC_CONTROLLER_NODE DT_INST(0, nordic_rpc_flash_controller)

void rsp_error_code_handle(const struct nrf_rpc_group *group, struct nrf_rpc_cbor_ctx *ctx,
			   void *handler_data)
{
	int32_t val;

	if (zcbor_int32_decode(ctx->zs, &val)) {
		*(int *)handler_data = (int)val;
	} else {
		*(int *)handler_data = -NRF_EINVAL;
	}
}

static void flash_rpc_get_rsp(const struct nrf_rpc_group *group, struct nrf_rpc_cbor_ctx *ctx,
				 void *handler_data)
{
	int *result = (int *)handler_data;

	if (!zcbor_int32_decode(ctx->zs, result)) {
		LOG_ERR("Unable to decode result");
		*result = -EINVAL;
	}
}

static bool encode_flash_msg(struct nrf_rpc_cbor_ctx *ctx, off_t *offset, void *ptr, size_t *len)
{
	NRF_RPC_CBOR_ALLOC(&flash_rpc_api, *ctx, CBOR_BUF_FLASH_MSG_SIZE);

	if (!zcbor_uint32_put(ctx->zs, (uint32_t)*offset)) {
		return false;
	}

	if (!zcbor_uint32_put(ctx->zs, (uint32_t)ptr)) {
		return false;
	}

	return zcbor_uint32_put(ctx->zs, (uint32_t)*len);
}

int flash_rpc_init(void)
{
	int err;
	int result;
	struct nrf_rpc_cbor_ctx ctx;

	NRF_RPC_CBOR_ALLOC(&flash_rpc_api, ctx, sizeof(int));

	err = nrf_rpc_cbor_cmd(&flash_rpc_api, RPC_COMMAND_FLASH_INIT, &ctx,
			       rsp_error_code_handle, &result);
	if (err < 0) {
		return err;
	}

	return result;
}

int flash_rpc_read(const struct device *dev, off_t offset, void *buffer, size_t len)
{
	ARG_UNUSED(dev);
	int err;
	int result;

	struct nrf_rpc_cbor_ctx ctx;

	if (buffer == NULL || len == 0) {
		return -EINVAL;
	}

	if (!encode_flash_msg(&ctx, &offset, buffer, &len)) {
		return -EINVAL;
	}

	LOG_DBG("offset: %x, buffer_ptr: %p, len: %d", (uint32_t)offset, buffer, (uint32_t)len);

	err = nrf_rpc_cbor_cmd(&flash_rpc_api, RPC_COMMAND_FLASH_READ, &ctx,
				flash_rpc_get_rsp, &result);
	if (err) {
		return -EINVAL;
	}

	LOG_DBG("Read command sent: %d", result);

	return result;
}

int flash_rpc_write(const struct device *dev, off_t offset, const void *data, size_t len)
{
	ARG_UNUSED(dev);
	int err;
	int result;

	struct nrf_rpc_cbor_ctx ctx;

	if (data == NULL || len == 0) {
		return -EINVAL;
	}

	if (!encode_flash_msg(&ctx, &offset, (void *)data, &len)) {
		return -EINVAL;
	}

	err = nrf_rpc_cbor_cmd(&flash_rpc_api, RPC_COMMAND_FLASH_WRITE, &ctx,
			flash_rpc_get_rsp, &result);

	LOG_DBG("offset: %x, data_ptr: %p, len: %d", (uint32_t)offset, data, (uint32_t)len);
	if (err) {
		LOG_ERR("Failed to send command: %d", err);
		return -EINVAL;
	}

	LOG_DBG("Write command sent: %d", result);

	return result;
}

int flash_rpc_erase(const struct device *dev, off_t offset, size_t size)
{
	ARG_UNUSED(dev);
	int err;
	int result;

	struct nrf_rpc_cbor_ctx ctx;

	if (!encode_flash_msg(&ctx, &offset, NULL, &size)) {
		return -EINVAL;
	}

	err = nrf_rpc_cbor_cmd(&flash_rpc_api, RPC_COMMAND_FLASH_ERASE, &ctx,
				flash_rpc_get_rsp, &result);
	if (err) {
		return -EINVAL;
	}

	LOG_DBG("Erase command sent: %d", result);

	return result;
}

static const struct flash_parameters *flash_rpc_get_parameters(const struct device *dev)
{
	return NULL;
}

#ifdef CONFIG_FLASH_PAGE_LAYOUT
static const struct flash_pages_layout flash_rpc_pages_layout = {
	/* TODO Use DTS to define address and region, as in flash simulator */
	.pages_count = 1,
	.pages_size = 0x40000,
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
			CONFIG_FLASH_INIT_PRIORITY, &flash_driver_rpc_api);
