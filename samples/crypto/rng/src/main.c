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

LOG_MODULE_REGISTER(rng, LOG_LEVEL_DBG);

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

/* ========== Global variables/defines for the RNG example ========== */

#define NRF_CRYPTO_EXAMPLE_RNG_DATA_SIZE (100)
#define NRF_CRYPTO_EXAMPLE_RNG_ITERATIONS (5)

static uint8_t m_rng_data[NRF_CRYPTO_EXAMPLE_RNG_DATA_SIZE];

/* ========== End of global variables/defines for the RNG example ========== */

int produce_rng_data()
{
	psa_status_t status;

	print_message("Producing 5 random numbers...");

	/* Initialize PSA Crypto */
	status = psa_crypto_init();
	PSA_ERROR_CHECK("psa_crypto_init", status);

	for(int i=0; i<NRF_CRYPTO_EXAMPLE_RNG_ITERATIONS; i++){
		/* Generate the random number */
		status = psa_generate_random(m_rng_data, sizeof(m_rng_data));
		PSA_ERROR_CHECK("psa_generate_random", status);

		print_hex("RNG data", m_rng_data, sizeof(m_rng_data));
	}

	return 1;
}

void main(void)
{
	enum tfm_status_e tfm_status;

	print_message("Starting RNG example...");

	tfm_status = tfm_ns_interface_init();
	if (tfm_status != TFM_SUCCESS) {
		print_message("TFM initialization failed!");
		return;
	}

	if (produce_rng_data() > 0){
			print_message("Example completed succesfully!");
			return;
	}

	print_message("Example exited with error!");
}
