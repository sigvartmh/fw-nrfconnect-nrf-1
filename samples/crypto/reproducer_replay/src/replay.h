/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef REPLAY_H_
#define REPLAY_H_

#include <stdint.h>

#define REPLAY_MAX_VEC 4

struct replay_iv {
	uint32_t addr;     /* original buffer address to stage data at */
	uint32_t len;      /* original in_vec length */
	uint32_t blob_off; /* offset of captured content in replay_blob */
	uint32_t dlen;     /* captured content length (<= len) */
};

struct replay_ov {
	uint32_t addr; /* original output buffer address */
	uint32_t len;  /* original out_vec length */
};

struct replay_call {
	uint16_t seq;
	uint32_t handle;
	int32_t type;
	int32_t expected_st;
	uint8_t n_in;
	uint8_t n_out;
	struct replay_iv iv[REPLAY_MAX_VEC];
	struct replay_ov ov[REPLAY_MAX_VEC];
};

extern const uint8_t replay_blob[];
extern const struct replay_call replay_calls[];
extern const uint32_t replay_num_calls;
extern const uint32_t replay_min_addr;
extern const uint32_t replay_max_addr;
extern const uint32_t replay_remap_below;
extern const uint32_t replay_low_base;
extern const uint32_t replay_low_size;
extern const uint32_t replay_guard_addr;

#endif /* REPLAY_H_ */
