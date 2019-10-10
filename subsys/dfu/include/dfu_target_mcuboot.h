/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/** @file dfu_target_mcuboot.h
 *
 * @defgroup dfu_target_mcuboot MCUBoot DFU Target
 * @{
 * @brief DFU Target for upgrades performed by MCUBoot
 */

#ifndef DFU_TARGET_MCUBOOT_H__
#define DFU_TARGET_MCUBOOT_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief See if data in buf indicates MCUBoot style upgrade.
 *
 * @retval true if data matches, false otherwise.
 */
bool dfu_target_mcuboot_identify(const void *const buf);

/**
 * @brief Initialize dfu target, perform steps necessary to receive firmware.
 *
 * @param file_size[in] Size of the current file being downloaded.
 *
 * @retval 0 If successful, negative errno otherwise.
 */
int dfu_target_mcuboot_init(size_t file_size);

/**
 * @brief Get offset of firmware
 *
 * @return Offset of firmware
 */
int dfu_target_mcuboot_offset_get(size_t *offset);

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
 * @brief Deinitialize resources and finalize firmware upgrade if successful.

 * @param successful[in] Indicate whether the firmware was successfully recived.
 *
 * @return 0 on success, negative errno otherwise.
 */
int dfu_target_mcuboot_done(bool successful);

#endif /* DFU_TARGET_MCUBOOT_H__ */

/**@} */
