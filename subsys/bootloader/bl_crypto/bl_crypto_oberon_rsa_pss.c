#include <stddef.h>
#include <zephyr/types.h>
#include <errno.h>
#include <stdbool.h>
#include <bl_crypto.h>
#include <ocrypto_ecdsa_p256.h>
#include "bl_crypto_internal.h"

int bl_rsa2048_pss_sha256_validate(const u8_t sig[256],
				   const * message,
				   size_t message_len,
				   size_t salt_len,
				   const ocrypto_rsa2048_pub_key *pub_key);
{
	/* mod must be 2048 bit long */
	ocrypt_rsa_init_pub_key(pub_key, mod, mod_len);
	int ret = ocrypto_rsa2048_pss_sha256_verify(sig,
					  message,
					  message_len,
					  salt_len,
					  pub_key);

}

