/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <nrfx_ipc.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(CONFIG_BOARD, LOG_LEVEL_INF);

#define PCD_CMD_MAGIC 0xb5b4b3b6
#ifdef PM_PCD_SRAM_ADDRESS
#define PCD_CMD_ADDRESS PM_PCD_SRAM_ADDRESS
#else
/* extra '_' since its in a different domain */
#define PCD_CMD_ADDRESS PM__PCD_SRAM_ADDRESS
#endif /* PM_PCD_SRAM_ADDRESS */

enum pcd_cmd {
	PCD_CMD_COPY = 0,
	PCD_CMD_COPY_DONE = 1,
	PCD_CMD_COPY_FAILED = 2
	PCD_CMD_READ = 3,
	PCD_CMD_READ_DONE = 4,
	PCD_CMD_READ_FAILED = 5,
};

struct pcd_cmd {
	uint32_t magic; /* Magic value to identify this structure in memory */
	const void *data;     /* Data to copy*/
	size_t len;           /* Number of bytes to copy */
	off_t offset;         /* Offset to store the flash image in */
	enum pcd_cdm_type type;
} __aligned(4);

nrfx_ipc_handler_t ipc_handler(uint8_t event_idx, void * p_context)
{
	struct pcd_cmd *cmd = p_context;

	uint32_t channel = event_idx;
}

static struct pcd_cmd *cmd = (struct pcd_cmd *)PCD_CMD_ADDRESS;

int main(void)
{
	struct ipc_data *shared_mem = PM_PCD_
	nrfx_ipc_init(0, ipc_handler, (void *) cmd);
	IRQ_CONNECT(DT_INST_IRQN(0), DT_INST_IRQ(0, priority), nrfx_isr, nrfx_ipc_irq_handler, 0);
	LOG_INF("Hello world from %s\n", CONFIG_BOARD);

	return 0;
}
