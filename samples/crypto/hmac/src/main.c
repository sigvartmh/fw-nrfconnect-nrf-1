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

LOG_MODULE_REGISTER(hmac, LOG_LEVEL_DBG);

#define PSA_ERROR_CHECK(func, status)                                          \
	({                                                                     \
		if (status != PSA_SUCCESS) {                                   \
			LOG_INF("Function %s failed with error code: %d\r\n",  \
				func, status);                                 \
			return -1;                                             \
		}                                                              \
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
/* 				Global variables/defines for the HMAC example 			  */

#define NRF_CRYPTO_EXAMPLE_HMAC_TEXT_SIZE (100)
#define NRF_CRYPTO_EXAMPLE_HMAC_KEY_SIZE (32)

/* HMAC sample key, DO NOT USE IN PRODUCTION */
static uint8_t m_key[NRF_CRYPTO_EXAMPLE_HMAC_KEY_SIZE] = {
	'H', 'M', 'A', 'C', ' ', 'K', 'E', 'Y', ' ', 'T', 'E', 'S', 'T'
};

/* Below text is used as plaintext for signing/verifiction */
static uint8_t m_plain_text[NRF_CRYPTO_EXAMPLE_HMAC_TEXT_SIZE] = {
	"Example string to demonstrate basic usage of HMAC signing/verification."
};

static uint8_t hmac[NRF_CRYPTO_EXAMPLE_HMAC_KEY_SIZE];

/* ====================================================================== */

int hmac_verify()
{
	psa_status_t status;

	/* Crypto settings for HMAC verification */
	psa_algorithm_t alg = PSA_ALG_HMAC(PSA_ALG_SHA_256);
	psa_key_type_t key_type = PSA_KEY_TYPE_HMAC;
	psa_key_usage_t key_usage = PSA_KEY_USAGE_VERIFY_HASH;
	psa_key_lifetime_t key_lifetime = PSA_KEY_LIFETIME_VOLATILE;
	psa_key_handle_t key_handle;
	psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;
	psa_mac_operation_t operation = PSA_MAC_OPERATION_INIT;

	print_message("Verifying the HMAC signature...");

	/* Initialize PSA Crypto */
	status = psa_crypto_init();
	PSA_ERROR_CHECK("psa_crypto_init", status);

	/* Configure the key attributes */
	psa_set_key_usage_flags(&key_attributes, key_usage);
	psa_set_key_lifetime(&key_attributes, key_lifetime);
	psa_set_key_algorithm(&key_attributes, alg);
	psa_set_key_type(&key_attributes, key_type);
	psa_set_key_bits(&key_attributes, 256);

	/* Import the key to the keystore */
	status = psa_import_key(&key_attributes, m_key, sizeof(m_key),
				&key_handle);
	PSA_ERROR_CHECK("psa_import_key", status);

	/* After the key handle is acquired the attributes are not needed */
	psa_reset_key_attributes(&key_attributes);

	/* Initialize the HMAC verification operation */
	psa_mac_verify_setup(&operation, key_handle, alg);
	PSA_ERROR_CHECK("psa_mac_verify_setup", status);

	/* Perform the HMAC verification */
	status = psa_mac_update(&operation, m_plain_text, sizeof(m_plain_text));
	PSA_ERROR_CHECK("psa_mac_update", status);

	/* Finalize the HMAC verification */
	status = psa_mac_verify_finish(&operation, hmac, sizeof(hmac));
	PSA_ERROR_CHECK("psa_mac_verify_finish", status);

	print_message("HMAC verified succesfully!");

	/* Destroy the key */
	status = psa_destroy_key(key_handle);
	PSA_ERROR_CHECK("psa_destroy_key", status);

	return 1;
}

int hmac_sign()
{
	uint32_t output_len;
	psa_status_t status;

	/* Crypto settings for HMAC signing */
	psa_algorithm_t alg = PSA_ALG_HMAC(PSA_ALG_SHA_256);
	psa_key_type_t key_type = PSA_KEY_TYPE_HMAC;
	psa_key_usage_t key_usage = PSA_KEY_USAGE_SIGN_HASH;
	psa_key_lifetime_t key_lifetime = PSA_KEY_LIFETIME_VOLATILE;
	psa_key_handle_t key_handle;
	psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;
	psa_mac_operation_t operation = PSA_MAC_OPERATION_INIT;

	print_message("Signing using HMAC ...");

	/* Initialize PSA Crypto */
	status = psa_crypto_init();
	PSA_ERROR_CHECK("psa_crypto_init", status);

	/* Configure the key attributes */
	psa_set_key_usage_flags(&key_attributes, key_usage);
	psa_set_key_lifetime(&key_attributes, key_lifetime);
	psa_set_key_algorithm(&key_attributes, alg);
	psa_set_key_type(&key_attributes, key_type);
	psa_set_key_bits(&key_attributes, 256);

	/* Import the key to the keystore */
	status = psa_import_key(&key_attributes, m_key, sizeof(m_key),
				&key_handle);
	PSA_ERROR_CHECK("psa_import_key", status);

	/* After the key handle is acquired the attributes are not needed */
	psa_reset_key_attributes(&key_attributes);

	/* Initialize the HMAC signing operation */
	status = psa_mac_sign_setup(&operation, key_handle, alg);
	PSA_ERROR_CHECK("psa_mac_sign_setup", status);

	/* Perform the HMAC signing */
	status = psa_mac_update(&operation, m_plain_text, sizeof(m_plain_text));
	PSA_ERROR_CHECK("psa_mac_update", status);

	/* Finalize the HMAC signing */
	status = psa_mac_sign_finish(&operation, hmac, sizeof(hmac),
				     &output_len);
	PSA_ERROR_CHECK("psa_sign_finish", status);

	print_message("Signing succesfull!");
	print_hex("Key", m_key, sizeof(m_key));
	print_hex("Plaintext", m_plain_text, sizeof(m_plain_text));
	print_hex("HMAC", hmac, sizeof(hmac));

	/* Destroy the key */
	status = psa_destroy_key(key_handle);
	PSA_ERROR_CHECK("psa_destroy_key", status);

	return 1;
}

void main(void)
{
	enum tfm_status_e tfm_status;

	print_message("Starting HMAC example...");

	tfm_status = tfm_ns_interface_init();
	if (tfm_status != TFM_SUCCESS) {
		print_message("TFM initialization failed!");
		return;
	}

	if (hmac_sign() > 0) {
		if (hmac_verify() > 0) {
			print_message("Example completed succesfully!");
			return;
		}
	}

	print_message("Example exited with error!");
}
