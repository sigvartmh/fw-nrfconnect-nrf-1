/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */
#ifndef DFU_TARGET_MODEM_H__
#define DFU_TARGET_MODEM_H__

#include <zephyr/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <dfu/dfu_target.h>

/**
 * @brief See if data in buf indicates modem upgrade.
 *
 * @retval true if data matches, false otherwise.
 */
bool dfu_target_modem_identify(const void *const buf);

/**
 * @brief Initialize dfu target, perform steps necessary to receive firmware.
 *
 * @retval 0 If successful, negative errno otherwise.
 */
int dfu_target_modem_init(void);

/**
 * @brief Get offset of firmware
 *
 * @return Offset of firmware
 */
int dfu_target_modem_offset(void);

/**
 * @brief Write firmware data.
 *
 * @param buf Pointer to data that should be written.
 * @param len Length of data to write.
 *
 * @return 0 on success, negative errno otherwise.
 */
int dfu_target_modem_write(const void *const buf, size_t len);

/**
 * @brief Finalize firmware transfer.
 *
 * @return 0 on success, negative errno otherwise.
 */
int dfu_target_modem_done(void);

/** @brief Expose API compatible with dfu_target. This is used by
 *	   dfu_target.c.
 */
static struct dfu_target dfu_target_modem = {
	.identify = dfu_target_modem_identify,
	.init = dfu_target_modem_init,
	.offset = dfu_target_modem_offset,
	.write = dfu_target_modem_write,
	.done = dfu_target_modem_done,
};

#endif /* DFU_TARGET_MODEM_H__ */

/**@} */
