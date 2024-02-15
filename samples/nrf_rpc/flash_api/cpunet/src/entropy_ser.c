/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <errno.h>
#include <zephyr/init.h>
#include <zephyr/drivers/entropy.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/logging/log.h>

#include <nrf_rpc/nrf_rpc_ipc.h>
#include <nrf_rpc_cbor.h>

#include <zephyr/device.h>

#include "../../common_ids.h"

LOG_MODULE_REGISTER(net_rpc, 4);

NRF_RPC_IPC_TRANSPORT(flash_netcore_api_tr, DEVICE_DT_GET(DT_NODELABEL(ipc0)), "flash_api_net_ept");
NRF_RPC_GROUP_DEFINE(flash_netcore_api, "flash_api_network_core", &flash_netcore_api_tr, NULL, NULL,
		     NULL);

static const struct device *const flash_controller =
	DEVICE_DT_GET_OR_NULL(DT_CHOSEN(zephyr_flash_controller));

static void rsp_error_code_send(const struct nrf_rpc_group *group, int err_code)
{
	struct nrf_rpc_cbor_ctx ctx;

	NRF_RPC_CBOR_ALLOC(group, ctx, sizeof(int));

	zcbor_int32_put(ctx.zs, err_code);

	nrf_rpc_cbor_rsp_no_err(group, &ctx);
}

static void flash_netcore_api_init_handler(const struct nrf_rpc_group *group,
					   struct nrf_rpc_cbor_ctx *ctx, void *handler_data)
{
	ARG_UNUSED(handler_data);
	nrf_rpc_cbor_decoding_done(group, ctx);

	if (!device_is_ready(flash_controller)) {
		LOG_ERR("Flash Init failure");
		rsp_error_code_send(group, -EINVAL);
		return;
	}

	rsp_error_code_send(group, 0);
}

static bool decode_flash_msg(struct nrf_rpc_cbor_ctx *ctx, off_t *offset, void **ptr, size_t *len)
{
	uint32_t ptr_addr;

	if (!zcbor_uint32_decode(ctx->zs, (uint32_t *)offset)) {
		return false;
	}
	if (zcbor_uint32_decode(ctx->zs, &ptr_addr)) {
		*ptr = (void *)ptr_addr;
	} else {
		return false;
	}
	return zcbor_uint32_decode(ctx->zs, (uint32_t *)len);
}

static void flash_write_handler(const struct nrf_rpc_group *group, struct nrf_rpc_cbor_ctx *ctx,
			void *handler_data)
{
	int err;
	off_t offset;
	size_t len;
	const void *data = NULL;

	if (!decode_flash_msg(ctx, &offset, (void *)&data, &len)) {
		err = -EBADMSG;
		goto error_exit;
	}

	LOG_DBG("offset: %x, data_ptr: %p, len: %d", (uint32_t)offset, data, (uint32_t)len);
	err = flash_write(flash_controller, offset, data, len);

error_exit:
	nrf_rpc_cbor_decoding_done(group, ctx);
	rsp_error_code_send(group, err);
}

static void flash_read_handler(const struct nrf_rpc_group *group, struct nrf_rpc_cbor_ctx *ctx,
		       void *handler_data)
{
	int err;
	size_t len;
	off_t offset;
	void *buffer = NULL;

	if (!decode_flash_msg(ctx, &offset, &buffer, &len)) {
		err = -EBADMSG;
		goto error_exit;
	}

	LOG_DBG("buffer_ptr: %p, offset: 0x%x, len: %d", buffer, (uint32_t)offset, (uint32_t)len);
	err = flash_read(flash_controller, offset, buffer, len);

error_exit:
	nrf_rpc_cbor_decoding_done(group, ctx);
	rsp_error_code_send(group, err);
}

static void flash_erase_handler(const struct nrf_rpc_group *group, struct nrf_rpc_cbor_ctx *ctx,
		       void *handler_data)
{
	int err;
	size_t size;
	off_t offset;
	void *buffer = NULL;

	if (!decode_flash_msg(ctx, &offset, &buffer, &size)) {
		LOG_INF("Decode failure");
		err = -EBADMSG;
		goto error_exit;
	}

	LOG_DBG("offset: 0x%x, size: %d", (uint32_t)offset, (uint32_t)size);
	err = flash_erase(flash_controller, offset, size);
	if (err) {
		LOG_ERR("Erase failed");
	}

error_exit:
	nrf_rpc_cbor_decoding_done(group, ctx);
	rsp_error_code_send(group, err);
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

	err = nrf_rpc_init(err_handler);
	if (err) {
		return -NRF_EINVAL;
	}

	return 0;
}

NRF_RPC_CBOR_CMD_DECODER(flash_netcore_api, flash_init, RPC_COMMAND_FLASH_INIT,
			 flash_netcore_api_init_handler, NULL);

NRF_RPC_CBOR_CMD_DECODER(flash_netcore_api, flash_write,
			 RPC_COMMAND_FLASH_WRITE, flash_write_handler,
			 NULL);

NRF_RPC_CBOR_CMD_DECODER(flash_netcore_api, flash_read,
			 RPC_COMMAND_FLASH_READ, flash_read_handler,
			 NULL);

NRF_RPC_CBOR_CMD_DECODER(flash_netcore_api, flash_erase,
			 RPC_COMMAND_FLASH_ERASE, flash_erase_handler,
			 NULL);


SYS_INIT(serialization_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);
