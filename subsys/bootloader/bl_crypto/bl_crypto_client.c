/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <bl_crypto.h>
#include "bl_crypto_internal.h"
#include <fw_metadata.h>

extern struct fw_abi_getter_info abi_getter_in;

int crypto_root_of_trust(const u8_t *pk, const u8_t *pk_hash,
			 const u8_t *sig, const u8_t *fw,
			 const u32_t fw_len)
{
	return ((struct bl_crypto_abi*)(*abi_getter_in.abis))->abi.
		crypto_root_of_trust(pk, pk_hash, sig, fw, fw_len);
}
