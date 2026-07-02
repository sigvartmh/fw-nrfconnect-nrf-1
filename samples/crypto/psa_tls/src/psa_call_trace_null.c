/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/*
 * Debug instrumentation: a passthrough psa_call wrapper that does
 * nothing. Compiles to a tail branch, so the only difference from an
 * uninstrumented build is the text insertion (and the resulting shift
 * of downstream code addresses). Used to bisect which side effect of
 * the tracing wrappers suppresses the IronSide SE psa_call hang.
 */

#include <stdint.h>
#include <stddef.h>

struct trace_iovec {
	const void *base;
	size_t len;
};

int32_t __real_psa_call(int32_t handle, int32_t type, const struct trace_iovec *in_vec,
			size_t in_len, struct trace_iovec *out_vec, size_t out_len);

int32_t __wrap_psa_call(int32_t handle, int32_t type, const struct trace_iovec *in_vec,
			size_t in_len, struct trace_iovec *out_vec, size_t out_len)
{
	return __real_psa_call(handle, type, in_vec, in_len, out_vec, out_len);
}
