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

LOG_MODULE_REGISTER(aes_gcm, LOG_LEVEL_DBG);

K_MUTEX_DEFINE(tfm_mutex);

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
/* 			Global variables/defines for the AES GCM mode example		  */

#define NRF_CRYPTO_EXAMPLE_AES_MAX_TEXT_SIZE (100)
#define NRF_CRYPTO_EXAMPLE_AES_BLOCK_SIZE (16)
#define NRF_CRYPTO_EXAMPLE_AES_ADDITIONAL_SIZE (35)
#define NRF_CRYPTO_EXAMPLE_AES_GCM_TAG_LENGTH (16)

/* AES sample key, DO NOT USE IN PRODUCTION */
static uint8_t m_key[NRF_CRYPTO_EXAMPLE_AES_BLOCK_SIZE] = {
	'A', 'E', 'S', ' ', 'G', 'C', 'M', ' ',
	'K', 'E', 'Y', ' ', 'T', 'E', 'S', 'T'
};

/* AES sample IV, DO NOT USE IN PRODUCTION */
static uint8_t m_iv[NRF_CRYPTO_EXAMPLE_AES_BLOCK_SIZE] = { 'S', 'A', 'M',
							   'P', 'L', 'E',
							   ' ', 'I', 'V' };

/* Below text is used as plaintext for encryption/decryption */
static uint8_t m_plain_text[NRF_CRYPTO_EXAMPLE_AES_MAX_TEXT_SIZE] = {
	"Example string to demonstrate basic usage of AES GCM mode."
};

/* Below text is used as additional data for authentication */
static uint8_t m_additional_data[NRF_CRYPTO_EXAMPLE_AES_ADDITIONAL_SIZE] = {
	"Example string of additional data"
};

static uint8_t m_encrypted_text[NRF_CRYPTO_EXAMPLE_AES_MAX_TEXT_SIZE +
				NRF_CRYPTO_EXAMPLE_AES_GCM_TAG_LENGTH];

static uint8_t m_decrypted_text[NRF_CRYPTO_EXAMPLE_AES_MAX_TEXT_SIZE];

/* ====================================================================== */

int decrypt_aes_gcm()
{
	uint32_t output_len;
	psa_status_t status;

	/* Crypto settings for GCM mode using an AES-128 bit key */
	psa_algorithm_t alg = PSA_ALG_GCM;
	psa_key_type_t key_type = PSA_KEY_TYPE_AES;
	psa_key_usage_t key_usage = PSA_KEY_USAGE_DECRYPT;
	psa_key_lifetime_t key_lifetime = PSA_KEY_LIFETIME_VOLATILE;
	psa_key_handle_t key_handle;
	psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;

	print_message("Decrypting using AES GCM MODE...");

	/* Initialize PSA Crypto */
	status = psa_crypto_init();
	PSA_ERROR_CHECK("psa_crypto_init", status);

	/* Configure the key attributes */
	psa_set_key_usage_flags(&key_attributes, key_usage);
	psa_set_key_lifetime(&key_attributes, key_lifetime);
	psa_set_key_algorithm(&key_attributes, alg);
	psa_set_key_type(&key_attributes, key_type);
	psa_set_key_bits(&key_attributes, 128);

	/* Import the key to the keystore */
	status = psa_import_key(&key_attributes, m_key, sizeof(m_key),
				&key_handle);
	PSA_ERROR_CHECK("psa_import_key", status);

	/* After the key handle is acquired the attributes are not needed */
	psa_reset_key_attributes(&key_attributes);

	/* Decrypt and authenticate the encrypted data */
	status = psa_aead_decrypt(key_handle, alg, m_iv, sizeof(m_iv),
				  m_additional_data, sizeof(m_additional_data),
				  m_encrypted_text, sizeof(m_encrypted_text),
				  m_decrypted_text, sizeof(m_decrypted_text),
				  &output_len);
	PSA_ERROR_CHECK("psa_aead_decrypt", status);

	print_hex("Decrypted text", m_decrypted_text, sizeof(m_decrypted_text));

	/* Check the validity of the decryption */
	if ( memcmp(m_decrypted_text,
						m_plain_text,
						NRF_CRYPTO_EXAMPLE_AES_MAX_TEXT_SIZE) !=0 ){
			print_message("Error: Decrypted text doesn't match the plaintext");
			return -1;
	}

	print_message("Decryption and authentication succesfull!");

	/* Destroy the key */
	psa_destroy_key(key_handle);
	PSA_ERROR_CHECK("psa_destroy_key", status);

	return 1;
}

int encrypt_aes_gcm()
{
	uint32_t output_len;
	psa_status_t status;

	/* Crypto settings for GCM mode using an AES-128 bit key */
	psa_algorithm_t alg = PSA_ALG_GCM;
	psa_key_type_t key_type = PSA_KEY_TYPE_AES;
	psa_key_usage_t key_usage = PSA_KEY_USAGE_ENCRYPT;
	psa_key_lifetime_t key_lifetime = PSA_KEY_LIFETIME_VOLATILE;
	psa_key_handle_t key_handle;
	psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;

	print_message("Encrypting using AES GCM MODE...");

	/* Initialize PSA Crypto */
	status = psa_crypto_init();
	PSA_ERROR_CHECK("psa_crypto_init", status);

	/* Configure the key attributes */
	psa_set_key_usage_flags(&key_attributes, key_usage);
	psa_set_key_lifetime(&key_attributes, key_lifetime);
	psa_set_key_algorithm(&key_attributes, alg);
	psa_set_key_type(&key_attributes, key_type);
	psa_set_key_bits(&key_attributes, 128);

	/* Import the key to the keystore */
	status = psa_import_key(&key_attributes, m_key, sizeof(m_key),
				&key_handle);
	PSA_ERROR_CHECK("psa_import_key", status);

	/* After the key handle is acquired the attributes are not needed */
	psa_reset_key_attributes(&key_attributes);

	/* Encrypt the plaintext and create the authentication tag */
	status = psa_aead_encrypt(key_handle, alg, m_iv, sizeof(m_iv),
				  m_additional_data, sizeof(m_additional_data),
				  m_plain_text, sizeof(m_plain_text),
				  m_encrypted_text, sizeof(m_encrypted_text),
				  &output_len);
	PSA_ERROR_CHECK("psa_aead_encrypt", status);

	print_message("Encryption succesfull!");
	print_hex("Key", m_key, sizeof(m_key));
	print_hex("IV", m_iv, sizeof(m_iv));
	print_hex("Additional data", m_additional_data,
		  sizeof(m_additional_data));
	print_hex("Plaintext", m_plain_text, sizeof(m_plain_text));
	print_hex("Encrypted text", m_encrypted_text, sizeof(m_encrypted_text));

	/* Destroy the key */
	psa_destroy_key(key_handle);
	PSA_ERROR_CHECK("psa_destroy_key", status);

	return 1;
}

void main(void)
{
	enum tfm_status_e tfm_status;

	print_message("Starting AES-GCM example...");

	tfm_status = tfm_ns_interface_init();
	if (tfm_status != TFM_SUCCESS) {
		print_message("TFM initialization failed!");
		return;
	}

	if (encrypt_aes_gcm() > 0) {
		if (decrypt_aes_gcm() > 0) {
			print_message("Example completed succesfully!");
			return;
		}
	}
	print_message("Example exited with error!");
}
