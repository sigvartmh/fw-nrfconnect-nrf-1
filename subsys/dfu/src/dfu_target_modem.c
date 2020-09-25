#include <zephyr.h>
#include <stdio.h>
#include <drivers/flash.h>
#include <net/socket.h>
#include <nrf_socket.h>
#include <logging/log.h>
#include <dfu/dfu_target.h>

#define DELETE_WORK_QUEUE_STACK 2048
#define DELETE_WORK_QUEUE_PRIORITY 5
struct k_work_q delete_work_q;
K_THREAD_STACK_DEFINE(delete_work_stack_area, DELETE_WORK_QUEUE_STACK);


LOG_MODULE_REGISTER(dfu_target_modem, CONFIG_DFU_TARGET_LOG_LEVEL);

#define DIRTY_IMAGE 0x280000
#define MODEM_MAGIC 0x7544656d

struct modem_delta_header {
	uint16_t pad1;
	uint16_t pad2;
	uint32_t magic;
};

static int  fd;
static int  offset;
static dfu_target_callback_t callback;

static int get_modem_error(void)
{
	int rc;
	int err = 0;
	socklen_t len;

	len = sizeof(err);
	rc = getsockopt(fd, SOL_DFU, SO_DFU_ERROR, &err, &len);
	if (rc) {
		LOG_ERR("Unable to fetch modem error, errno %d", errno);
	}

	return err;
}

static int apply_modem_upgrade(void)
{
	int err;

	LOG_INF("Scheduling modem firmware upgrade at next boot");

	err = setsockopt(fd, SOL_DFU, SO_DFU_APPLY, NULL, 0);
	if (err < 0) {
		if (errno == ENOEXEC) {
			LOG_ERR("SO_DFU_APPLY failed, modem error %d",
				get_modem_error());
		} else {
			LOG_ERR("SO_DFU_APPLY failed, modem error %d", err);
		}
	}
	return 0;
}
static int delete_error;
static int timeout;
static bool done;
static struct k_delayed_work delete_mfw_w;
#define SLEEP_TIME 1

int start_delete_work(void)
{
	timeout = CONFIG_DFU_TARGET_MODEM_TIMEOUT;
	done = false;
	LOG_INF("Deleting firmware image, this can take several minutes");
	int err = setsockopt(fd, SOL_DFU, SO_DFU_BACKUP_DELETE, NULL, 0);
	if (err < 0) {
		LOG_ERR("Failed to delete backup, errno %d", errno);
		return -EFAULT;
	}
	err = k_delayed_work_submit_to_queue(&delete_work_q, &delete_mfw_w, K_MSEC(500));
	if (err < 0) {
		LOG_ERR("Failed to submit delete work, err %d", err);
		return err;
	}
	return 0;
}

void delete_banked_modem_fw_work(struct k_work *unused)
{
	LOG_INF("Delete work called timeout: %d", timeout);
	socklen_t len = sizeof(offset);
	int err = getsockopt(fd, SOL_DFU, SO_DFU_OFFSET, &offset, &len);

	if (err < 0) {
		if (timeout < 0) {
			LOG_INF("Firing callback: %d", timeout);
			callback(DFU_TARGET_EVT_TIMEOUT);
			timeout = CONFIG_DFU_TARGET_MODEM_TIMEOUT;
			err = k_delayed_work_submit_to_queue(&delete_work_q, &delete_mfw_w, K_MSEC(500));
			if (err < 0) {
				LOG_ERR("Failed to submit delete work, err %d", err);
			}
			return;
		}
		if (errno == ENOEXEC) {
			err = get_modem_error();
			if (err != DFU_ERASE_PENDING) {
				LOG_ERR("DFU error: %d", err);
			}
			err = k_delayed_work_submit_to_queue(&delete_work_q, &delete_mfw_w, K_MSEC(500));
			if (err < 0) {
				LOG_ERR("Failed to submit delete work, err %d", err);
			}
			timeout -= SLEEP_TIME;
			return;
		}
	} 
	LOG_INF("Delete done: %d", timeout);

	done = true;
	delete_error = 0;
	callback(DFU_TARGET_EVT_ERASE_DONE);
	LOG_INF("Modem FW delete complete");

	return;
}

static int delete_banked_modem_fw(void)
{
	int err;
	socklen_t len = sizeof(offset);
	int timeout = CONFIG_DFU_TARGET_MODEM_TIMEOUT;

	LOG_INF("Deleting firmware image, this can take several minutes");
	err = setsockopt(fd, SOL_DFU, SO_DFU_BACKUP_DELETE, NULL, 0);
	if (err < 0) {
		LOG_ERR("Failed to delete backup, errno %d", errno);
		return -EFAULT;
	}
	while (true) {
		err = getsockopt(fd, SOL_DFU, SO_DFU_OFFSET, &offset, &len);
		if (err < 0) {
			if (timeout < 0) {
				callback(DFU_TARGET_EVT_TIMEOUT);
				timeout = CONFIG_DFU_TARGET_MODEM_TIMEOUT;
			}
			if (errno == ENOEXEC) {
				err = get_modem_error();
				if (err != DFU_ERASE_PENDING) {
					LOG_ERR("DFU error: %d", err);
				}
				k_sleep(K_SECONDS(SLEEP_TIME));
			}
			timeout -= SLEEP_TIME;
		} else {
			callback(DFU_TARGET_EVT_ERASE_DONE);
			LOG_INF("Modem FW delete complete");
			break;
		}
	}

	return 0;
}

/**@brief Initialize DFU socket. */
static int modem_dfu_socket_init(void)
{
	int err;
	socklen_t len;
	uint8_t version[36];
	char version_string[37];

	/* Create a socket for firmware upgrade*/
	fd = socket(AF_LOCAL, SOCK_STREAM, NPROTO_DFU);
	if (fd < 0) {
		LOG_ERR("Failed to open Modem DFU socket.");
		return fd;
	}

	LOG_INF("Modem DFU Socket created");

	len = sizeof(version);
	err = getsockopt(fd, SOL_DFU, SO_DFU_FW_VERSION, &version,
			    &len);
	if (err < 0) {
		LOG_ERR("Firmware version request failed, errno %d", errno);
		return -1;
	}

	snprintf(version_string, sizeof(version_string), "%.*s",
		 sizeof(version), version);

	LOG_INF("Modem firmware version: %s", log_strdup(version_string));

	return err;
}

bool dfu_target_modem_identify(const void *const buf)
{
	return ((const struct modem_delta_header *)buf)->magic == MODEM_MAGIC;

}

int dfu_target_modem_init(size_t file_size, dfu_target_callback_t cb)
{
	k_work_q_start(&delete_work_q,
			delete_work_stack_area,
			K_THREAD_STACK_SIZEOF(delete_work_stack_area),
			DELETE_WORK_QUEUE_PRIORITY);
	int err;
	size_t scratch_space;
	socklen_t len = sizeof(offset);

	callback = cb;
	k_delayed_work_init(&delete_mfw_w, delete_banked_modem_fw_work);

	err = modem_dfu_socket_init();
	if (err < 0) {
		return err;
	}

	err = getsockopt(fd, SOL_DFU, SO_DFU_RESOURCES, &scratch_space, &len);
	if (err < 0) {
		if (errno == ENOEXEC) {
			LOG_ERR("Modem error: %d", get_modem_error());
		} else {
			LOG_ERR("getsockopt(OFFSET) errno: %d", errno);
		}
	}

	if (file_size > scratch_space) {
		LOG_ERR("Requested file too big to fit in flash %d > %d",
			file_size, scratch_space);
		return -EFBIG;
	}

	/* Check offset, store to local variable */
	err = getsockopt(fd, SOL_DFU, SO_DFU_OFFSET, &offset, &len);
	if (err < 0) {
		if (errno == ENOEXEC) {
			LOG_ERR("Modem error: %d", get_modem_error());
		} else {
			LOG_ERR("getsockopt(OFFSET) errno: %d", errno);
		}
	}

	if (offset == DIRTY_IMAGE) {
		//delete_banked_modem_fw();
		LOG_INF("Dirty image");
		start_delete_work();
		while(!done){ k_sleep(K_MSEC(100));};
	} else if (offset != 0) {
		LOG_INF("Setting offset to 0x%x", offset);
		len = sizeof(offset);
		err = setsockopt(fd, SOL_DFU, SO_DFU_OFFSET, &offset, len);
		if (err != 0) {
			LOG_INF("Error while setting offset: %d", offset);
		}
	}

	return 0;
}

int dfu_target_modem_offset_get(size_t *out)
{
	*out = offset;
	return 0;
}

int dfu_target_modem_write(const void *const buf, size_t len)
{
	int err = 0;
	int sent = 0;
	int modem_error = 0;
	int send_result = 0;

	while (send_result >= 0) {
		send_result = send(fd, (((uint8_t *)buf) + sent),
				   (len - sent), 0);
		if (send_result > 0) {
			sent += send_result;
			if (sent >= len) {
				offset += len;
				return 0;
			}
		}
	}

	if (errno != ENOEXEC) {
		return -EFAULT;
	}

	modem_error = get_modem_error();
	LOG_ERR("send failed, modem errno %d, dfu err %d", errno, modem_error);
	switch (modem_error) {
	case DFU_INVALID_UUID:
		return -EINVAL;
	case DFU_INVALID_FILE_OFFSET:
		//delete_banked_modem_fw();
		LOG_INF("Invalid file offset");
		start_delete_work();
		while(done != true){ k_sleep(K_MSEC(100));}
		err = dfu_target_modem_write(buf, len);
		if (err < 0) {
			return -EINVAL;
		} else {
			return 0;
		}
	case DFU_AREA_NOT_BLANK:
		LOG_INF("DFU area not blank");
		start_delete_work();
		//delete_banked_modem_fw();
		while(done != true){ k_sleep(K_MSEC(100));}
		err = dfu_target_modem_write(buf, len);
		if (err < 0) {
			return -EINVAL;
		} else {
			return 0;
		}
	default:
		return -EFAULT;
	}
}

int dfu_target_modem_done(bool successful)
{
	int err = 0;

	if (successful) {
		err = apply_modem_upgrade();
		if (err < 0) {
			LOG_ERR("Failed request modem DFU upgrade");
			return err;
		}
	} else {
		LOG_INF("Modem upgrade aborted.");
	}


	err = close(fd);
	if (err < 0) {
		LOG_ERR("Failed to close modem DFU socket.");
		return err;
	}

	return 0;
}
