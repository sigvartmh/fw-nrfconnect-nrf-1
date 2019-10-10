/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/** @file dfu_target_mcuboot.h
 *
 * @defgroup dfu_target_mcuboot MCUBoot DFU Target
 * @{
 * @brief DFU Target for updates performed by MCUBoot
 */

#ifndef DFU_TARGET_MCUBOOT_H__
#define DFU_TARGET_MCUBOOT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <dfu/dfu_target.h>

/**
 * @brief See if data in buf indicates MCUBoot style upgrade.
 *
 * @retval true if data matches, false otherwise.
 */
bool dfu_target_mcuboot_identify(const void *const buf);

/**
 * @brief Initialize dfu target, perform steps necessary to receive firmware.
 *
 * @retval 0 If successful, negative errno otherwise.
 */
int dfu_target_mcuboot_init(void);

/**
 * @brief Get offset of firmware
 *
 * @return Offset of firmware
 */
int dfu_target_mcuboot_offset(void);

/**
 * @brief Write firmware data.
 *
 * @param buf Pointer to data that should be written.
 * @param len Length of data to write.
 *
 * @return 0 on success, negative errno otherwise.
 */
int dfu_target_mcuboot_write(const void *const buf, size_t len);

/**
 * @brief Finalize firmware transfer.
 *
 * @return 0 on success, negative errno otherwise.
 */
int dfu_target_mcuboot_done(void);

/** @brief Expose API compatible with dfu_target. This is used by
 *	   dfu_target.c.
 */
struct dfu_target dfu_target_mcuboot = {
	.identify = dfu_target_mcuboot_identify,
	.init  = dfu_target_mcuboot_init,
	.offset = dfu_target_mcuboot_offset,
	.write = dfu_target_mcuboot_write,
	.done  = dfu_target_mcuboot_done,
};

#endif /* DFU_TARGET_MCUBOOT_H__ */

/**@} */
