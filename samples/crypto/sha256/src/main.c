/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr.h>
#include <sys/printk.h>
#include <stdio.h>
#include <stdlib.h>

#include <psa/crypto.h>
#include <psa/crypto_extra.h>

#include <logging/log.h>
#include <tfm_ns_interface.h>

LOG_MODULE_REGISTER(sha256, LOG_LEVEL_DBG);

#define PSA_ERROR_CHECK(func, status)\
	({\
		if (status != PSA_SUCCESS) {\
			LOG_INF("Function %s failed with error code: %d\r\n",\
				func, status);\
			return -1;\
		}\
	})

void print_message(char const *p_text)
{
	LOG_INF("%s", p_text);
}

void print_hex(char const *p_label, char const *p_text, size_t len)
{
	LOG_INF("---- %s (len: %u): ----", p_label, len);
	LOG_HEXDUMP_INF(p_text, len, "Content:");
	LOG_INF("---- %s end  ----", p_label);
}

/* ====================================================================== */
/* 				Global variables/defines for the SHA256 example			  */

#define NRF_CRYPTO_EXAMPLE_SHA256_TEXT_SIZE (100)
#define NRF_CRYPTO_EXAMPLE_SHA256_SIZE (32)

/* Below text is used as plaintext for computing/verifying the hash. */
static uint8_t m_plain_text[NRF_CRYPTO_EXAMPLE_SHA256_TEXT_SIZE] = {
	"Example string to demonstrate basic usage of SHA256."
};

static uint8_t m_hash[NRF_CRYPTO_EXAMPLE_SHA256_SIZE];

/* ====================================================================== */

int verify_sha256()
{
	psa_status_t status;

	/* Crypto settings for SHA256*/
	psa_algorithm_t alg = PSA_ALG_SHA_256;

	print_message("Verifying the SHA256 hash...");

	/* Initialize PSA Crypto */
	status = psa_crypto_init();
	PSA_ERROR_CHECK("psa_crypto_init", status);

	/* Verify the hash */
	status = psa_hash_compare(alg, m_plain_text, sizeof(m_plain_text),
				  m_hash, sizeof(m_hash));
	PSA_ERROR_CHECK("psa_hash_compare", status);

	print_message("SHA256 verification succesfull!");

	return 1;
}

int hash_sha256()
{
	uint32_t output_len;
	psa_status_t status;

	/* Crypto settings for SHA256*/
	psa_algorithm_t alg = PSA_ALG_SHA_256;

	print_message("Hashing using SHA256...");

	/* Initialize PSA Crypto */
	status = psa_crypto_init();
	PSA_ERROR_CHECK("psa_crypto_init", status);

	/* Calculate the SHA256 hash */
	status = psa_hash_compute(alg, m_plain_text, sizeof(m_plain_text),
				  m_hash, sizeof(m_hash), &output_len);
	PSA_ERROR_CHECK("psa_hash_compute", status);

	print_message("Hashing succesfull!");
	print_hex("Plaintext", m_plain_text, sizeof(m_plain_text));
	print_hex("SHA256 hash", m_hash, sizeof(m_hash));

	return 1;
}

void main(void)
{
	enum tfm_status_e tfm_status;

	print_message("Starting SHA256 example...");

	tfm_status = tfm_ns_interface_init();
	if (tfm_status != TFM_SUCCESS) {
		print_message("TFM initialization failed!");
		return;
	}

	if (hash_sha256() > 0) {
		if (verify_sha256() > 0) {
			print_message("Example completed succesfully!");
			return;
		}
	}
	print_message("Example exited with error!");
}
