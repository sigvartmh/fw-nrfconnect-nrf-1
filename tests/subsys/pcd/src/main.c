/* Copyright (c) 2020 Nordic Semiconductor ASA
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */
#include <ztest.h>
#include <string.h>
#include <stdbool.h>
#include <zephyr/types.h>
#include <devicetree.h>
#include <drivers/flash.h>
#include <dfu/pcd.h>

#define FLASH_NAME DT_CHOSEN_ZEPHYR_FLASH_CONTROLLER_LABEL

#define BUF_LEN ((32*1024) + 10) /* Some pages, and then some */

#define WRITE_OFFSET 0x80000

#define GET_CMD_PTR() pcd_cmd_write(write_buf, (void *)0xf00ba17, 10, 0)


/* Only 'a' */
static const uint8_t data[BUF_LEN] = {[0 ... BUF_LEN - 1] = 'a'};
static uint8_t read_buf[sizeof(data)];
static uint8_t write_buf[100];

static void test_pcd_cmd_get(void)
{
	struct pcd_cmd *cmd;
	struct pcd_cmd *cmd_2;

	cmd = pcd_cmd_get(write_buf);
	zassert_equal(cmd, NULL, "should be NULL");

	cmd = GET_CMD_PTR();
	zassert_not_equal(cmd, NULL, "should not be NULL");

	cmd_2 = pcd_cmd_get(write_buf);
	zassert_not_equal(cmd, NULL, "should not be NULL");

	zassert_equal_ptr(cmd, cmd_2, "commands are not equal");
}

static void test_pcd_cmd_write(void)
{
	struct pcd_cmd *cmd;

	/* Null checks */
	cmd = pcd_cmd_write(NULL, (const void *)data, sizeof(data),
			    WRITE_OFFSET);
	zassert_equal(cmd, NULL, "should be NULL");

	cmd = pcd_cmd_write(write_buf, NULL, sizeof(data),
			    WRITE_OFFSET);
	zassert_equal(cmd, NULL, "should be NULL");

	cmd = GET_CMD_PTR();
	zassert_not_equal(cmd, NULL, "should not be NULL");
}

static void test_pcd_invalidate(void)
{
	int rc;
	struct pcd_cmd *cmd;

	rc = pcd_invalidate(NULL);
	zassert_true(rc < 0, "Unexpected success");

	cmd = GET_CMD_PTR();
	zassert_not_equal(cmd, NULL, "should not be NULL");

	rc = pcd_status(cmd);
	zassert_true(rc >= 0, "pcd_status should return non-negative int");

	rc = pcd_invalidate(cmd);
	zassert_equal(rc, 0, "Unexpected failure");

	rc = pcd_status(cmd);
	zassert_true(rc < 0, "pcd_status should return negative int");
}


static void test_pcd_fetch(void)
{
	int rc;
	struct pcd_cmd *cmd;
	struct device *fdev;

	cmd = pcd_cmd_write(write_buf, (const void *)data,
			    sizeof(data), WRITE_OFFSET);
	zassert_not_equal(cmd, NULL, "should not be NULL");

	fdev = device_get_binding(FLASH_NAME);
	zassert_true(fdev != NULL, "fdev is NULL");

	rc = pcd_status(cmd);
	zassert_equal(rc, 0, "pcd_status should be zero when not complete");

	/* Null check */
	rc = pcd_fetch(NULL, fdev);
	zassert_not_equal(rc, 0, "Unexpected success");

	rc = pcd_fetch(cmd, NULL);
	zassert_not_equal(rc, 0, "Unexpected success");

	/* Valid fetch */
	rc = pcd_fetch(cmd, fdev);
	zassert_equal(rc, 0, "Unexpected failure");

	rc = pcd_status(cmd);
	zassert_true(rc > 0, "pcd_status should return positive int");

	rc = flash_read(fdev, WRITE_OFFSET, read_buf, sizeof(data));
	zassert_equal(rc, 0, "Unexpected failure");

	zassert_true(memcmp((const void *)data, (const void *)read_buf,
			    sizeof(data)) == 0, "neq");
}

void test_main(void)
{
	ztest_test_suite(pcd_test,
			 ztest_unit_test(test_pcd_cmd_get),
			 ztest_unit_test(test_pcd_cmd_write),
			 ztest_unit_test(test_pcd_invalidate),
			 ztest_unit_test(test_pcd_fetch)
			 );

	ztest_run_test_suite(pcd_test);
}
