/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/*
 * Debug instrumentation: traces every psa_call() into a RAM ring buffer
 * via a linker --wrap. Unlike the console tracer (psa_call_trace.c),
 * the per-call overhead is a few dozen memory stores, so the timing of
 * the traced run is essentially unperturbed - intended for capturing
 * the call stream of a run that hangs, to be dumped over the debug
 * probe post-mortem.
 *
 * Each record is written BEFORE dispatch with status set to
 * TRACE_RING_IN_FLIGHT, and the status is patched on return - a record
 * that still holds TRACE_RING_IN_FLIGHT marks the call that never
 * returned.
 */

#include <stdint.h>
#include <stddef.h>
#include <zephyr/kernel.h>

#define TRACE_RING_SIZE      512
#define TRACE_RING_MAGIC     0x50534152 /* "PSAR" */
#define TRACE_RING_IN_FLIGHT 0x7fffffff

struct trace_rec {
	uint32_t seq;
	uint32_t cyc;      /* k_cycle_get_32() at entry */
	uint32_t key_id;   /* iov pack word 0 */
	uint32_t alg;      /* iov pack word 1 */
	uint32_t op_handle;/* iov pack word 2 */
	uint16_t fn_id;    /* iov pack bytes 40..41 */
	uint8_t n_in;
	uint8_t n_out;
	uint32_t iv_addr[2]; /* in_vec[1..2] */
	uint32_t iv_len[2];
	uint32_t ov_addr[2]; /* out_vec[0..1] */
	uint32_t ov_len[2];
	int32_t status;
	uint32_t pad;
};

struct trace_ring {
	uint32_t magic;
	uint32_t pos; /* total records written; index = (pos - 1) % size */
	uint32_t size;
	uint32_t rec_size;
	struct trace_rec rec[TRACE_RING_SIZE];
};

/* ABI-compatible with psa_invec/psa_outvec from psa/client.h. */
struct trace_iovec {
	const void *base;
	size_t len;
};

int32_t __real_psa_call(int32_t handle, int32_t type, const struct trace_iovec *in_vec,
			size_t in_len, struct trace_iovec *out_vec, size_t out_len);

struct trace_ring psa_call_trace_ring = {
	.magic = TRACE_RING_MAGIC,
	.size = TRACE_RING_SIZE,
	.rec_size = sizeof(struct trace_rec),
};

int32_t __wrap_psa_call(int32_t handle, int32_t type, const struct trace_iovec *in_vec,
			size_t in_len, struct trace_iovec *out_vec, size_t out_len)
{
	struct trace_ring *ring = &psa_call_trace_ring;
	struct trace_rec *r = &ring->rec[ring->pos % TRACE_RING_SIZE];
	int32_t status;

	r->seq = ring->pos;
	r->cyc = k_cycle_get_32();
	r->n_in = (uint8_t)in_len;
	r->n_out = (uint8_t)out_len;

	if (in_len > 0 && in_vec[0].len >= 42) {
		const uint8_t *pack = in_vec[0].base;

		r->key_id = ((const uint32_t *)pack)[0];
		r->alg = ((const uint32_t *)pack)[1];
		r->op_handle = ((const uint32_t *)pack)[2];
		r->fn_id = (uint16_t)(pack[40] | (pack[41] << 8));
	} else {
		r->key_id = r->alg = r->op_handle = 0;
		r->fn_id = 0xffff;
	}

	for (int i = 0; i < 2; i++) {
		r->iv_addr[i] = (in_len > (size_t)(i + 1)) ?
			(uint32_t)(uintptr_t)in_vec[i + 1].base : 0;
		r->iv_len[i] = (in_len > (size_t)(i + 1)) ? in_vec[i + 1].len : 0;
		r->ov_addr[i] = (out_len > (size_t)i) ?
			(uint32_t)(uintptr_t)out_vec[i].base : 0;
		r->ov_len[i] = (out_len > (size_t)i) ? out_vec[i].len : 0;
	}

	r->status = TRACE_RING_IN_FLIGHT;
	ring->pos++;

	status = __real_psa_call(handle, type, in_vec, in_len, out_vec, out_len);

	r->status = status;

	return status;
}
