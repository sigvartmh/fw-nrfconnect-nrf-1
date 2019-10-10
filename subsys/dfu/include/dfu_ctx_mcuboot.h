/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/** @file dfu_ctx_mcuboot.h
 *
 * @defgroup dfu_ctx_mcuboot MCUBoot DFU Context Handler
 * @{
 * @brief DFU Context Handler for updates performed by MCUBoot
 */

#ifndef DFU_CTX_MCUBOOT_H__
#define DFU_CTX_MCUBOOT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <dfu/dfu_context_handler.h>

/**
 * @brief See if data in buf indicates MCUBoot style upgrade.
 *
 * @retval true if data matches, false otherwise.
 */
bool dfu_ctx_mcuboot_identify(const void *const buf);

/**
 * @brief Initialize dfu context, perform steps necessary to receive firmware.
 *
 * @retval 0 If successful, negative errno otherwise.
 */
int dfu_ctx_mcuboot_init(void);

/**
 * @brief Get offset of firmware
 *
 * @return Offset of firmware
 */
int dfu_ctx_mcuboot_offset(void);

/**
 * @brief Write firmware data.
 *
 * @param buf Pointer to data that should be written.
 * @param len Length of data to write.
 *
 * @return 0 on success, negative errno otherwise.
 */
int dfu_ctx_mcuboot_write(const void *const buf, size_t len);

/**
 * @brief Finalize firmware transfer.
 *
 * @return 0 on success, negative errno otherwise.
 */
int dfu_ctx_mcuboot_done(void);

/** @brief Expose API compatible with dfu_ctx. This is used by
 *	   dfu_context_handler.c.
 */
struct dfu_ctx dfu_ctx_mcuboot = {
	.identify = dfu_ctx_mcuboot_identify,
	.init  = dfu_ctx_mcuboot_init,
	.offset = dfu_ctx_mcuboot_offset,
	.write = dfu_ctx_mcuboot_write,
	.done  = dfu_ctx_mcuboot_done,
};

#endif /* DFU_CTX_MCUBOOT_H__ */

/**@} */
