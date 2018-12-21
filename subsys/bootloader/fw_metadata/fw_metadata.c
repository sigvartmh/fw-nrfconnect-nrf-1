/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include "fw_metadata.h"
#include <linker/sections.h>

__weak const struct fw_abi_info _abis_start;

extern const u32_t _image_rom_start;
extern const u32_t _flash_used;
extern const struct fw_abi_info _abis_start;

static const struct fw_abi_info * const abis = &_abis_start;

const struct fw_abi_getter_info m_abi_getter = {
	.magic = {ABI_GETTER_INFO_MAGIC},
	.abi_getter = NULL,
	.abis = &abis,
	.abis_len = 1,
};

__noinit struct fw_abi_getter_info abi_getter_in;

const struct fw_firmware_info m_firmware_info
_GENERIC_SECTION(.firmware_info)
__attribute__((used)) = {
	.magic = {FIRMWARE_INFO_MAGIC},
	.firmware_size = (u32_t)&_flash_used,
	.firmware_version = CONFIG_SB_FIRMWARE_VERSION,
	.firmware_address = (u32_t)&_image_rom_start,
	.abi_in = &abi_getter_in,
	.abi_out = &m_abi_getter,
};
