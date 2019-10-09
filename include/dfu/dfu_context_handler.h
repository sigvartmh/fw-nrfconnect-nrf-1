/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/** @file
 * @brief header.
 */

#ifndef _DFU_CONTEXT_MANAGER_H_
#define _DFU_CONTEXT_MANAGER_H_

/**
 * @defgroup event_manager Event Manager
 * @brief Event Manager
 *
 * @{
 */

#include <zephyr/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Interface which needs to be supported by all DFU contexts.
 */
struct dfu_ctx {
	bool (*identify)(const void * const buf);
	int (*init)(void);
	int (*offset)(void);
	int (*write)(const void *const buf, size_t len);
	int (*done)(void);
};

/**
 * @brief Find the image type for the buffer of bytes recived. Used to determine
 *	  what dfu context to initialize.
 *
 * @param buf A buffer of bytes which are the start of an binary firmware image.
 * @param len The length of the provided buffer.
 *
 * @return Positive identifier for an supported image type or a negative error
 *	   code identicating reason of failure.
 **/
int dfu_ctx_img_type(const void *const buf, size_t len);

/**
 * @brief Initialize the resources needed for the specific image type DFU
 *	  context.
 *
 * @param img_type Image type identifier.
 *
 * @return 0 for an supported image type or a negative error
 *	   code identicating reason of failure.
 *
 **/
int dfu_ctx_init(int img_type);

/**
 * @brief Write the given buffer to the initialized DFU context.
 *
 * @param buf A buffer of bytes which contains part of an binary firmware image.
 * @param len The length of the provided buffer.
 *
 * @return Positive identifier for an supported image type or a negative error
 *	   code identicating reason of failure.
 **/
int dfu_ctx_write(const void *const buf, size_t len);

/**
 * @brief Deinitialize the resources that were needed for the current DFU
 *	  context.
 *
 * @return 0 for an successful deinitialization or a negative error
 *	   code identicating reason of failure.
 **/
int dfu_ctx_done(void);

/**
 * @brief Get offset of firmware
 *
 * @return Offset of firmware
 */
int dfu_ctx_offset(void);

#ifdef __cplusplus
}
#endif

#endif /* _DFU_CONTEXT_MANAGER_H_ */
