/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/*
 * Debug instrumentation: a 32 KiB initialized array that displaces the
 * rest of the .data/.bss sections, mimicking the RAM footprint of the
 * ring tracer (psa_call_trace_ram.c) without wrapping psa_call. Used to
 * bisect which side effect of the tracing wrappers suppresses the
 * IronSide SE psa_call hang.
 */

#include <stdint.h>

__attribute__((used)) uint8_t psa_debug_data_pad[32768] = {1};
