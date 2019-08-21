/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr/types.h>
#include <string.h>
#include <image.h>
#include <errno.h>

/**@brief Reads MCUBoots Image header
 *
 * @param header_addr Address of where the image header is located.
 * @param[out] image_header Pointer to the image header struct which should be
 * populated.
 *
 * @retval 0 If successfully read the IMAGE_HEADER magic meaning an MCUBoot
 * header was present.
 * @retval -EINVAL If the image_header magic doesn't match meaning an MCUBoot
 * header was not found at the given address.
 */
static int mcuboot_read_img_hdr(u32_t *header_addr, struct image_header *image_header)
{
	memcpy(image_header, header_addr, IMAGE_HEADER_SIZE);
	if (image_header->ih_magic != IMAGE_MAGIC) {
		return -EINVAL;
	}
	return 0;
}

uint32_t mcuboot_read_version_number(u32_t *header_addr)
{
	struct image_header img_hdr;
	int found_header = mcuboot_read_img_hdr(header_addr, &img_hdr);
	if (found_header < 0) {
		return __UINT32_MAX__;
	};
	/* major 8-bit, minor 8-bit, revision 16-bit */
	return (((uint32_t)img_hdr.ih_ver.iv_major) << 24 |
		           img_hdr.ih_ver.iv_minor  << 16 |
			   img_hdr.ih_ver.iv_revision);
}
