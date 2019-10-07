/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */
#ifndef	_DFU_CTX_MCUBOOT_H_
#define _DFU_CTX_MCUBOOT_H_

#include <zephyr/types.h>

/**
 * @brief Initialize dfu context, perform steps necessary for preparing to
 *        receive new firmware.
 *
 * @retval 0 If successful, negative errno otherwise.
 */
int dfu_ctx_mcuboot_init(void);

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

/**
 * @brief Get offset of firmware
 *
 * @return Offset of firmware
 */
int dfu_ctx_mcuboot_offset(void);
#endif /* _DFU_CTX_MCUBOOT_H_ */
