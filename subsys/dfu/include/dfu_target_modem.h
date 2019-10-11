/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/** @file dfu_target_modem.h
 *
 * @defgroup dfu_target_modem Modem DFU Target
 * @{
 * @brief DFU Target for upgrades performed by Modem
 */

#ifndef DFU_TARGET_MODEM_H__
#define DFU_TARGET_MODEM_H__

#ifdef __cplusplus
extern "C" {
#endif

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
int dfu_target_modem_offset(size_t *offset);

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

#endif /* DFU_TARGET_MODEM_H__ */

/**@} */
