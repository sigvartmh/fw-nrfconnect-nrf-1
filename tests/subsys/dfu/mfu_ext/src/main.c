/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <string.h>
#include <zephyr/types.h>
#include <stdbool.h>
#include <ztest.h>
#include <device.h>
#include <drivers/flash.h>
#include <dfu/mfu_ext.h>

/* TODO replace this with actual data, make it bigger than the buf */
static const uint8_t data[] = {0x10, 0xab, 0x10};

#define BUF_SIZE (8*1024)
static uint8_t buf[BUF_SIZE];

/* TODO provide overlay for 9160 DK to use ext flash */
#define FLASH_NAME DT_CHOSEN_ZEPHYR_FLASH_CONTROLLER_LABEL

#define OFFSET 0x10000 /* Same used when generating the segments */

int mfu_stream_chunk(const struct memory_chunk *chunk)
{
	return 0;

}


int mfu_stream_end(void)
{
	return 0;
}

static void test_mfu_ext_load_segments(void)
{
	/* Generate a hex file with five segments, 0x10000 and on.
	 * Serialize this and copy the C array of these segments and its
	 * header to flash. Next, perform the mfu_ext_load operation
	 * and verify that the contents passed is correct.
	 */
	int err;
	const struct device *fdev = device_get_binding(FLASH_NAME);

	err = flash_write(fdev, OFFSET, data, sizeof(data));
	zassert_true(err == 0, "Unexpected failure");

	err = mfu_ext_init(buf, sizeof(buf));
	zassert_true(err == 0, "Unexpected failure");

	err = mfu_ext_load(fdev, OFFSET);
	zassert_true(err == 0, "Unexpected failure");
}

void test_main(void)
{
	ztest_test_suite(lib_mfu_ext_test,
	     ztest_unit_test(test_mfu_ext_load_segments)
	 );

	ztest_run_test_suite(lib_mfu_ext_test);
}
