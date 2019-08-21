/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/**@brief Reads MCUBoots Image version and returns it as a uint32 value
 *
 * @param header_addr Address of where the image header is located.
 *
 * @retval If successfully found the image header the Image version number formated as a 32-bit unsigned integer.
 * @retval MAX_UINT32 If the image header was not found.
 */
uint32_t mcuboot_read_version_number(u32_t *header_addr);
