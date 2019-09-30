/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */
#ifndef	_DFU_CTX_MODEM_H_
#define _DFU_CTX_MODEM_H_

#include <zephyr/types.h>
#include "dfu_context_handler_internal.h"
int dfu_ctx_modem_init(void);
int dfu_ctx_modem_write(const void *const buf, size_t len);
int dfu_ctx_modem_done(void);

static struct dfu_ctx dfu_ctx_modem = {
	.init = dfu_ctx_modem_init,
	.write = dfu_ctx_modem_write,
	.done = dfu_ctx_modem_done,
};

#endif /* _DFU_CTX_MODEM_H_ */
