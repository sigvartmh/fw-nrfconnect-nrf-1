/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <mfu_decode.h>
#include <drivers/flash.h>
#include <logging/log.h>
#include <dfu/mfu_ext.h>

LOG_MODULE_REGISTER(mfu_ext, CONFIG_MFU_EXT_LOG_LEVEL);

#define MAX_META_LEN 0x400

uint8_t *buf;

int mfu_ext_init(uint8_t *buf_in, size_t buf_len)
{
	if (buf_in == NULL) {
		return -EINVAL;
	}

	buf = buf_in;

	return 0;
}

int mfu_ext_prevalidate(const struct device *fdev, size_t offset, bool *valid,
			uint32_t *seg_offset)
{
	/* Parse manifest and return address of seg_offset, could
	 * use return address instead
	 */
	*valid = true;

	return 0;
}


static int load_segment(const struct device *fdev, size_t seg_size,
			uint32_t seg_addr)
{
	struct memory_chunk chunk = {.data = buf};
	size_t bytes_left = seg_size;
	uint32_t read_addr = seg_addr;
	int err;

	while (bytes_left) {
		uint32_t read_len = MAX(RPC_BUFFER_SIZE, bytes_left);
		err = flash_read(fdev, read_addr, buf, read_len);
		if (err != 0) {
			LOG_ERR("flash_read failed2: %d", err);
			return err;
		}

		chunk.data_len = read_len;
		chunk.target_address = seg_addr + \
				       (seg_size - bytes_left - read_len);

		err = mfu_stream_chunk(&chunk);
		if (err != 0) {
			LOG_ERR("mfu_stream failed: %d", err);
			return err;
		}

		bytes_left -= read_len;
		read_addr += read_len;
	}

	return 0;
}

int mfu_ext_load(const struct device *fdev, size_t offset)
{
	int err;
	Segments_t seg;
	size_t meta_size;
	size_t prev_segments_len = 0;

	if (fdev == NULL) {
		return -EINVAL;
	}

	err = flash_read(fdev, offset, buf, MAX_META_LEN);
	if (err != 0) {
		LOG_ERR("flash_read failed1: %d", err);
		return err;
	}

	err = cbor_decode_Segments(buf, MAX_META_LEN, &seg, false, &meta_size);
	if (err != 0) {
		LOG_ERR("cbor_decode_Segments failed: %d", err);
		return err;
	}

	for (int i = 0; i < seg._Segments__Segment_count; i++) {
		size_t seg_size = seg._Segments__Segment[i]._Segment_len;
		uint32_t seg_addr = meta_size + prev_segments_len;

		err = load_segment(fdev, seg_size, seg_addr);
		if (err != 0) {
			LOG_ERR("load_segment failed: %d", err);
			return err;
		}

		err = mfu_stream_end();
		if (err != 0) {
			LOG_ERR("mfu_stream_end failed: %d", err);
			return err;
		}

		prev_segments_len += seg_size;
	}

	return 0;

}

