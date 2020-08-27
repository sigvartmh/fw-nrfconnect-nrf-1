/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <device.h>
#include <dfu/pcd.h>
#include <logging/log.h>
#include <storage/stream_flash.h>

LOG_MODULE_REGISTER(pcd, CONFIG_PCD_LOG_LEVEL);

/** Magic value written to indicate that a copy should take place. */
#define PCD_CMD_MAGIC_COPY 0xb5b4b3b6
/** Magic value written to indicate that a something failed. */
#define PCD_CMD_MAGIC_FAIL 0x25bafc15
/** Magic value written to indicate that a copy is done. */
#define PCD_CMD_MAGIC_DONE 0xf103ce5d

struct pcd_cmd {
	uint32_t magic; /* Magic value to identify this structure in memory */
	const void *src_addr; /* Source address to copy from */
	size_t len;           /* Number of bytes to copy */
	size_t offset;        /* Offset to store the flash image in */
} __aligned(4);


struct pcd_cmd *pcd_cmd_get(void *addr)
{
	struct pcd_cmd *cmd = (struct pcd_cmd *)addr;

	if (cmd->magic != PCD_CMD_MAGIC_COPY) {
		return NULL;
	}

	return cmd;
}

struct pcd_cmd *pcd_cmd_write(void *cmd_addr, const void *src_addr, size_t len,
			      size_t offset)
{
	struct pcd_cmd *cmd = (struct pcd_cmd *)cmd_addr;

	if (cmd_addr == NULL || src_addr == NULL || len == 0) {
		return NULL;
	}

	cmd->magic = PCD_CMD_MAGIC_COPY;
	cmd->src_addr = src_addr;
	cmd->len = len;
	cmd->offset = offset;

	return cmd;
}

int pcd_invalidate(struct pcd_cmd *cmd)
{
	if (cmd == NULL) {
		return -EINVAL;
	}

	cmd->magic = PCD_CMD_MAGIC_FAIL;

	return 0;
}

int pcd_status(struct pcd_cmd *cmd)
{
	if (cmd->magic == PCD_CMD_MAGIC_COPY) {
		return 0;
	} else if (cmd->magic == PCD_CMD_MAGIC_DONE) {
		return 1;
	} else {
		return -1;
	}
}

int pcd_fetch(struct pcd_cmd *cmd, struct device *fdev)
{
	struct stream_flash_ctx stream;
	uint8_t buf[CONFIG_PCD_BUF_SIZE];
	int rc;

	if (cmd == NULL) {
		return -EINVAL;
	}

	rc = stream_flash_init(&stream, fdev, buf, sizeof(buf),
			       cmd->offset, 0, NULL);
	if (rc != 0) {
		LOG_ERR("stream_flash_init failed: %d", rc);
		return rc;
	}

	rc = stream_flash_buffered_write(&stream, (uint8_t *)cmd->src_addr,
					 cmd->len, true);
	if (rc != 0) {
		LOG_ERR("stream_flash_buffered_write fail: %d", rc);
		return rc;
	}

	LOG_INF("Transfer done");

	/* Signal complete by setting magic to DONE */
	cmd->magic = PCD_CMD_MAGIC_DONE;

	return 0;
}
