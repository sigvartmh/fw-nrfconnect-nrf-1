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

//#define CBOR_BUF_SIZE 16
#define CBOR_BUF_SIZE 64 /*Page erase size*/

struct entropy_get_result {
	uint8_t *buffer;
	size_t length;
	int result;
};

enum call_type {
	CALL_TYPE_STANDARD,
	CALL_TYPE_CBK,
	CALL_TYPE_ASYNC,
};

NRF_RPC_IPC_TRANSPORT(flash_netcore_api_tr, DEVICE_DT_GET(DT_NODELABEL(ipc0)), "nrf_rpc_ept");
NRF_RPC_GROUP_DEFINE(flash_netcore_api, "flash_api_network_core", &flash_netcore_api_tr, NULL, NULL,
		     NULL);

static const struct device *const flash_controller =
	DEVICE_DT_GET_OR_NULL(DT_CHOSEN(zephyr_flash_controller));

static void entropy_print(const uint8_t *buffer, size_t length)
{
	for (size_t i = 0; i < length; i++) {
		printk("  0x%02x", buffer[i]);
	}

	printk("\n");
}

static void rsp_error_code_send(const struct nrf_rpc_group *group, int err_code)
{
	struct nrf_rpc_cbor_ctx ctx;

	NRF_RPC_CBOR_ALLOC(group, ctx, CBOR_BUF_SIZE);

	zcbor_int32_put(ctx.zs, err_code);

	nrf_rpc_cbor_rsp_no_err(group, &ctx);
}

static void flash_netcore_api_init_handler(const struct nrf_rpc_group *group,
					   struct nrf_rpc_cbor_ctx *ctx, void *handler_data)
{
	ARG_UNUSED(handler_data);
	LOG_INF("flash Init begin");
	nrf_rpc_cbor_decoding_done(group, ctx);

	if (!device_is_ready(flash_controller)) {
		LOG_ERR("Flash Init failure");
		rsp_error_code_send(group, -NRF_EINVAL);
		return;
	}

	LOG_INF("Flash Init Success");

	rsp_error_code_send(group, 0);
}


NRF_RPC_CBOR_CMD_DECODER(flash_netcore_api, flash_init, RPC_COMMAND_FLASH_INIT,
			 flash_netcore_api_init_handler, NULL);


static void entropy_get_rsp(const struct nrf_rpc_group *group, int err_code, const uint8_t *data,
			    size_t length)
{
	struct nrf_rpc_cbor_ctx ctx;

	NRF_RPC_CBOR_ALLOC(group, ctx, CBOR_BUF_SIZE + length);

	zcbor_int32_put(ctx.zs, err_code);
	zcbor_bstr_encode_ptr(ctx.zs, data, length);

	nrf_rpc_cbor_rsp_no_err(group, &ctx);
}

static void rsp_empty_handler(const struct nrf_rpc_group *group, struct nrf_rpc_cbor_ctx *ctx,
			      void *handler_data)
{
}

static void flash_write_handler(const struct nrf_rpc_group *group, struct nrf_rpc_cbor_ctx *ctx,
			void *handler_data)
{
	LOG_INF("entering flash write handler");
	int err;
	bool ok;
	int offset;
	char buff[100];
	char wr_buff[32] = {0xFF};
	struct zcbor_string zst;
	struct nrf_rpc_cbor_ctx rsp_ctx;
	struct entropy_get_result *result = (struct entropy_get_result *)handler_data;

	err = 0;
	ok = zcbor_uint32_decode(ctx->zs, &offset);
	LOG_INF("Got offset: %x", offset);

	if (!zcbor_bstr_decode(ctx->zs, &zst) && (result->length != zst.len)) {
		LOG_ERR("Bad message");
		err = -NRF_EBADMSG;
		goto error_exit;
	}
	 /*
	if (!ok || result->length < 0 || result->length > sizeof(buffer)) {
		err = -NRF_EBADMSG;
		goto error_exit;
	}
	*/
	LOG_INF("Reading buff");
	LOG_INF("Len buff: %d, len zcbor string: %d", result->length, zst.len);
	for (int i = 0; i < zst.len; i++){
		printk("%c", zst.value[i]);
	}
	printk("\n");
	memcpy(wr_buff, zst.value, zst.len);
	err = flash_write(flash_controller, offset, wr_buff, 32);
	LOG_INF("Flash write err: %d", err);
	flash_read(flash_controller, offset, buff, zst.len);
	LOG_INF("FLASH_READ");
	for (int i = 0; i < zst.len; i++) {
		printk("%c", zst.value[i]);
	}
	printk("\n");

error_exit:
	nrf_rpc_cbor_decoding_done(group, ctx);
	LOG_INF("Decoding done");
	NRF_RPC_CBOR_ALLOC(group, rsp_ctx, 32);
	zcbor_int32_put(rsp_ctx.zs, err);
	nrf_rpc_cbor_rsp_no_err(group, &rsp_ctx);
	return;
}

/*
static void flash_read(const struct nrf_rpc_group *group, struct nrf_rpc_cbor_ctx *ctx,
		       void *handler_data)
{
	int err;
	bool ok;
	size_t addr;
	uint8_t buf[4096];
	struct entropy_get_result *result =
		(struct entropy_get_result *)handler_data;

	ok = zcbor_uint32_decode(ctx->zs, &addr);

	if (!zcbor_bstr_decode(ctx->zs, &zst) && result->length == zst.len) {
		memcpy(buffer, zst.value, zst.len);
		goto error_exit;
	}

	nrf_rpc_cbor_decoding_done(group, ctx);
	if (!ok || length < 0 || length > sizeof(buf)) {
		err = -NRF_EBADMSG;
		goto error_exit;
	}

	struct nrf_rpc_cbor_ctx ctx;

error_exit:
	entropy_get_rsp(group, err, "", 0);
}

NRF_RPC_CBOR_CMD_DECODER(flash_netcore_api, flash_erase,
		RPC_COMMAND_FLASH_ERASE, flash_erase_handler, NULL);

NRF_RPC_CBOR_CMD_DECODER(flash_netcore_api, flash_read, RPC_COMMAND_FLASH_READ, flash_read_handler,
			NULL);
*/

NRF_RPC_CBOR_CMD_DECODER(flash_netcore_api, flash_write,
			 RPC_COMMAND_FLASH_WRITE, flash_write_handler,
			 NULL);

static void entropy_get_result(const struct nrf_rpc_group *group, int err_code, const uint8_t *data,
			       size_t length, enum call_type type)
{
	struct nrf_rpc_cbor_ctx ctx;

	NRF_RPC_CBOR_ALLOC(group, ctx, CBOR_BUF_SIZE + length);

	zcbor_int32_put(ctx.zs, err_code);
	zcbor_bstr_encode_ptr(ctx.zs, data, length);

	if (type == CALL_TYPE_ASYNC) {
		nrf_rpc_cbor_evt_no_err(group,
					RPC_EVENT_ENTROPY_GET_ASYNC_RESULT,
					&ctx);
	} else {
		nrf_rpc_cbor_cmd_no_err(group,
					RPC_COMMAND_ENTROPY_GET_CBK_RESULT,
					&ctx, rsp_empty_handler, NULL);
	}
}


static void entropy_get_handler(const struct nrf_rpc_group *group, struct nrf_rpc_cbor_ctx *ctx,
				void *handler_data)
{
	int err;
	bool ok;
	int length;
	uint8_t buf[64];
	enum call_type type = (enum call_type)handler_data;
	
	err = 0;

	ok = zcbor_int32_decode(ctx->zs, &length);

	nrf_rpc_cbor_decoding_done(group, ctx);

	if (!ok || length < 0 || length > sizeof(buf)) {
		err = -NRF_EBADMSG;
		goto error_exit;
	}
/*
	err = entropy_get_entropy(entropy, buf, length);
	if (!err) {
		entropy_print(buf, length);
	}
	*/

	switch (type) {
	case CALL_TYPE_STANDARD:
		entropy_get_rsp(group, err, buf, length);
		break;

	case CALL_TYPE_CBK:
		entropy_get_result(group, err, buf, length, type);
		rsp_error_code_send(group, err);
		break;

	case CALL_TYPE_ASYNC:
		entropy_get_result(group, err, buf, length, type);
		break;
	}

	return;

error_exit:
	switch (type) {
	case CALL_TYPE_STANDARD:
		entropy_get_rsp(group, err, "", 0);
		break;

	case CALL_TYPE_CBK:
		rsp_error_code_send(group, err);
		break;

	case CALL_TYPE_ASYNC:
		entropy_get_result(group, err, "", 0, type);
		break;
	}
}

NRF_RPC_CBOR_CMD_DECODER(entropy_group, entropy_get,
			 RPC_COMMAND_ENTROPY_GET, entropy_get_handler,
			 (void *)CALL_TYPE_STANDARD);

NRF_RPC_CBOR_CMD_DECODER(entropy_group, entropy_get_cbk,
			 RPC_COMMAND_ENTROPY_GET_CBK, entropy_get_handler,
			 (void *)CALL_TYPE_CBK);

NRF_RPC_CBOR_EVT_DECODER(entropy_group, entropy_get_async,
			 RPC_EVENT_ENTROPY_GET_ASYNC, entropy_get_handler,
			 (void *)CALL_TYPE_ASYNC);


static void err_handler(const struct nrf_rpc_err_report *report)
{
	printk("nRF RPC error %d ocurred. See nRF RPC logs for more details.",
	       report->code);
	k_oops();
}


static int serialization_init(void)
{

	int err;

	printk("Init begin\n");

	err = nrf_rpc_init(err_handler);
	if (err) {
		return -NRF_EINVAL;
	}

	printk("Init done\n");

	return 0;
}


SYS_INIT(serialization_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);
