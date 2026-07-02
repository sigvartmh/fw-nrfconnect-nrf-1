/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/*
 * Debug instrumentation: traces every psa_call() to the console via a
 * linker --wrap. Each request is printed in full BEFORE dispatch (so the
 * final trace lines identify a call that never returns), followed by a
 * RET line when it completes.
 *
 * Format (one record per line, hex without 0x):
 *   PSAC <seq> h=<handle> t=<type> il=<in_len> ol=<out_len>
 *   IV<i> @<base> l=<len> <up to 64 data bytes>
 *   OV<i> @<base> l=<len>
 *   RET <seq> st=<status> ol0=<len> ...
 */

#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

/* ABI-compatible with psa_invec/psa_outvec from psa/client.h, which is
 * not on the application include path.
 */
struct trace_iovec {
	const void *base;
	size_t len;
};

int32_t __real_psa_call(int32_t handle, int32_t type, const struct trace_iovec *in_vec,
			size_t in_len, struct trace_iovec *out_vec, size_t out_len);

static uint32_t trace_seq;

int32_t __wrap_psa_call(int32_t handle, int32_t type, const struct trace_iovec *in_vec,
			size_t in_len, struct trace_iovec *out_vec, size_t out_len)
{
	const uint32_t seq = trace_seq++;
	int32_t status;

	printk("PSAC %u h=%08x t=%d il=%u ol=%u\n", seq, (unsigned int)handle, (int)type,
	       (unsigned int)in_len, (unsigned int)out_len);

	for (size_t i = 0; i < in_len; i++) {
		const uint8_t *data = in_vec[i].base;
		size_t dump_len = MIN(in_vec[i].len, 64);

		printk("IV%u @%08x l=%u ", (unsigned int)i,
		       (unsigned int)(uintptr_t)in_vec[i].base, (unsigned int)in_vec[i].len);
		for (size_t j = 0; j < dump_len; j++) {
			printk("%02x", data[j]);
		}
		printk("\n");
	}

	for (size_t i = 0; i < out_len; i++) {
		printk("OV%u @%08x l=%u\n", (unsigned int)i,
		       (unsigned int)(uintptr_t)out_vec[i].base, (unsigned int)out_vec[i].len);
	}

	status = __real_psa_call(handle, type, in_vec, in_len, out_vec, out_len);

	printk("RET %u st=%d", seq, (int)status);
	for (size_t i = 0; i < out_len; i++) {
		printk(" ol%u=%u", (unsigned int)i, (unsigned int)out_vec[i].len);
	}
	printk("\n");

	return status;
}
