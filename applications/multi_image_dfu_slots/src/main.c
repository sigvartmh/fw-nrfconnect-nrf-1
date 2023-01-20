/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/kernel.h>
#include <zephyr/dfu/mcuboot.h>

#include <pm_config.h>

void find_executing_slot(int* slot);

void main(void)
{
    int ret = 0;
	struct mcuboot_img_header mcuboot_header;
    ret = boot_read_bank_header(PM_MCUBOOT_PRIMARY_ID,
		    &mcuboot_header,
		    sizeof(mcuboot_header));
    if (ret != 0) {
	printk("Failed to read bank header");
    } else {
    printk("\nHello Confirm!: %d,%d,%d\n",
		mcuboot_header.h.v1.sem_ver.major,
		mcuboot_header.h.v1.sem_ver.minor,
		mcuboot_header.h.v1.sem_ver.revision
	  );
    }

    ret = boot_write_img_confirmed();
    printk("Image confirmation returned: %d\n", ret);
}
