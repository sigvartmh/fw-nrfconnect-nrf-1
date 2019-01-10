/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <bl_crypto.h>
#include "bl_crypto_internal.h"
#include <fw_metadata.h>

//extern struct fw_abi_getter_info abi_getter_in;
extern const struct fw_abi_getter_info _abis_start;
int crypto_root_of_trust(const u8_t *pk, const u8_t *pk_hash,
			 const u8_t *sig, const u8_t *fw,
			 const u32_t fw_len)
{
	static const struct fw_abi_getter_info * const m_abi_getter = &_abis_start;
	printk("abi_getter_in addr: 0x%x\r\n", &(*m_abi_getter));
	printk("abi_length: %d\r\n", m_abi_getter->abis_len);
	struct bl_crypto_abi *crypto_abi = (struct bl_crypto_abi *) &m_abi_getter->abis;
	printk("abi_id: 0x%x\r\n", crypto_abi->header.abi_id);
	return ((struct bl_crypto_abi *) &m_abi_getter->abis)->abi.crypto_root_of_trust(pk, pk_hash, sig, fw, fw_len);
	//return ((struct bl_crypto_abi*)(*m_abi_getter->abis))->abi.
//		crypto_root_of_trust(pk, pk_hash, sig, fw, fw_len);
}
