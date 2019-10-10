/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/** @file
 * @brief header.
 */

#ifndef DFU_TARGET_H__
#define DFU_TARGET_H__

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

/** @brief Functions which needs to be supported by all DFU targets.
 */
struct dfu_target {
	bool (*identify)(const void * const buf);
	int (*init)(void);
	int (*offset)(void);
	int (*write)(const void *const buf, size_t len);
	int (*done)(void);
};

/**
 * @brief Find the image type for the buffer of bytes recived. Used to determine
 *	  what dfu target to initialize.
 *
 * @param buf A buffer of bytes which are the start of an binary firmware image.
 * @param len The length of the provided buffer.
 *
 * @return Positive identifier for a supported image type or a negative error
 *	   code identicating reason of failure.
 **/
int dfu_target_img_type(const void *const buf, size_t len);

/**
 * @brief Initialize the resources needed for the specific image type DFU
 *	  target.
 *
 * @param img_type Image type identifier.
 *
 * @return 0 for a supported image type or a negative error
 *	   code identicating reason of failure.
 *
 **/
int dfu_target_init(int img_type);

/**
 * @brief Get offset of firmware
 *
 * @return Offset of firmware
 */
int dfu_target_offset(void);


/**
 * @brief Write the given buffer to the initialized DFU target.
 *
 * @param buf A buffer of bytes which contains part of an binary firmware image.
 * @param len The length of the provided buffer.
 *
 * @return Positive identifier for a supported image type or a negative error
 *	   code identicating reason of failure.
 **/
int dfu_target_write(const void *const buf, size_t len);

/**
 * @brief Deinitialize the resources that were needed for the current DFU
 *	  target.
 *
 * @return 0 for an successful deinitialization or a negative error
 *	   code identicating reason of failure.
 **/
int dfu_target_done(void);

#ifdef __cplusplus
}
#endif

#endif /* DFU_TARGET_H__ */
