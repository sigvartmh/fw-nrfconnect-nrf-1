/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include "debug.h"
#include <zephyr/types.h>
#include <toolchain.h>
#include <bl_crypto.h>
#include <fw_metadata.h>
#include "bl_crypto_internal.h"

__weak int crypto_init_sig(void)
{
	return 0;
}

__weak int crypto_init_hash(void)
{
	return 0;
}

int crypto_init(void)
{
	int err = crypto_init_sig();
	if (err) {
		return err;
	}
	err = crypto_init_hash();
	if (err) {
		return err;
	}
	return 0;
}


static int _crypto_root_of_trust(const u8_t *pk, const u8_t *pk_hash,
				 const u8_t *sig, const u8_t *fw,
				 const u32_t fw_len, bool external)
{
	__ASSERT(pk && pk_hash && sig && fw, "A parameter was NULL.");
	if (!verify_truncated_hash(pk, CONFIG_SB_PUBLIC_KEY_LEN, pk_hash,
				   CONFIG_SB_PUBLIC_KEY_HASH_LEN, external)) {
		return -EPKHASHINV;
	}

	if (!verify_sig(fw, fw_len, sig, pk, external)) {
		return -ESIGINV;
	}
	return 0;
}


int crypto_root_of_trust(const u8_t *pk, const u8_t *pk_hash,
			 const u8_t *sig, const u8_t *fw,
			 const u32_t fw_len)
{
	return _crypto_root_of_trust(pk, pk_hash, sig, fw, fw_len, false);
}


int crypto_root_of_trust_external(const u8_t *pk, const u8_t *pk_hash,
				  const u8_t *sig, const u8_t *fw,
				  const u32_t fw_len)
{
	return _crypto_root_of_trust(pk, pk_hash, sig, fw, fw_len, true);
}


const struct bl_crypto_abi bl_crypto_abi
_GENERIC_SECTION(.extabis)
__attribute__((used)) = {
	.header = {
		.magic = {ABI_INFO_MAGIC},
		.abi_flags = 0,
		.abi_id = 0x12345678,
		.abi_version = 1,
		.abi_len = sizeof(struct bl_crypto_abi),
	},
	.abi = {
		.crypto_root_of_trust = crypto_root_of_trust_external,
	},
};
