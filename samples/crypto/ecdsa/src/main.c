/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr.h>
#include <sys/printk.h>
#include <logging/log.h>
#include <stdio.h>
#include <stdlib.h>

#include <psa/crypto.h>
#include <psa/crypto_extra.h>
#include <tfm_ns_interface.h>

LOG_MODULE_REGISTER(ecdsa, LOG_LEVEL_DBG);

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
/* 				Global variables/defines for the ECDSA example 	 	  	  */

#define NRF_CRYPTO_EXAMPLE_ECDSA_TEXT_SIZE (100)

#define NRF_CRYPTO_EXAMPLE_ECDSA_KEY_BITS (256)
#define NRF_CRYPTO_EXAMPLE_ECDSA_PUBLIC_KEY_SIZE (65)
#define NRF_CRYPTO_EXAMPLE_ECDSA_SIGNATURE_SIZE (64)
#define NRF_CRYPTO_EXAMPLE_ECDSA_HASH_SIZE (32)

/* Below text is used as plaintext for signing/verification */
static uint8_t m_plain_text[NRF_CRYPTO_EXAMPLE_ECDSA_TEXT_SIZE] = {
	"Example string to demonstrate basic usage of ECDSA."
};

static uint8_t m_pub_key[NRF_CRYPTO_EXAMPLE_ECDSA_PUBLIC_KEY_SIZE];

static uint8_t m_signature[NRF_CRYPTO_EXAMPLE_ECDSA_SIGNATURE_SIZE];
static uint8_t m_hash[NRF_CRYPTO_EXAMPLE_ECDSA_HASH_SIZE];

/* ====================================================================== */

int verify_message()
{
	psa_status_t status;

	/* Crypto settings for ECDSA verification using the SHA256 hashing
		algorithm and the secp256r1 curve */
	psa_algorithm_t alg = PSA_ALG_ECDSA(PSA_ALG_SHA_256);
	psa_key_type_t key_type =
		PSA_KEY_TYPE_ECC_PUBLIC_KEY(PSA_ECC_FAMILY_SECP_R1);
	psa_key_usage_t key_usage = PSA_KEY_USAGE_VERIFY_HASH;
	psa_key_lifetime_t key_lifetime = PSA_KEY_LIFETIME_VOLATILE;
	psa_key_handle_t key_handle;
	psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;

	print_message("Verifying ECDSA signature...");

	/* Initialize PSA Crypto */
	status = psa_crypto_init();
	PSA_ERROR_CHECK("psa_crypto_init", status);

	/* Configure the key attributes */
	psa_set_key_usage_flags(&key_attributes, key_usage);
	psa_set_key_lifetime(&key_attributes, key_lifetime);
	psa_set_key_algorithm(&key_attributes, alg);
	psa_set_key_type(&key_attributes, key_type);
	psa_set_key_bits(&key_attributes, 256);

	status = psa_import_key(&key_attributes, m_pub_key, sizeof(m_pub_key),
				&key_handle);
	PSA_ERROR_CHECK("psa_import_key", status);

	psa_reset_key_attributes(&key_attributes);

	/* Verify the signature of the hash */
	status = psa_verify_hash(key_handle, alg, m_hash, sizeof(m_hash),
				 m_signature, sizeof(m_signature));

	PSA_ERROR_CHECK("psa_verify_hash", status);

	print_message("Signature verification was succesfull!");

	status = psa_destroy_key(key_handle);
	PSA_ERROR_CHECK("psa_destroy_key", status);

	return 1;
}

int sign_message()
{
	uint32_t output_len;
	psa_status_t status;

	/* Crypto settings for ECDSA signing using the SHA256 hashing algorithm
	   and the secp256r1 curve */
	psa_algorithm_t hash_alg = PSA_ALG_SHA_256;
	psa_algorithm_t alg = PSA_ALG_ECDSA(hash_alg);
	psa_key_type_t key_type =
		PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1);
	psa_key_usage_t key_usage = PSA_KEY_USAGE_SIGN_HASH;
	psa_key_lifetime_t key_lifetime = PSA_KEY_LIFETIME_VOLATILE;
	psa_key_handle_t key_handle;
	psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;

	print_message("Signing a message using ECDSA...");

	/* Initialize PSA Crypto */
	status = psa_crypto_init();
	PSA_ERROR_CHECK("psa_crypto_init", status);

	/* Configure the key attributes */
	psa_set_key_usage_flags(&key_attributes, key_usage);
	psa_set_key_lifetime(&key_attributes, key_lifetime);
	psa_set_key_algorithm(&key_attributes, alg);
	psa_set_key_type(&key_attributes, key_type);
	psa_set_key_bits(&key_attributes, 256);

	/* Generate a key pair */
	status = psa_generate_key(&key_attributes, &key_handle);
	PSA_ERROR_CHECK("psa_generate_key", status);

	psa_reset_key_attributes(&key_attributes);

	/* Compute the SHA256 hash*/
	status = psa_hash_compute(hash_alg, m_plain_text, sizeof(m_plain_text),
				  m_hash, sizeof(m_hash), &output_len);
	PSA_ERROR_CHECK("psa_hash_compute", status);

	/* Sign the hash */
	status = psa_sign_hash(key_handle, alg, m_hash, sizeof(m_hash),
			       m_signature, sizeof(m_signature), &output_len);
	PSA_ERROR_CHECK("psa_sign_hash", status);

	print_message("Signing the message succesfull!");
	print_hex("Plaintext", m_plain_text, sizeof(m_plain_text));
	print_hex("SHA256 hash", m_hash, sizeof(m_hash));
	print_hex("Signature", m_signature, sizeof(m_signature));

	/* Export the public key */
	status = psa_export_public_key(key_handle, m_pub_key, sizeof(m_pub_key),
				       &output_len);
	PSA_ERROR_CHECK("psa_export_public_key", status);

	/* Destroy the key */
	status = psa_destroy_key(key_handle);
	PSA_ERROR_CHECK("psa_destroy_key", status);

	return 1;
}

void main(void)
{
	enum tfm_status_e tfm_status;

	print_message("Starting ECDSA example...");

	tfm_status = tfm_ns_interface_init();
	if (tfm_status != TFM_SUCCESS) {
		print_message("TFM initialization failed!");
		return;
	}

	if (sign_message() > 0) {
		if (verify_message() > 0) {
			print_message("Example finished succesfully!");
			return;
		}
	}

	print_message("Example exited with error!");
}
