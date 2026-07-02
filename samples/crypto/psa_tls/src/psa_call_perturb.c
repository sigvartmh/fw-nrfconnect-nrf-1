/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/*
 * Debug instrumentation: wraps psa_call with one isolated runtime
 * perturbation, to bisect which side effect of the tracing wrappers
 * suppresses the IronSide SE psa_call hang. Select exactly one via
 * PERTURB_MODE:
 *   1 - short busy loop (pure CPU delay, no memory or peripheral)
 *   2 - 64 bytes of stores to a static buffer (dirties D-cache lines)
 *   3 - one k_cycle_get_32() read (timer peripheral bus access)
 *   4 - read the request data (iov pack and in_vec buffers), as both
 *       tracing wrappers incidentally do
 *   5 - 64 bytes of stores at a rotating offset in a 32 KiB buffer
 *       (replicates the RAM ring tracer's cache eviction pressure)
 *   6 - full D-cache clean+invalidate before each call
 */

#include <stdint.h>
#include <stddef.h>
#include <zephyr/kernel.h>
#include <zephyr/cache.h>

#ifndef PERTURB_MODE
#error "PERTURB_MODE must be defined"
#endif

struct trace_iovec {
	const void *base;
	size_t len;
};

int32_t __real_psa_call(int32_t handle, int32_t type, const struct trace_iovec *in_vec,
			size_t in_len, struct trace_iovec *out_vec, size_t out_len);

#if PERTURB_MODE == 2
static uint8_t perturb_buf[64];
#endif
#if PERTURB_MODE == 3
volatile uint32_t perturb_cyc;
#endif
#if PERTURB_MODE == 4
volatile uint32_t perturb_sum;
#endif
#if PERTURB_MODE == 5
static uint8_t perturb_ring[32768];
static uint32_t perturb_pos;
#endif

int32_t __wrap_psa_call(int32_t handle, int32_t type, const struct trace_iovec *in_vec,
			size_t in_len, struct trace_iovec *out_vec, size_t out_len)
{
#if PERTURB_MODE == 1
	for (volatile int i = 0; i < 100; i++) {
	}
#elif PERTURB_MODE == 2
	for (unsigned int i = 0; i < sizeof(perturb_buf); i++) {
		perturb_buf[i] = (uint8_t)(i + in_len);
	}
#elif PERTURB_MODE == 3
	perturb_cyc = k_cycle_get_32();
#elif PERTURB_MODE == 4
	uint32_t sum = 0;

	for (size_t i = 0; i < in_len; i++) {
		const uint8_t *p = in_vec[i].base;
		size_t n = in_vec[i].len < 64 ? in_vec[i].len : 64;

		for (size_t j = 0; j < n; j++) {
			sum += p[j];
		}
	}
	perturb_sum = sum;
#elif PERTURB_MODE == 5
	uint8_t *rec = &perturb_ring[perturb_pos % sizeof(perturb_ring)];

	for (int i = 0; i < 64; i++) {
		rec[i] = (uint8_t)(i + in_len);
	}
	perturb_pos += 64;
#elif PERTURB_MODE == 6
	sys_cache_data_flush_and_invd_all();
#endif

	return __real_psa_call(handle, type, in_vec, in_len, out_vec, out_len);
}
