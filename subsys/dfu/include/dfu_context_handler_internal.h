/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/** @file
 * @brief header.
 */
#ifndef _DFU_CONTEXT_MANAGER_INTERNAL_H_
#define _DFU_CONTEXT_MANAGER_INTERNAL_H_
#include <zephyr/types.h>

typedef int init_function(void);
typedef int done_function(void);
typedef int write_function(const void *const buf, size_t len);
struct dfu_ctx {
	/** Implement init function */
	init_function *init;
	/** Implement write function */
	write_function *write;
	/** Implement done function */
	done_function *done;
};

#endif /* _DFU_CONTEXT_MANAGER_INTERNAL_H_ */
