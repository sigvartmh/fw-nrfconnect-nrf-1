/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/*
 * Minimal reproducer for the IronSide SE psa_call hang observed in the
 * psa_tls sample on nrf54h20dk/nrf54h20/cpuapp during a TLS 1.3
 * handshake with TLS_AES_256_GCM_SHA384 (no networking required).
 *
 * The hanging call is psa_hash_finish() on a clone of the SHA-384
 * handshake-transcript hash operation, issued while mbedtls processes
 * the server Finished message. In the captured hang the request sits in
 * IPC slot 0 (0x2f88fb80) with status 6 (IRONSIDE_SE_CALL_STATUS_REQ),
 * function_id 0x305 (hash finish), in_vec[0] = 64-byte iovec pack,
 * out_vec[0] = 4-byte operation handle at a stack address with
 * addr % 32 == 0x0c, out_vec[1] = 48-byte digest at addr % 32 == 0x18.
 *
 * This sample replays the same PSA call sequence: a long-running
 * multipart SHA-384 operation updated with handshake-message-sized
 * chunks, with clone+finish transcript extractions at the points where
 * TLS 1.3 needs the transcript hash, using the same stack layout for
 * the finish output vectors.
 *
 * On a hang, the console shows the last completed iteration, and these
 * globals can be read from a debug probe while the core runs:
 *   repro_iteration - current iteration (1-based)
 *   repro_step      - transcript extractions attempted this iteration
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <psa/crypto.h>

LOG_MODULE_REGISTER(repro_sha384, LOG_LEVEL_INF);

#define NUM_ITERATIONS 10000

/* Additional PSA traffic from the TLS 1.3 handshake, added stepwise to
 * find the ingredient that arms the hang:
 *   0 - hash operations only
 *   1 - + HKDF-SHA-384 key schedule (extract+expand) after each
 *       transcript extraction
 *   2 - + ECDHE key generation and raw key agreement (per iteration)
 *   3 - + AES-256-GCM record decrypt before each encrypted message
 */
#ifndef PREAMBLE_LEVEL
#define PREAMBLE_LEVEL 1
#endif

/* Progress markers readable from a debug probe while the core runs. */
volatile uint32_t repro_iteration;
volatile uint32_t repro_step;
volatile uint32_t repro_misalign;

/* Handshake message sizes hashed into the TLS 1.3 transcript in the
 * failing run: ClientHello, ServerHello, EncryptedExtensions,
 * Certificate, CertificateVerify, server Finished.
 */
static const size_t transcript_msg_sizes[] = { 213, 155, 32, 442, 82, 56 };

/* Transcript extractions (clone+finish), indexed like
 * transcript_msg_sizes: after ServerHello (handshake secrets), after
 * Certificate (CertificateVerify check), after CertificateVerify
 * (server Finished check - the call that hung), after server Finished
 * (client Finished / master secret).
 */
static const bool extract_after[] = { false, true, false, true, true, true };

static uint8_t msg_buf[512];

/* Replicates the stack layout around the hanging psa_hash_finish(): the
 * 4-byte operation handle (out_vec[0]) at offset 0x0c of a 32-byte
 * cache line whose other words hold live app data, and the digest
 * (out_vec[1]) at offset 0x38.
 */
struct finish_layout {
	uint32_t live[3];
	psa_hash_operation_t op;
	uint8_t pad[0x38 - 0x0c - sizeof(psa_hash_operation_t)];
	uint8_t digest[56];
};

BUILD_ASSERT(offsetof(struct finish_layout, op) == 0x0c);
BUILD_ASSERT(offsetof(struct finish_layout, digest) == 0x38);

static psa_status_t transcript_extract(const psa_hash_operation_t *transcript,
				       uint32_t misalign, uint8_t *digest_out)
{
	struct finish_layout l __aligned(32);
	size_t hash_len;
	psa_status_t status;

	memset(&l, 0, sizeof(l));
	/* Live app-owned words sharing the cache line with op, like the
	 * return addresses found there in the captured hang.
	 */
	l.live[0] = 0x0e048e85;
	l.live[1] = (uint32_t)(uintptr_t)&l;
	l.live[2] = 3;

	status = psa_hash_clone(transcript, &l.op);
	if (status != PSA_SUCCESS) {
		LOG_ERR("psa_hash_clone failed: %d", status);
		return status;
	}

	repro_step++;
	/* misalign values 1-3 make out_vec[1].base % 4 != 0, which routes
	 * the digest through the SSF client bounce-buffer path; 0 and 4 stay
	 * on the in-place path with different mod-8 placement.
	 */
	status = psa_hash_finish(&l.op, l.digest + misalign, 48, &hash_len);
	if (status != PSA_SUCCESS) {
		LOG_ERR("psa_hash_finish failed: %d (misalign %u)", status, misalign);
		return status;
	}

	memcpy(digest_out, l.digest + misalign, 48);

	return status;
}

#if PREAMBLE_LEVEL >= 1
/* One HKDF-Extract + HKDF-Expand round with SHA-384, as done by the
 * mbedtls TLS 1.3 key schedule after each transcript extraction.
 */
static psa_status_t key_schedule_hkdf(const uint8_t *ikm, size_t ikm_len)
{
	psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
	psa_key_derivation_operation_t op = PSA_KEY_DERIVATION_OPERATION_INIT;
	static const uint8_t salt[48];
	static const uint8_t info[] = "tls13 derived reproducer label";
	uint8_t prk[48];
	uint8_t okm[48];
	psa_key_id_t key = PSA_KEY_ID_NULL;
	psa_status_t status;

	/* HKDF-Extract */
	psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_DERIVE);
	psa_set_key_algorithm(&attr, PSA_ALG_HKDF_EXTRACT(PSA_ALG_SHA_384));
	psa_set_key_type(&attr, PSA_KEY_TYPE_DERIVE);

	status = psa_import_key(&attr, ikm, ikm_len, &key);
	if (status != PSA_SUCCESS) {
		LOG_ERR("psa_import_key (extract) failed: %d", status);
		return status;
	}

	status = psa_key_derivation_setup(&op, PSA_ALG_HKDF_EXTRACT(PSA_ALG_SHA_384));
	if (status == PSA_SUCCESS) {
		status = psa_key_derivation_input_bytes(&op, PSA_KEY_DERIVATION_INPUT_SALT,
							salt, sizeof(salt));
	}
	if (status == PSA_SUCCESS) {
		status = psa_key_derivation_input_key(&op, PSA_KEY_DERIVATION_INPUT_SECRET,
						      key);
	}
	if (status == PSA_SUCCESS) {
		status = psa_key_derivation_output_bytes(&op, prk, sizeof(prk));
	}
	psa_key_derivation_abort(&op);
	psa_destroy_key(key);
	if (status != PSA_SUCCESS) {
		LOG_ERR("HKDF-Extract failed: %d", status);
		return status;
	}

	/* HKDF-Expand */
	psa_set_key_algorithm(&attr, PSA_ALG_HKDF_EXPAND(PSA_ALG_SHA_384));

	status = psa_import_key(&attr, prk, sizeof(prk), &key);
	if (status != PSA_SUCCESS) {
		LOG_ERR("psa_import_key (expand) failed: %d", status);
		return status;
	}

	status = psa_key_derivation_setup(&op, PSA_ALG_HKDF_EXPAND(PSA_ALG_SHA_384));
	if (status == PSA_SUCCESS) {
		status = psa_key_derivation_input_key(&op, PSA_KEY_DERIVATION_INPUT_SECRET,
						      key);
	}
	if (status == PSA_SUCCESS) {
		status = psa_key_derivation_input_bytes(&op, PSA_KEY_DERIVATION_INPUT_INFO,
							info, sizeof(info) - 1);
	}
	if (status == PSA_SUCCESS) {
		status = psa_key_derivation_output_bytes(&op, okm, sizeof(okm));
	}
	psa_key_derivation_abort(&op);
	psa_destroy_key(key);
	if (status != PSA_SUCCESS) {
		LOG_ERR("HKDF-Expand failed: %d", status);
	}

	return status;
}
#endif /* PREAMBLE_LEVEL >= 1 */

int main(void)
{
	psa_status_t status;

	LOG_INF("SHA-384 SSF psa_call hang reproducer started");

	status = psa_crypto_init();
	if (status != PSA_SUCCESS) {
		LOG_ERR("psa_crypto_init failed: %d", status);
		return 0;
	}

	for (size_t i = 0; i < sizeof(msg_buf); i++) {
		msg_buf[i] = (uint8_t)i;
	}

	for (repro_iteration = 1; repro_iteration <= NUM_ITERATIONS; repro_iteration++) {
		/* mbedtls keeps checksum operations for both hashes running
		 * for the whole handshake; with both alive the SHA-384 clone
		 * gets operation handle 3, matching the captured hang.
		 */
		psa_hash_operation_t checksum_256 = PSA_HASH_OPERATION_INIT;
		psa_hash_operation_t transcript = PSA_HASH_OPERATION_INIT;

		repro_step = 0;
		/* Sweep digest misalignment 0..7 across iterations. */
		repro_misalign = repro_iteration % 8;

		status = psa_hash_setup(&checksum_256, PSA_ALG_SHA_256);
		if (status != PSA_SUCCESS) {
			LOG_ERR("psa_hash_setup (256) failed: %d", status);
			return 0;
		}

		status = psa_hash_setup(&transcript, PSA_ALG_SHA_384);
		if (status != PSA_SUCCESS) {
			LOG_ERR("psa_hash_setup failed: %d", status);
			return 0;
		}

		for (size_t m = 0; m < ARRAY_SIZE(transcript_msg_sizes); m++) {
			status = psa_hash_update(&checksum_256, msg_buf,
						 transcript_msg_sizes[m]);
			if (status != PSA_SUCCESS) {
				LOG_ERR("psa_hash_update (256) failed: %d", status);
				return 0;
			}

			status = psa_hash_update(&transcript, msg_buf,
						 transcript_msg_sizes[m]);
			if (status != PSA_SUCCESS) {
				LOG_ERR("psa_hash_update failed: %d", status);
				return 0;
			}

			if (extract_after[m]) {
				uint8_t digest[48];

				status = transcript_extract(&transcript, repro_misalign,
							    digest);
				if (status != PSA_SUCCESS) {
					return 0;
				}

#if PREAMBLE_LEVEL >= 1
				status = key_schedule_hkdf(digest, sizeof(digest));
				if (status != PSA_SUCCESS) {
					return 0;
				}
#endif
			}
		}

		psa_hash_abort(&checksum_256);
		psa_hash_abort(&transcript);

		if (repro_iteration % 100 == 0) {
			LOG_INF("iteration %u ok", repro_iteration);
		}
	}

	LOG_INF("All iterations completed - no hang");
	return 0;
}
