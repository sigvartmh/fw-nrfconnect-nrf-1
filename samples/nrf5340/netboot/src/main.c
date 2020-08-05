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
#include <hal/nrf_mutex.h>
#include <dfu/pcd.h>
#include <device.h>

#define CMD_ADDR 0x20000000
#define FLASH_NAME DT_CHOSEN_ZEPHYR_FLASH_CONTROLLER_LABEL

void main(void)
{
	printk("Starting Network bootloader \n\r");
	struct pcd_cmd *cmd;
	int err = fprotect_area(PM_B0N_IMAGE_ADDRESS, PM_B0N_IMAGE_SIZE);
	struct device *fdev = device_get_binding(FLASH_NAME);

	if (err) {
		printk("Failed to protect b0n flash, cancel startup.\n\r");
		return;
	}
	while(!nrf_mutex_lock(NRF_APPMUTEX_S, 0));	
	printk("Aquired mutex 0\n\r");
	cmd = pcd_get_cmd((void*)CMD_ADDR);
	if (cmd != NULL) {
		err = pcd_transfer(cmd, fdev);
		if (err != 0) {
			printk("Failed to transfer image: %d. \n\r", err);
			return;
		}
		printk("PCD cmd = %x\n\r", cmd->magic);
		nrf_mutex_unlock(NRF_APPMUTEX_S, 0);
		printk("Mutex 0 unlocked\n\r");
	}

	uint32_t s0_addr = s0_address_read();
	printk("s0_addr: 0x%x\n\r", s0_addr);

	while(!nrf_mutex_lock(NRF_APPMUTEX_S, 0));	
	if (cmd != NULL) {
		if (!bl_validate_firmware(s0_addr, s0_addr)) {
			err = pcd_invalidate(cmd);
			if (err != 0) {
				printk("Failed invalidation: %d. \n\r", err);
				return;
			}
		}
	}
	nrf_mutex_unlock(NRF_APPMUTEX_S, 0);
	

	err = fprotect_area(PM_APP_ADDRESS, PM_APP_SIZE);
	if (err) {
		printk("Failed to protect app flash: %d. \n\r", err);
		return;
	}

	bl_boot(fw_info_find(s0_addr));
	
	return;
}
