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

LOG_MODULE_REGISTER(hkdf, LOG_LEVEL_DBG);

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
/* 				Global variables/defines for the HKDF example 			  */

#define NRF_CRYPTO_EXAMPLE_HKDF_INPUT_KEY_SIZE (22)
#define NRF_CRYPTO_EXAMPLE_HKDF_SALT_SIZE (13)
#define NRF_CRYPTO_EXAMPLE_HKDF_AINFO_SIZE (10)
#define NRF_CRYPTO_EXAMPLE_HKDF_OUTPUT_KEY_SIZE (42)

/* Test data from RFC 5869 Test Case 1 (https://tools.ietf.org/html/rfc5869) */
static uint8_t m_input_key[NRF_CRYPTO_EXAMPLE_HKDF_INPUT_KEY_SIZE] = {
	0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
	0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b
};

static uint8_t m_salt[NRF_CRYPTO_EXAMPLE_HKDF_SALT_SIZE] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
	0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c
};

/* Additional info material optionally used to increase entropy. */
static uint8_t m_ainfo[NRF_CRYPTO_EXAMPLE_HKDF_AINFO_SIZE] = {
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9
};

/* Buffer to hold the key generated from HKDF */
static uint8_t m_output_key[NRF_CRYPTO_EXAMPLE_HKDF_OUTPUT_KEY_SIZE];

/* Expected output key material (result of HKDF operation) */
static uint8_t m_expected_output_key[NRF_CRYPTO_EXAMPLE_HKDF_OUTPUT_KEY_SIZE] = {
	0x3c, 0xb2, 0x5f, 0x25, 0xfa, 0xac, 0xd5, 0x7a, 0x90, 0x43, 0x4f,
	0x64, 0xd0, 0x36, 0x2f, 0x2a, 0x2d, 0x2d, 0x0a, 0x90, 0xcf, 0x1a,
	0x5a, 0x4c, 0x5d, 0xb0, 0x2d, 0x56, 0xec, 0xc4, 0xc5, 0xbf, 0x34,
	0x00, 0x72, 0x08, 0xd5, 0xb8, 0x87, 0x18, 0x58, 0x65
};

/* ====================================================================== */

int derive_hkdf()
{
	uint32_t output_len;
	psa_status_t status;

	/* Crypto settings for HKDF */
	psa_algorithm_t alg = PSA_ALG_HKDF(PSA_ALG_SHA_256);
	psa_key_derivation_operation_t operation =
		PSA_KEY_DERIVATION_OPERATION_INIT;

	/* Input key settings */
	psa_key_type_t in_key_type = PSA_KEY_TYPE_DERIVE;
	psa_key_usage_t in_key_usage = PSA_KEY_USAGE_DERIVE;
	psa_key_lifetime_t in_key_lifetime = PSA_KEY_LIFETIME_VOLATILE;
	psa_key_handle_t in_key_handle;
	psa_key_attributes_t in_key_attributes = PSA_KEY_ATTRIBUTES_INIT;

	/* Derived key settings
	   Warning: This key usage makes the key exportable which is not safe and
	   is only done to demonstrate the validity of the results. Please do not use
	   this in production environments.  */
	psa_key_type_t out_key_type = PSA_KEY_TYPE_RAW_DATA;
	psa_key_usage_t out_key_usage = PSA_KEY_USAGE_EXPORT;
	psa_key_lifetime_t out_key_lifetime = PSA_KEY_LIFETIME_VOLATILE;
	psa_key_handle_t out_key_handle;
	psa_key_attributes_t out_key_attributes = PSA_KEY_ATTRIBUTES_INIT;

	/* Configure the input key attributes */
	psa_set_key_usage_flags(&in_key_attributes, in_key_usage);
	psa_set_key_lifetime(&in_key_attributes, in_key_lifetime);
	psa_set_key_algorithm(&in_key_attributes, alg);
	psa_set_key_type(&in_key_attributes, in_key_type);
	psa_set_key_bits(&in_key_attributes,
			 NRF_CRYPTO_EXAMPLE_HKDF_INPUT_KEY_SIZE * 8);

	/* Configure the output key attributes */
	psa_set_key_lifetime(&out_key_attributes, out_key_lifetime);
	psa_set_key_type(&out_key_attributes, out_key_type);
	psa_set_key_usage_flags(&out_key_attributes, out_key_usage);
	psa_set_key_bits(&out_key_attributes,
			 NRF_CRYPTO_EXAMPLE_HKDF_OUTPUT_KEY_SIZE * 8);

	print_message("Deriving a key using HKDR and SHA256...");

	/* Initialize PSA Crypto */
	status = psa_crypto_init();
	PSA_ERROR_CHECK("psa_crypto_init", status);

	/* Import the master key into the keystore */
	status = psa_import_key(&in_key_attributes, m_input_key,
				sizeof(m_input_key), &in_key_handle);
	PSA_ERROR_CHECK("psa_import_key", status);

	/* Set the derivation algorithm */
	status = psa_key_derivation_setup(&operation, alg);
	PSA_ERROR_CHECK("psa_key_derivation_setup", status);

	/* Set the salt for the operation */
	status = psa_key_derivation_input_bytes(&operation,
						PSA_KEY_DERIVATION_INPUT_SALT,
						m_salt, sizeof(m_salt));
	PSA_ERROR_CHECK("psa_key_derivation_input_bytes", status);

	/* Set the master key for the operation */
	status = psa_key_derivation_input_key(
		&operation, PSA_KEY_DERIVATION_INPUT_SECRET, in_key_handle);
	PSA_ERROR_CHECK("psa_key_derivation_input_key", status);

	/* Set the additional info for the operation */
	status = psa_key_derivation_input_bytes(&operation,
						PSA_KEY_DERIVATION_INPUT_INFO,
						m_ainfo, sizeof(m_ainfo));
	PSA_ERROR_CHECK("psa_key_derivation_input_bytes", status);

	/* Store the derived key in the keystore slot pointed by out_key_handle */
	status = psa_key_derivation_output_key(&out_key_attributes, &operation,
					       &out_key_handle);
	PSA_ERROR_CHECK("psa_key_derivation_output_key", status);

	/* Export the generated key content to verify it's value */
	status = psa_export_key(out_key_handle, m_output_key,
				sizeof(m_output_key), &output_len);
	PSA_ERROR_CHECK("psa_export_key", status);

	/* Clean up the context/buffers */
	psa_reset_key_attributes(&in_key_attributes);
	psa_reset_key_attributes(&out_key_attributes);

	status = psa_key_derivation_abort(&operation);
	PSA_ERROR_CHECK("psa_key_derivation_abort", status);

	status = psa_destroy_key(in_key_handle);
	PSA_ERROR_CHECK("psa_destroy_key", status);

	status = psa_destroy_key(out_key_handle);
	PSA_ERROR_CHECK("psa_destroy_key", status);

	print_message("Key derivation succesfull!");
	print_hex("Input key", m_input_key, sizeof(m_input_key));
	print_hex("Salt", m_salt, sizeof(m_salt));
	print_hex("Additional data", m_ainfo, sizeof(m_ainfo));
	print_hex("Output key", m_output_key, sizeof(m_output_key));

	print_message("Compare derived key with expected value...");
	if (memcmp(m_expected_output_key, m_output_key, sizeof(m_output_key)) !=
	    0) {
		print_message(
			"Error,the key doesn't match the expected value.");
		return -1;
	}

	print_message("The derived key matches the expected value!");

	return 1;
}

void main(void)
{
	enum tfm_status_e tfm_status;

	print_message("Starting HKDF example...");

	tfm_status = tfm_ns_interface_init();
	if (tfm_status != TFM_SUCCESS) {
		print_message("TFM initialization failed!");
		return;
	}

	if (derive_hkdf() > 0) {
		print_message("Example finished succesfully!");
		return;
	}

	print_message("Example exited with error!");
}
