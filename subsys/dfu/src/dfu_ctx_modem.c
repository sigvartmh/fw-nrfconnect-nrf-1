#include <zephyr.h>
#include <flash.h>
#include <nrf_socket.h>
#include <net/socket.h>
#include <logging/log.h>

LOG_MODULE_REGISTER(dfu_ctx_modem_ctx, CONFIG_DFU_CTX_LOG_LEVEL);

static int			fd;
static struct k_sem		modem_fw_delete_sem;
static struct k_delayed_work	modem_fw_delete_dwork;

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
	int rc;
	int modem_error;

	LOG_INF("Scheduling modem firmware update at next boot");

	rc = setsockopt(fd, SOL_DFU, SO_DFU_APPLY, NULL, 0);
	if (rc < 0) {
		modem_error = get_modem_error();
		LOG_ERR("Failed to schedule modem firmware update, errno %d, "
			"modem_error:%d", errno, modem_error);
	}
	return 0;
}

static int delete_old_modem_fw(void)
{
	int rc;

	LOG_INF("Deleting firmware image, this could take a while...\n");
	rc = setsockopt(fd, SOL_DFU, SO_DFU_BACKUP_DELETE, NULL, 0);
	if (rc < 0) {
		LOG_ERR("Failed to delete backup, errno %d\n", errno);
		return -EFAULT;
	}
	k_delayed_work_submit(&modem_fw_delete_dwork, K_SECONDS(1));

	return 0;
}

static void delete_old_modem_fw_task(struct k_work *w)
{
	int rc;
	u32_t off;
	int err = 0;
	socklen_t len = sizeof(off);

	/* Request DFU offset from modem to ensure that the old firmware update
	 * has been deleted to make space for the new.
	 */
	rc = getsockopt(fd, SOL_DFU, SO_DFU_OFFSET, &off, &len);
	if (rc < 0) {
		if (errno == ENOEXEC) {
			err = get_modem_error();
			if (err  == DFU_ERASE_PENDING) {
				k_delayed_work_submit(&modem_fw_delete_dwork,
						K_MSEC(500));
			} else {
				LOG_ERR("Unexpected modem error");
			}
		} else {
			LOG_ERR("Unexpected errno: %d", errno);
		}
	} else {
		LOG_INF("Old firmware image deleted");
		k_sem_give(&modem_fw_delete_sem);
	}
}

/**@brief Initialize DFU socket. */
static int modem_dfu_socket_init(void)
{
	int rc;
	socklen_t len;
	u8_t version[36];
	u32_t resource;
	u32_t off;

	/* Create a socket for firmware update */
	fd = socket(AF_LOCAL, SOCK_STREAM, NPROTO_DFU);
	if (fd < 0) {
		LOG_ERR("Failed to open Modem DFU socket.");
		return fd;
	}

	LOG_INF("Modem DFU Socket created");

	len = sizeof(version);
	rc = getsockopt(fd, SOL_DFU, SO_DFU_FW_VERSION, &version,
			    &len);
	if (rc < 0) {
		LOG_ERR("Firmware version request failed, errno %d", errno);
		return -1;
	}

	*((u8_t *)version + sizeof(version) - 1) = '\0';
	LOG_INF("Modem firmware version: %s", log_strdup(version));

	len = sizeof(resource);
	rc = getsockopt(fd, SOL_DFU, SO_DFU_RESOURCES, &resource,
			    &len);
	if (rc < 0) {
		LOG_ERR("Resource request failed, errno %d", errno);
		return -1;
	}

	LOG_INF("Flash memory available: %u", resource);

	len = sizeof(off);
	rc = getsockopt(fd, SOL_DFU, SO_DFU_OFFSET,
			    &off, &len);
	if (rc < 0) {
		LOG_ERR("Offset request failed, errno %d", errno);
		return -1;
	}

	LOG_INF("Offset retrieved: %u", off);

	return rc;
}

int dfu_ctx_modem_init(void)
{
	int err;

	err = modem_dfu_socket_init();
	if (err < 0) {
		return err;
	}

	k_sem_init(&modem_fw_delete_sem, 0, 1);
	k_delayed_work_init(&modem_fw_delete_dwork, delete_old_modem_fw_task);
	delete_old_modem_fw();
	k_sem_take(&modem_fw_delete_sem, K_SECONDS(60*2));

	return 0;
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
	LOG_ERR("Modem refused fragment, errno %d, dfu err %d", errno, modem_error);

	if (modem_error == DFU_AREA_NOT_BLANK ||
	    modem_error == DFU_INVALID_FILE_OFFSET) {
		/* Request delete task: TODO: Make sure the connection
		 * does not close while the delete is being processed
		 */
		delete_old_modem_fw();
		/* Don't wait forever as this would lock the sys */
		k_sem_take(&modem_fw_delete_sem, K_SECONDS(60*2));
		dfu_ctx_modem_write(buf, len);
		return 0;
	}
	if (modem_error == DFU_INVALID_UUID) {
		return -EINVAL;
	}
	return -EFAULT;
}

int dfu_ctx_modem_done(void)
{
	int rc = apply_modem_update();

	if (rc < 0) {
		LOG_ERR("Failed request modem DFU update");
		return rc;
	}

	rc = close(fd);

	if (rc < 0) {
		LOG_ERR("Failed to close modem DFU socket.");
		return rc;
	}

	return 0;
}
