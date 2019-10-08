/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <logging/log.h>
#include <dfu/mcuboot.h>
#include <dfu_ctx_mcuboot.h>
#ifdef CONFIG_DFU_CTX_MODEM_UPDATE_SUPPORT
#include <dfu_ctx_modem.h>
#endif
#include <dfu/dfu_context_handler.h>

#define MCUBOOT_IMAGE 1
#define MODEM_DELTA_IMAGE 2
#define MODEM_MAGIC1 0xcb01
#define MODEM_MAGIC2 0x7544656d

struct modem_delta_header
{
	u16_t magic1;
	u16_t reserved;
	u32_t magic2;
};

LOG_MODULE_REGISTER(dfu_context_handler, CONFIG_DFU_CTX_LOG_LEVEL);

typedef int offset_function(void);

struct dfu_ctx {
	int (*init)(void);
	int (*offset)(void);
	int (*write)(const void *const buf, size_t len);
	int (*done)(void);
};

static struct dfu_ctx dfu_ctx_mcuboot = {
	.init  = dfu_ctx_mcuboot_init,
	.offset = dfu_ctx_mcuboot_offset,
	.write = dfu_ctx_mcuboot_write,
	.done  = dfu_ctx_mcuboot_done,
};

static struct dfu_ctx dfu_ctx_modem = {
	.init = dfu_ctx_modem_init,
	.offset = dfu_ctx_modem_offset,
	.write = dfu_ctx_modem_write,
	.done = dfu_ctx_modem_done,
};

static struct dfu_ctx *ctx;
static bool   initialized = false;

static inline void log_img_hdr(struct mcuboot_img_header_v2 *img_hdr)
{
	struct mcuboot_img_sem_ver ver = img_hdr->sem_ver;

	LOG_DBG("magic:\t\t0x%x\n", img_hdr->magic);
	LOG_DBG("load address:\t0x%x\n", img_hdr->load_addr);
	LOG_DBG("header size:\t%d\n", img_hdr->hdr_size);
	LOG_DBG("pad1:\t\t%d\n", img_hdr->_pad1);
	LOG_DBG("image_size:\t%d\n", img_hdr->img_size);
	LOG_DBG("flags:\t0x%x\n", img_hdr->flags);
	LOG_DBG("image version:\t%d.%d.%d.%d\n", ver.major, ver.minor,
		ver.revision, ver.build_num);
}

int dfu_ctx_init(int img_type)
{
	if (!initialized) {
		if (img_type == MCUBOOT_IMAGE) {
			ctx = &dfu_ctx_mcuboot;
		}
#ifdef CONFIG_DFU_CTX_MODEM_UPDATE_SUPPORT
		if (img_type == MODEM_DELTA_IMAGE) {
			ctx = &dfu_ctx_modem;
		}
#endif /* CONFIG_DFU_CTX_LOG_LEVEL */
		if (ctx == NULL) {
			LOG_ERR("Unknown image type");
			return -ENOTSUP;
		}

		initialized = true;

		return ctx->init();
	}

	return 0;
}

int dfu_ctx_write(const void *const buf, size_t len)
{
	__ASSERT_NO_MSG(initialized);

	if (ctx == NULL) {
		return -ESRCH;
	}

	return ctx->write(buf, len);
}


int dfu_ctx_done(void)
{
	__ASSERT_NO_MSG(initialized);

	if (ctx == NULL) {
		return -ESRCH;
	}

	int err;

	err = ctx->done();
	if (err < 0) {
		LOG_ERR("Unable to clean up dfu_ctx");
		return err;
	}
	ctx = NULL;

	initialized = false;

	return 0;
}

int dfu_ctx_offset(void)
{
	__ASSERT_NO_MSG(initialized);

	if (ctx == NULL) {
		return -ESRCH;
	}

	return ctx->offset();
}

int dfu_ctx_img_type(const void *const buf, size_t len)
{
	if (len < BOOT_HEADER_SZ) {
		return -EAGAIN;
	}
	struct mcuboot_img_header_v2 *img_hdr = (struct mcuboot_img_header_v2 *)
						buf;
	log_img_hdr(img_hdr);
	if (img_hdr->magic == BOOT_MAGIC) {
		return MCUBOOT_IMAGE;
	}
#if defined(CONFIG_DFU_CTX_MODEM_UPDATE_SUPPORT)
	struct modem_delta_header *md_hdr = (struct modem_delta_header *) buf;
	if (md_hdr->magic2 == MODEM_MAGIC2){
		return MODEM_DELTA_IMAGE;
	}
#endif /* CONFIG_DFU_CTX_MODEM_UPDATE_SUPPORT */
	LOG_ERR("No supported image type found");
	return -ENOTSUP;
}
