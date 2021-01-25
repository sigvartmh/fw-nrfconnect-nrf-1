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

LOG_MODULE_REGISTER(ecdh, LOG_LEVEL_DBG);

#define PSA_ERROR_CHECK(func, status)\
	({\
		if (status != PSA_SUCCESS) {\
			LOG_INF("Function %s failed with error code: %d\r\n",\
			       func, status);\
			return -1;\
		}\
	})

void print_message(char const *p_text){
	LOG_INF("%s", p_text);
}

void print_hex(char const *p_label, char const *p_text, size_t len)
{
	LOG_INF("---- %s (len: %u): ----", p_label, len);
	LOG_HEXDUMP_INF(p_text, len, "Content:");
	LOG_INF("---- %s end  ----", p_label);
}

/* ====================================================================== */
/* 				Global variables/defines for the ECDH example 	 		  */

#define NRF_CRYPTO_EXAMPLE_ECDH_TEXT_SIZE (100)

#define NRF_CRYPTO_EXAMPLE_ECDH_KEY_BITS (256)
#define NRF_CRYPTO_EXAMPLE_ECDH_PUBLIC_KEY_SIZE (65)
#define NRF_CRYPTO_EXAMPLE_ECDH_SIGNATURE_SIZE (64)

/* Buffers to hold Bob's and Alice's public keys */
static uint8_t m_pub_key_bob[NRF_CRYPTO_EXAMPLE_ECDH_PUBLIC_KEY_SIZE];
static uint8_t m_pub_key_alice[NRF_CRYPTO_EXAMPLE_ECDH_PUBLIC_KEY_SIZE];

/* Buffers to hold Bob's and Alice's secret values */
static uint8_t m_secret_alice[32];
static uint8_t m_secret_bob[32];

psa_key_handle_t key_handle_alice;
psa_key_handle_t key_handle_bob;

/* ====================================================================== */

int create_keypair_bob(){
	uint32_t output_len;
	psa_status_t status;

	/* Crypto settings for ECDH using the SHA256 hashing algorithm,
	   the secp256r1 curve */
	psa_algorithm_t alg = PSA_ALG_ECDH;
	psa_key_type_t key_type = PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1);
	psa_key_usage_t key_usage = PSA_KEY_USAGE_DERIVE;
	psa_key_lifetime_t key_lifetime = PSA_KEY_LIFETIME_VOLATILE;
	psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;


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
	status = psa_generate_key(&key_attributes, &key_handle_bob);
	PSA_ERROR_CHECK("psa_generate_key", status);

	/* Export the public key */
	status = psa_export_public_key(key_handle_bob, m_pub_key_bob,
									sizeof(m_pub_key_bob), &output_len);
	PSA_ERROR_CHECK("psa_export_public_key", status);

	psa_reset_key_attributes(&key_attributes);

	return 1;
}

int create_keypair_alice(){
	uint32_t output_len;
	psa_status_t status;

	/* Crypto settings for ECDH using the SHA256 hashing algorithm,
	   the secp256r1 curve */
	psa_algorithm_t alg = PSA_ALG_ECDH;
	psa_key_type_t key_type = PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1);
	psa_key_usage_t key_usage = PSA_KEY_USAGE_DERIVE;
	psa_key_lifetime_t key_lifetime = PSA_KEY_LIFETIME_VOLATILE;
	psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;


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
	status = psa_generate_key(&key_attributes, &key_handle_alice);
	PSA_ERROR_CHECK("psa_generate_key", status);

	/* Export the public key */
	status = psa_export_public_key(key_handle_alice, m_pub_key_alice,
									sizeof(m_pub_key_alice), &output_len);
	PSA_ERROR_CHECK("psa_export_public_key", status);

	psa_reset_key_attributes(&key_attributes);

	return 1;
}

int calculate_secret_alice(){
	uint32_t output_len;
	psa_status_t status;
	psa_algorithm_t alg = PSA_ALG_ECDH;

	/* Initialize PSA Crypto */
	status = psa_crypto_init();
	PSA_ERROR_CHECK("psa_crypto_init", status);

	/* Perform the ECDH key exchange to calculate the secret */
	status = psa_raw_key_agreement(alg,
									key_handle_alice,
									m_pub_key_bob,
									sizeof(m_pub_key_bob),
									m_secret_alice,
									sizeof(m_secret_alice),
									&output_len);
	PSA_ERROR_CHECK("psa_raw_key_agreement", status);

	return 1;
}

int calculate_secret_bob(){
	uint32_t output_len;
	psa_status_t status;
	psa_algorithm_t alg = PSA_ALG_ECDH;

	/* Initialize PSA Crypto */
	status = psa_crypto_init();
	PSA_ERROR_CHECK("psa_crypto_init", status);

	/* Perform the ECDH key exchange to calculate the secret */
	status = psa_raw_key_agreement(alg,
									key_handle_bob,
									m_pub_key_alice,
									sizeof(m_pub_key_alice),
									m_secret_bob,
									sizeof(m_secret_bob),
									&output_len);
	PSA_ERROR_CHECK("psa_raw_key_agreement", status);

	return 1;
}

int destroy_keys(){
	psa_status_t status;

	status = psa_destroy_key(key_handle_alice);
	PSA_ERROR_CHECK("psa_raw_key_agreement", status);

	status = psa_destroy_key(key_handle_bob);
	PSA_ERROR_CHECK("psa_raw_key_agreement", status);

	return 1;
}

int ecdh()
{
	int ret = 1;

	print_message("Creating ECDH key pair for Alice");
	ret = create_keypair_alice();
	if (ret < 0){
		print_message("Error creating keypair for Alice");
		return ret;
	}

	print_message("Creating ECDH key pair for Bob");
	ret = create_keypair_bob();
	if (ret < 0){
		print_message("Error creating keypair for Bob");
		return ret;
	}

	print_message("Calculating the secret value for Alice");
	ret = calculate_secret_alice();
	if ( ret < 0){
		print_message("Error calculating the secret value for Alice");
		return ret;
	}
	print_hex("Alice's secret value", m_secret_alice, sizeof(m_secret_alice));

	print_message("Calculating the secret value for Bob");
	ret = calculate_secret_bob();
	if ( ret < 0){
		print_message("Error calculating the secret value for Bob");
		return ret;
	}
	print_hex("Bob's secret value", m_secret_bob, sizeof(m_secret_bob));

	print_message("Comparing the secret values");
	ret = memcmp(m_secret_bob, m_secret_alice, sizeof(m_secret_alice));
	if( ret != 0){
		print_message("Error, the calculated secrets don't match");
		return -1;
	}
	print_message("The secret values of Bob and Alice match!");

	ret = destroy_keys();
	if ( ret < 0){
		print_message("Error destroying the keys");
		return ret;
	}

	return 1;
}

void main(void)
{
	enum tfm_status_e tfm_status;

	print_message("Starting ECDH example...");

	tfm_status = tfm_ns_interface_init();
	if (tfm_status != TFM_SUCCESS) {
		print_message("TFM initialization failed!");
		return;
	}

	if ( ecdh() > 0 ){
				print_message("Example finished succesfully!");
				return;
	}

	print_message("Example exited with error!");
}
