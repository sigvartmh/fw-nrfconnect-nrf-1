/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <errno.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <string.h>

#include <nrf_rpc/nrf_rpc_ipc.h>
#include <nrf_rpc_cbor.h>

#include "../../common_ids.h"
#include <zcbor_common.h>
#include <zcbor_decode.h>

#include <zcbor_encode.h>

LOG_MODULE_REGISTER(app_rpc, 4);

NRF_RPC_IPC_TRANSPORT(flash_netcore_api_tr, DEVICE_DT_GET(DT_NODELABEL(ipc0)), "flash_api_net_ept");
NRF_RPC_GROUP_DEFINE(flash_netcore_api, "flash_api_network_core", &flash_netcore_api_tr, NULL, NULL,
		     NULL);

#define CBOR_BUF_FLASH_MSG_SIZE (sizeof(void *) + sizeof(size_t) + sizeof(off_t))

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

int flash_remote_api_init(void)
{
	int result;
	int err;
	struct nrf_rpc_cbor_ctx ctx;

	/* TODO: Check Size */
	NRF_RPC_CBOR_ALLOC(&flash_netcore_api, ctx, CBOR_BUF_FLASH_MSG_SIZE);

	err = nrf_rpc_cbor_cmd(&flash_netcore_api, RPC_COMMAND_FLASH_INIT, &ctx,
			       rsp_error_code_handle, &result);
	if (err < 0) {
		return err;
	}

	return result;
}

static void flash_remote_get_rsp(const struct nrf_rpc_group *group, struct nrf_rpc_cbor_ctx *ctx,
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
	NRF_RPC_CBOR_ALLOC(&flash_netcore_api, *ctx, CBOR_BUF_FLASH_MSG_SIZE);

	if (!zcbor_uint32_put(ctx->zs, (uint32_t)*offset)) {
		return false;
	}

	if (!zcbor_uint32_put(ctx->zs, (uint32_t)ptr)) {
		return false;
	}

	return zcbor_uint32_put(ctx->zs, (uint32_t)*len);
}

int flash_remote_write(off_t offset, const void *data, size_t len)
{
	int err;
	int result;

	struct nrf_rpc_cbor_ctx ctx;

	if (data == NULL || len == 0) {
		return -EINVAL;
	}

	if (!encode_flash_msg(&ctx, &offset, (void *)data, &len)) {
		return -EINVAL;
	}

	err = nrf_rpc_cbor_cmd(&flash_netcore_api, RPC_COMMAND_FLASH_WRITE, &ctx,
			flash_remote_get_rsp, &result);

	LOG_DBG("offset: %x, data_ptr: %p, len: %d", (uint32_t)offset, data, (uint32_t)len);
	if (err) {
		LOG_ERR("Failed to send command: %d", err);
		return -NRF_EINVAL;
	}

	LOG_DBG("Write command sent: %d", result);

	return result;
}

int flash_remote_read(off_t offset, void *buffer, size_t len)
{
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

	err = nrf_rpc_cbor_cmd(&flash_netcore_api, RPC_COMMAND_FLASH_READ, &ctx,
				flash_remote_get_rsp, &result);
	if (err) {
		return -EINVAL;
	}

	LOG_DBG("Read command sent: %d", result);

	return result;
}

int flash_remote_erase(off_t offset, size_t size)
{
	int err;
	int result;

	struct nrf_rpc_cbor_ctx ctx;

	if (!encode_flash_msg(&ctx, &offset, NULL, &size)) {
		return -EINVAL;
	}

	err = nrf_rpc_cbor_cmd(&flash_netcore_api, RPC_COMMAND_FLASH_ERASE, &ctx,
				flash_remote_get_rsp, &result);
	if (err) {
		return -EINVAL;
	}

	LOG_DBG("Erase command sent: %d", result);

	return result;
}

static void err_handler(const struct nrf_rpc_err_report *report)
{
	LOG_ERR("nRF RPC error %d ocurred. See nRF RPC logs for more details.",
	       report->code);
	k_oops();
}

static int serialization_init(void)
{
	int err;

	LOG_INF("Init begin");

	err = nrf_rpc_init(err_handler);
	if (err) {
		LOG_ERR("Something went wrong");
		return -NRF_EINVAL;
	}

	LOG_INF("Init done");
	return 0;
}


SYS_INIT(serialization_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);
