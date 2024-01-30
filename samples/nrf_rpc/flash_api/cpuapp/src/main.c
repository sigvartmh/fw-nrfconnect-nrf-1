/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>

#include <nrf_rpc.h>
#include <nrf_rpc/nrf_rpc_ipc.h>
#include <nrf_rpc_cbor.h>

#include "entropy_ser.h"

#define BUFFER_LENGTH 10

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
		printk("Entropy remote get failed: %d\n", result);
		return;
	}

	entropy_print(buffer, length);
}

NRF_RPC_IPC_TRANSPORT(flash_netcore_api_tr, DEVICE_DT_GET(DT_NODELABEL(ipc0)), "nrf_rpc_ept");
NRF_RPC_GROUP_DEFINE(flash_netcore_api, "flash_api_network_core", &flash_netcore_api_tr, NULL, NULL,
		     NULL);

#include "../../common_ids.h"
#define CBOR_BUF_SIZE 16
void rsp_error_code_handle_main(const struct nrf_rpc_group *group, struct nrf_rpc_cbor_ctx *ctx,
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

	NRF_RPC_CBOR_ALLOC(&flash_netcore_api, ctx, CBOR_BUF_SIZE);

	err = nrf_rpc_cbor_cmd(&flash_netcore_api, RPC_COMMAND_FLASH_INIT, &ctx,
			       rsp_error_code_handle_main, &result);
	if (err < 0) {
		return err;
	}

	return result;
}

int main(void)
{
	int err;

	printk("Entropy sample started[APP Core].\n");

	err = flash_remote_api_init();
	if (err) {
		printk("Remote entropy driver initialization failed\n");
		return 0;
	}

	printk("Remote init send\n");

	while (true) {
		k_sleep(K_MSEC(2000));

		err = entropy_remote_get(buffer, sizeof(buffer));
		if (err) {
			printk("Entropy remote get failed: %d\n", err);
			continue;
		}

		entropy_print(buffer, ARRAY_SIZE(buffer));

		k_sleep(K_MSEC(2000));

		err = entropy_remote_get_inline(buffer, sizeof(buffer));
		if (err) {
			printk("Entropy remote get failed: %d\n", err);
			continue;
		}

		entropy_print(buffer, ARRAY_SIZE(buffer));

		k_sleep(K_MSEC(2000));

		err = entropy_remote_get_async(sizeof(buffer), result_callback);
		if (err) {
			printk("Entropy remote get async failed: %d\n", err);
			continue;
		}

		k_sleep(K_MSEC(2000));

		err = entropy_remote_get_cbk(sizeof(buffer), result_callback);
		if (err) {
			printk("Entropy remote get callback failed: %d\n", err);
			continue;
		}
	}

}
