/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <nrfx_ipc.h>
#include <hal/nrf_mutex.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(flash_rpc_ipc_net, 4);

struct flash_msg {
	off_t offset;
	size_t len;
	void* buff;
};

#define VDEV_START_ADDR		DT_REG_ADDR(DT_CHOSEN(zephyr_ipc_shm))
#define VDEV_SIZE		DT_REG_SIZE(DT_CHOSEN(zephyr_ipc_shm)
struct flash_msg *msg_ctx = (struct flash_msg *) VDEV_START_ADDR;
int *result = (int *) WB_UP(VDEV_START_ADDR + sizeof(struct flash_msg));

void ipc_event_handler(uint8_t event_mask, void * p_context)
{
	nrf_mutex_unlock(NRF_APPMUTEX_S, 1);
	bool locked = false;
	while(!locked){
		locked = nrf_mutex_lock(NRF_APPMUTEX_S, 0);
		LOG_INF("Waiting for mutex: %d", locked);
	}
    struct flash_msg *msg = (struct flash_msg *) p_context; 
    LOG_INF("Event mask 0x%x, context: %p", event_mask, p_context);
    LOG_INF("msg: 0x%lx, result:%d, mutex: %d", msg->offset, *result, locked);
	*result = 1;
	msg->offset = 0xdeadbeef;
		k_busy_wait(2000 * MSEC_PER_SEC);
	nrf_mutex_unlock(NRF_APPMUTEX_S, 0);
    nrfx_ipc_send_task_channel_assign(0,1);
    nrfx_ipc_signal(0);
}

int main(void)
{
        
        IRQ_CONNECT(IPC_IRQn, 1, nrfx_ipc_irq_handler, 0, 0);

        LOG_INF("Hello World! %s\n", CONFIG_BOARD);

        int err = nrfx_ipc_init(0, ipc_event_handler, &msg_ctx);
        if (err != NRFX_SUCCESS) {
            LOG_INF("nrfx_ipc_init() failed. Error 0x%x\r\n", err);
        }
        nrfx_ipc_receive_event_channel_assign(1, 0);
        nrfx_ipc_receive_event_enable(1); 

	while (1) {
           k_msleep(100);
        }
}
