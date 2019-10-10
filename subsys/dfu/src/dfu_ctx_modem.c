#include <zephyr.h>
#include <flash.h>
#include <nrf_socket.h>
#include <net/socket.h>
#include <logging/log.h>

LOG_MODULE_REGISTER(dfu_ctx_modem_ctx, CONFIG_DFU_CTX_LOG_LEVEL);

#define DIRTY_IMAGE 2621440
#define MODEM_MAGIC 0x7544656d

struct modem_delta_header {
	u16_t pad1;
	u16_t pad2;
	u32_t magic;
};

static int  fd;
static int  offset;

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

static int apply_modem_update(void)
{
	int err;
	int modem_error;

	LOG_INF("Scheduling modem firmware update at next boot");

	err = setsockopt(fd, SOL_DFU, SO_DFU_APPLY, NULL, 0);
	if (err < 0) {
		modem_error = get_modem_error();
		LOG_ERR("Failed to schedule modem firmware update, errno %d, "
			"modem_error:%d", errno, modem_error);
	}
	return 0;
}

static int delete_banked_modem_fw(void)
{
	int err;
	socklen_t len = sizeof(offset);

	LOG_INF("Deleting firmware image, this can take several minutes");
	err = setsockopt(fd, SOL_DFU, SO_DFU_BACKUP_DELETE, NULL, 0);
	if (err < 0) {
		LOG_ERR("Failed to delete backup, errno %d", errno);
		return -EFAULT;
	}

	while (true) {
		err = getsockopt(fd, SOL_DFU, SO_DFU_OFFSET, &offset, &len);
		if (err != 0) {
			if (err == ENOEXEC) {
				err = get_modem_error();
				if (err != DFU_ERASE_PENDING) {
					LOG_ERR("DFU error: %d", err);
				}
			} else {
				k_sleep(K_MSEC(500));
			}
		} else {
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
	u8_t version[36];

	/* Create a socket for firmware update */
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

	version[sizeof(version) - 1] = '\0';

	LOG_INF("Modem firmware version: %s", log_strdup(version));

	return err;
}

bool dfu_ctx_modem_identify(const void *const buf)
{
	return ((struct modem_delta_header *)buf)->magic == MODEM_MAGIC;

}

int dfu_ctx_modem_init(void)
{
	int err;
	socklen_t len = sizeof(offset);

	err = modem_dfu_socket_init();
	if (err < 0) {
		return err;
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
		delete_banked_modem_fw();
	} else if (offset != 0) {
		LOG_INF("Setting offset to 0x%x", offset);
		err = setsockopt(fd, SOL_DFU, SO_DFU_OFFSET, &offset, 4);
		if (err != 0) {
			LOG_INF("Error while setting offset: %d", offset);
		}
	}

	return 0;
}

int dfu_ctx_modem_offset(void)
{
	return offset;
}

int dfu_ctx_modem_write(const void *const buf, size_t len)
{
	int sent = 0;
	int modem_error = 0;

	sent = send(fd, buf, len, 0);
	if (sent > 0) {
		return 0;
	}

	modem_error = get_modem_error();
	LOG_ERR("send failed, modem errno %d, dfu err %d", errno, modem_error);

	if (modem_error == DFU_INVALID_UUID) {
		return -EINVAL;
	}

	return -EFAULT;
}

int dfu_ctx_modem_done(void)
{
	int err = apply_modem_update();

	if (err < 0) {
		LOG_ERR("Failed request modem DFU update");
		return err;
	}

	err = close(fd);

	if (err < 0) {
		LOG_ERR("Failed to close modem DFU socket.");
		return err;
	}

	return 0;
}
