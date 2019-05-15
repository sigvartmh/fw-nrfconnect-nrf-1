#include <stdint.h>
#include <zephyr.h>
#include <flash.h>
#include <download_client.h>
#include <dfu/mcuboot.h>
/* Support both PM and dtc */
#include <pm_config.h>

static bool		is_flash_page_erased[FLASH_PAGE_MAX_CNT];
static struct		device	*flash_dev;
static u32_t		flash_address;

/* Forward decleration of download_client_event_handler */
static int download_client_evt_handler(struct download_client *const dfu,
			enum download_client_evt event, u32_t status);

/**@brief Initialize flash device and set flash start addr. */
static int flash_init(void)
{

	flash_address = PM_MCUBOOT_PARTITIONS_SECONDARY_ADDRESS;
	for (int i = 0; i < FLASH_PAGE_MAX_CNT; i++) {
		is_flash_page_erased[i] = false;
	}
	flash_dev = device_get_binding(DT_FLASH_DEV_NAME);
	if (flash_dev == 0) {
		printk("Nordic nRF flash driver was not found!\n");
		return 1;
	}

}

static int flash_page_erase_if_needed(u32_t address)
{
	int err;
	struct flash_pages_info info;

	err = flash_get_page_info_by_offs(flash_dev, address,
		&info);
	if (err != 0) {
		printk("flash_get_page_info_by_offs returned error %d\n",
			err);
		return 1;
	}
	if (!is_flash_page_erased[info.index]) {
		err = flash_write_protection_set(flash_dev, false);
		if (err != 0) {
			printk("flash_write_protection_set returned error %d\n",
				err);
			return 1;
		}
		err = flash_erase(flash_dev, info.start_offset, info.size);
		if (err != 0) {
			printk("flash_erase returned error %d at address %08x\n",
				err, info.start_offset);
			return 1;
		}
		is_flash_page_erased[info.index] = true;
		err = flash_write_protection_set(flash_dev, true);
		if (err != 0) {
			printk("flash_write_protection_set returned error %d\n",
				err);
			return 1;
		}
	}
	return 0;
}

int start_dfu(const char * hostname,
	       const char * resource_path)
{
	flash_init();
	printk(" hostname: %s, resource: %s\n\r", hostname, resource_path);
	struct download_client dfu = {
		.host = hostname,
		.resource = resource_path,
		.callback = download_client_evt_handler
	};

	int retval = download_client_init(&dfu);

	if (retval != 0) {
		printk("download_client_init() failed, err %d", retval);
		return -EFAULT;
	}
	retval = download_client_connect(&dfu);

	if (retval != 0) {
		printk("download_client_connect() failed, err %d",
			retval);
		return -EFAULT;
	}

	retval = download_client_start(&dfu);
	if (retval != 0) {
		printk("download_client_start() failed, err %d",
			retval);
		return -EFAULT;
	}
	return 0;
}



static int download_client_evt_handler(
	struct download_client * const dfu,
	enum download_client_evt event, u32_t error)
{
	int err;

	switch (event) {
	case DOWNLOAD_CLIENT_EVT_DOWNLOAD_FRAG: {

		if (dfu->object_size > PM_MCUBOOT_PARTITIONS_SECONDARY_SIZE) {
			printk("Requested file too big to fit in flash\n");
			return 1;
		}
		err = flash_page_erase_if_needed(flash_address);
		if (err != 0) {
			return 1;
		}
		err = flash_write_protection_set(flash_dev, false);
		if (err != 0) {
			printk("flash_write_protection_set returned error %d\n",
				err);
			return 1;
		}
		err = flash_write(flash_dev, flash_address, dfu->fragment,
							dfu->fragment_size);
		if (err != 0) {
			printk("Flash write error %d at address %08x\n",
				err, flash_address);
			return 1;
		}
		err = flash_write_protection_set(flash_dev, true);
		if (err != 0) {
			printk("flash_write_protection_set returned error %d\n",
				err);
			return 1;
		}
		flash_address += dfu->fragment_size;
		break;
	}

	case DOWNLOAD_CLIENT_EVT_DOWNLOAD_DONE:
		flash_address = PM_MCUBOOT_PARTITIONS_SECONDARY_ADDRESS
			+PM_MCUBOOT_PARTITIONS_SECONDARY_SIZE-0x4;
		err = flash_page_erase_if_needed(flash_address);
		if (err != 0) {
			return 1;
		}
		err = boot_request_upgrade(0);
		if (err != 0) {
			printk("boot_request_upgrade returned error %d\n", err);
			return 1;
		}
		printk("DOWNLOAD_CLIENT_EVT_DOWNLOAD_DONE");
		break;

	case DOWNLOAD_CLIENT_EVT_ERROR: {
		download_client_disconnect(dfu);
		printk("Download client error, please restart "
			"the application\n");
		break;
	}
	default:
		break;
	}
	return 0;
}

