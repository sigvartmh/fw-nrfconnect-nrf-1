/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr/types.h>
#include <sys/printk.h>
#include <pm_config.h>
#include <fw_info.h>
#include <fprotect.h>
#include <bl_storage.h>
#include <bl_boot.h>
#include <bl_validation.h>
#include <dfu/pcd.h>
#include <device.h>

#define FLASH_NAME DT_CHOSEN_ZEPHYR_FLASH_CONTROLLER_LABEL

void main(void)
{
	struct pcd_cmd *cmd = NULL;
	int err = fprotect_area(PM_B0N_IMAGE_ADDRESS, PM_B0N_IMAGE_SIZE);
	struct device *fdev = device_get_binding(FLASH_NAME);

	if (err) {
		printk("Failed to protect b0n flash, cancel startup\n\r");
		goto failure;
	}

	cmd = pcd_cmd_get((void *)PCD_CMD_ADDRESS);
	if (cmd != NULL) {
		err = pcd_fetch(cmd, fdev);
		if (err != 0) {
			printk("Failed to transfer image: %d\n\r", err);
			goto failure;
		}

		/* Success, waiting to be rebooted */
		while (1)
			;
		CODE_UNREACHABLE;
	}

	uint32_t s0_addr = s0_address_read();

	if (cmd != NULL) {
		if (!bl_validate_firmware(s0_addr, s0_addr)) {
			err = pcd_invalidate(cmd);
			if (err != 0) {
				printk("Failed invalidation: %d\n\r", err);
				goto failure;
			}
		}
	}

	err = fprotect_area(PM_APP_ADDRESS, PM_APP_SIZE);
	if (err) {
		printk("Failed to protect app flash: %d\n\r", err);
		goto failure;
	}

	bl_boot(fw_info_find(s0_addr));
	return;

failure:
	pcd_invalidate(cmd);
}
