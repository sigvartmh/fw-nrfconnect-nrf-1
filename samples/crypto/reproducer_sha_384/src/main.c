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
#define PREAMBLE_LEVEL 4
#endif

/* Fixed-address experiment: issue the finish with the output vectors at
 * the exact addresses observed in both captured TLS hangs (which froze
 * at identical addresses for both SHA-256 and SHA-384 ciphersuites).
 * This RAM is unused in this sample's layout (_image_ram_end is far
 * below 0x2f013000).
 */
#ifndef FIXED_ADDR_OUTPUTS
#define FIXED_ADDR_OUTPUTS 1
#endif

#if FIXED_ADDR_OUTPUTS
#define FIXED_LINE_ADDR   0x2f013940UL
#define FIXED_OP_ADDR     0x2f01394cUL
#define FIXED_DIGEST_ADDR 0x2f013978UL
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
#if FIXED_ADDR_OUTPUTS
	psa_hash_operation_t *op = (psa_hash_operation_t *)FIXED_OP_ADDR;
	uint8_t *digest = (uint8_t *)FIXED_DIGEST_ADDR;
	volatile uint32_t *line = (volatile uint32_t *)FIXED_LINE_ADDR;
#else
	struct finish_layout l __aligned(32);
	psa_hash_operation_t *op = &l.op;
	uint8_t *digest = l.digest;
#endif
	size_t hash_len;
	psa_status_t status;

#if FIXED_ADDR_OUTPUTS
	memset(op, 0, sizeof(*op));
	memset(digest, 0, 48 + 8);
	/* Reproduce the captured content of the words sharing the cache
	 * line with the operation handle.
	 */
	line[0] = FIXED_DIGEST_ADDR;
	line[1] = 0x0e048e85;
	line[2] = 0x2f006b21;
#else
	memset(&l, 0, sizeof(l));
	/* Live app-owned words sharing the cache line with op, like the
	 * return addresses found there in the captured hang.
	 */
	l.live[0] = 0x0e048e85;
	l.live[1] = (uint32_t)(uintptr_t)&l;
	l.live[2] = 3;
#endif

	status = psa_hash_clone(transcript, op);
	if (status != PSA_SUCCESS) {
		LOG_ERR("psa_hash_clone failed: %d", status);
		return status;
	}

	repro_step++;
	/* misalign values 1-3 make out_vec[1].base % 4 != 0, which routes
	 * the digest through the SSF client bounce-buffer path; 0 and 4 stay
	 * on the in-place path with different mod-8 placement.
	 */
	status = psa_hash_finish(op, digest + misalign, 48, &hash_len);
	if (status != PSA_SUCCESS) {
		LOG_ERR("psa_hash_finish failed: %d (misalign %u)", status, misalign);
		return status;
	}

	memcpy(digest_out, digest + misalign, 48);

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

#if PREAMBLE_LEVEL >= 2
/* ECDHE exchange as done while processing ServerHello: generate an
 * ephemeral P-256 keypair and run raw ECDH. The own public key stands in
 * for the peer share (the math is valid either way).
 */
static psa_status_t ecdhe_exchange(uint8_t *shared, size_t shared_size, size_t *shared_len)
{
	psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
	psa_key_id_t key = PSA_KEY_ID_NULL;
	uint8_t peer_pub[65];
	size_t peer_pub_len;
	psa_status_t status;

	psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_DERIVE);
	psa_set_key_algorithm(&attr, PSA_ALG_ECDH);
	psa_set_key_type(&attr, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));
	psa_set_key_bits(&attr, 256);

	status = psa_generate_key(&attr, &key);
	if (status != PSA_SUCCESS) {
		LOG_ERR("psa_generate_key failed: %d", status);
		return status;
	}

	status = psa_export_public_key(key, peer_pub, sizeof(peer_pub), &peer_pub_len);
	if (status == PSA_SUCCESS) {
		status = psa_raw_key_agreement(PSA_ALG_ECDH, key, peer_pub, peer_pub_len,
					       shared, shared_size, shared_len);
	}
	if (status != PSA_SUCCESS) {
		LOG_ERR("ECDH failed: %d", status);
	}

	psa_destroy_key(key);

	return status;
}
#endif /* PREAMBLE_LEVEL >= 2 */

#if PREAMBLE_LEVEL >= 3
/* AES-256-GCM record protection as done for each encrypted handshake
 * record (EncryptedExtensions, Certificate, CertificateVerify,
 * Finished): encrypt a record-sized buffer, then decrypt it, like the
 * peer's record decrypt on the client.
 */
static psa_status_t record_aead_roundtrip(psa_key_id_t key, size_t rec_len, uint32_t seq)
{
	static uint8_t ciphertext[512 + 16];
	static uint8_t plaintext[512];
	uint8_t nonce[12] = { 0 };
	uint8_t ad[5] = { 0x17, 0x03, 0x03, (uint8_t)((rec_len + 16) >> 8),
			  (uint8_t)(rec_len + 16) };
	size_t ct_len;
	size_t pt_len;
	psa_status_t status;

	nonce[11] = (uint8_t)seq;

	status = psa_aead_encrypt(key, PSA_ALG_GCM, nonce, sizeof(nonce), ad, sizeof(ad),
				  msg_buf, rec_len, ciphertext, sizeof(ciphertext), &ct_len);
	if (status != PSA_SUCCESS) {
		LOG_ERR("psa_aead_encrypt failed: %d", status);
		return status;
	}

	status = psa_aead_decrypt(key, PSA_ALG_GCM, nonce, sizeof(nonce), ad, sizeof(ad),
				  ciphertext, ct_len, plaintext, sizeof(plaintext), &pt_len);
	if (status != PSA_SUCCESS) {
		LOG_ERR("psa_aead_decrypt failed: %d", status);
	}

	return status;
}

static psa_status_t record_key_import(psa_key_id_t *key)
{
	psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
	static const uint8_t key_data[32] = { 0x42 };

	psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT);
	psa_set_key_algorithm(&attr, PSA_ALG_GCM);
	psa_set_key_type(&attr, PSA_KEY_TYPE_AES);
	psa_set_key_bits(&attr, 256);

	return psa_import_key(&attr, key_data, sizeof(key_data), key);
}
#endif /* PREAMBLE_LEVEL >= 3 */

#if PREAMBLE_LEVEL >= 4
/* Certificate and CertificateVerify processing: one-shot hash of the
 * certificate, then ECDSA P-256 signature verification with an imported
 * public key (the signature is produced locally first so verification
 * succeeds, standing in for the server's signing).
 */
static psa_status_t certificate_verify(void)
{
	psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
	psa_key_id_t sign_key = PSA_KEY_ID_NULL;
	psa_key_id_t verify_key = PSA_KEY_ID_NULL;
	uint8_t cert_hash[32];
	uint8_t sig[72];
	uint8_t pub[65];
	size_t cert_hash_len;
	size_t sig_len;
	size_t pub_len;
	psa_status_t status;

	status = psa_hash_compute(PSA_ALG_SHA_256, msg_buf, 442, cert_hash,
				  sizeof(cert_hash), &cert_hash_len);
	if (status != PSA_SUCCESS) {
		LOG_ERR("psa_hash_compute failed: %d", status);
		return status;
	}

	psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_SIGN_HASH);
	psa_set_key_algorithm(&attr, PSA_ALG_ECDSA(PSA_ALG_SHA_256));
	psa_set_key_type(&attr, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));
	psa_set_key_bits(&attr, 256);

	status = psa_generate_key(&attr, &sign_key);
	if (status != PSA_SUCCESS) {
		LOG_ERR("psa_generate_key (sign) failed: %d", status);
		return status;
	}

	status = psa_sign_hash(sign_key, PSA_ALG_ECDSA(PSA_ALG_SHA_256), cert_hash,
			       cert_hash_len, sig, sizeof(sig), &sig_len);
	if (status == PSA_SUCCESS) {
		status = psa_export_public_key(sign_key, pub, sizeof(pub), &pub_len);
	}
	psa_destroy_key(sign_key);
	if (status != PSA_SUCCESS) {
		LOG_ERR("sign/export failed: %d", status);
		return status;
	}

	psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_VERIFY_HASH);
	psa_set_key_type(&attr, PSA_KEY_TYPE_ECC_PUBLIC_KEY(PSA_ECC_FAMILY_SECP_R1));

	status = psa_import_key(&attr, pub, pub_len, &verify_key);
	if (status != PSA_SUCCESS) {
		LOG_ERR("psa_import_key (verify) failed: %d", status);
		return status;
	}

	status = psa_verify_hash(verify_key, PSA_ALG_ECDSA(PSA_ALG_SHA_256), cert_hash,
				 cert_hash_len, sig, sig_len);
	psa_destroy_key(verify_key);
	if (status != PSA_SUCCESS) {
		LOG_ERR("psa_verify_hash failed: %d", status);
	}

	return status;
}
#endif /* PREAMBLE_LEVEL >= 4 */

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

#if PREAMBLE_LEVEL >= 4
		/* ClientHello random. */
		uint8_t client_random[32];

		status = psa_generate_random(client_random, sizeof(client_random));
		if (status != PSA_SUCCESS) {
			LOG_ERR("psa_generate_random failed: %d", status);
			return 0;
		}
#endif
#if FIXED_ADDR_OUTPUTS
		/* Exact captured addresses: no misalignment. */
		repro_misalign = 0;
#else
		/* Sweep digest misalignment 0..7 across iterations. */
		repro_misalign = repro_iteration % 8;
#endif

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

#if PREAMBLE_LEVEL >= 2
		uint8_t ecdhe_shared[32];
		size_t ecdhe_shared_len = 0;
#endif
#if PREAMBLE_LEVEL >= 3
		psa_key_id_t record_key = PSA_KEY_ID_NULL;
#endif

		for (size_t m = 0; m < ARRAY_SIZE(transcript_msg_sizes); m++) {
#if PREAMBLE_LEVEL >= 2
			/* ECDHE runs while processing ServerHello (m == 1). */
			if (m == 1) {
				status = ecdhe_exchange(ecdhe_shared, sizeof(ecdhe_shared),
							&ecdhe_shared_len);
				if (status != PSA_SUCCESS) {
					return 0;
				}
#if PREAMBLE_LEVEL >= 3
				/* Handshake traffic keys come into use here. */
				status = record_key_import(&record_key);
				if (status != PSA_SUCCESS) {
					LOG_ERR("record key import failed: %d", status);
					return 0;
				}
#endif
			}
#endif
#if PREAMBLE_LEVEL >= 3
			/* Messages from EncryptedExtensions (m == 2) onwards
			 * arrive in AEAD-protected records, decrypted before
			 * being hashed into the transcript.
			 */
			if (m >= 2) {
				status = record_aead_roundtrip(record_key,
							       transcript_msg_sizes[m],
							       (uint32_t)m);
				if (status != PSA_SUCCESS) {
					return 0;
				}
			}
#endif
#if PREAMBLE_LEVEL >= 4
			/* Certificate chain and CertificateVerify checks run
			 * while processing CertificateVerify (m == 4), right
			 * before the Finished message that hangs.
			 */
			if (m == 4) {
				status = certificate_verify();
				if (status != PSA_SUCCESS) {
					return 0;
				}
			}
#endif
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

#if PREAMBLE_LEVEL >= 2
				/* The handshake-secret extraction (m == 1) feeds
				 * the ECDHE shared secret into HKDF, like the
				 * TLS 1.3 key schedule.
				 */
				status = key_schedule_hkdf(
					m == 1 ? ecdhe_shared : digest,
					m == 1 ? ecdhe_shared_len : sizeof(digest));
				if (status != PSA_SUCCESS) {
					return 0;
				}
#elif PREAMBLE_LEVEL >= 1
				status = key_schedule_hkdf(digest, sizeof(digest));
				if (status != PSA_SUCCESS) {
					return 0;
				}
#endif
			}
		}

		psa_hash_abort(&checksum_256);
		psa_hash_abort(&transcript);
#if PREAMBLE_LEVEL >= 3
		psa_destroy_key(record_key);
#endif

		if (repro_iteration % 100 == 0) {
			LOG_INF("iteration %u ok", repro_iteration);
		}
	}

	LOG_INF("All iterations completed - no hang");
	return 0;
}
