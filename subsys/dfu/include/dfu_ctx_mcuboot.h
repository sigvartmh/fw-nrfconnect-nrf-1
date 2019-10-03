/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */
#ifndef	_DFU_CTX_MCUBOOT_H_
#define _DFU_CTX_MCUBOOT_H_

#include <zephyr/types.h>

int dfu_ctx_mcuboot_init(void);

int dfu_ctx_mcuboot_write(const void *const buf, size_t len);

int dfu_ctx_mcuboot_done(void);

#endif /* _DFU_CTX_MCUBOOT_H_ */
