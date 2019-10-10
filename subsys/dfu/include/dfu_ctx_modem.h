/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */
#ifndef	_DFU_CTX_MODEM_H_
#define _DFU_CTX_MODEM_H_

#include <zephyr/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <dfu/dfu_context_handler.h>

/**
 * @brief See if data in buf indicates modem upgrade.
 *
 * @retval true if data matches, false otherwise.
 */
bool dfu_ctx_modem_identify(const void *const buf);

/**
 * @brief Initialize dfu context, perform steps necessary to receive firmware.
 *
 * @retval 0 If successful, negative errno otherwise.
 */
int dfu_ctx_modem_init(void);

/**
 * @brief Get offset of firmware
 *
 * @return Offset of firmware
 */
int dfu_ctx_modem_offset(void);

/**
 * @brief Write firmware data.
 *
 * @param buf Pointer to data that should be written.
 * @param len Length of data to write.
 *
 * @return 0 on success, negative errno otherwise.
 */
int dfu_ctx_modem_write(const void *const buf, size_t len);

/**
 * @brief Finalize firmware transfer.
 *
 * @return 0 on success, negative errno otherwise.
 */
int dfu_ctx_modem_done(void);

/** @brief Expose API compatible with dfu_ctx. This is used by
 *	   dfu_context_handler.c.
 */
static struct dfu_ctx dfu_ctx_modem = {
	.identify = dfu_ctx_modem_identify,
	.init = dfu_ctx_modem_init,
	.offset = dfu_ctx_modem_offset,
	.write = dfu_ctx_modem_write,
	.done = dfu_ctx_modem_done,
};

#endif /* _DFU_CTX_MODEM_H_ */

/**@} */
