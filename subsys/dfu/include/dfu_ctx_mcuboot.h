/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */
#ifndef	_DFU_CTX_MCUBOOT_H_
#define _DFU_CTX_MCUBOOT_H_

#include <zephyr/types.h>
#include "dfu_context_handler_internal.h"

int dfu_ctx_mcuboot_init(void);
int dfu_ctx_mcuboot_write(const void *const buf, size_t len);
int dfu_ctx_mcuboot_done(void);

static struct dfu_ctx dfu_ctx_mcuboot = {
	.init  = dfu_ctx_mcuboot_init,
	.write = dfu_ctx_mcuboot_write,
	.done  = dfu_ctx_mcuboot_done,
};

#endif /* _DFU_CTX_MCUBOOT_H_ */
