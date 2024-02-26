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

LOG_MODULE_REGISTER(flash_rpc_ipc_app, 4);

struct flash_msg {
	off_t offset;
	size_t len;
	void* buff;
};

#define VDEV_START_ADDR		DT_REG_ADDR(DT_CHOSEN(zephyr_ipc_shm))
#define VDEV_SIZE		DT_REG_SIZE(DT_CHOSEN(zephyr_ipc_shm)
struct flash_msg *msg_ctx = (struct flash_msg *) VDEV_START_ADDR;
int *result = (int *) WB_UP(VDEV_START_ADDR + sizeof(struct flash_msg));
//int *result = (int *) ROUND_UP((VDEV_START_ADDR + sizeof(struct flash_msg)),32);

//DEVICE_DT_GET(DT_NODELABEL(ipc0))
#define IPC_NODE DT_NODELABEL(ipc0)
//DT_IRQ(IPC_NODE, priority)

int get_result(void) {
	int err = *result;
	*result = 0;
	return err;
}

void ipc_event_handler(uint8_t event_mask, void * p_context)
{
	bool locked = false;
	while(!locked){
		locked = nrf_mutex_lock(NRF_MUTEX, 0);
		LOG_INF("Waiting for mutex: %d", locked);
	}
	struct flash_msg *msg = (struct flash_msg *) p_context; 
	LOG_INF("IPC event handler: %d, %p", event_mask, p_context);
	LOG_INF("Msg: 0x%lx mutex: %d", msg->offset, locked);
	msg->offset = 0xcafebabe;
	int err = get_result();
		k_busy_wait(2500 * MSEC_PER_SEC);
	nrf_mutex_unlock(NRF_MUTEX, 0);
	LOG_INF("Result: %d", err);
	nrfx_ipc_signal(0);
}

int main(void)
{
	nrfx_err_t err;
	*result = 0;
        IRQ_CONNECT(IPC_IRQn, 1, nrfx_ipc_irq_handler, 0, 0);

        LOG_INF("Hello World! %s\n", CONFIG_BOARD);
		err = nrfx_ipc_init(0, ipc_event_handler, &msg_ctx);
        if (err != NRFX_SUCCESS) {
            LOG_INF("nrfx_ipc_init() failed. Error 0x%x\r\n", err);
        }

        nrfx_ipc_send_task_channel_assign(0, 0);
        nrfx_ipc_receive_event_channel_assign(1, 1);
        nrfx_ipc_receive_event_enable(1); 
	err = nrf_mutex_lock(NRF_MUTEX, 1);
	if (err != true) {
		LOG_ERR("Unable to lock initialization mutex");
	}
	while (!nrf_mutex_lock(NRF_MUTEX, 1)) {
	        nrfx_ipc_signal(0);
		#ifdef CONFIG_MULTITHREADING
                k_sleep(K_MSEC(1000));
		#else
		k_busy_wait(1000 * MSEC_PER_SEC);
		#endif
	};
	LOG_INF("Initialized flash_api");


        while(1)
        {
		#ifdef CONFIG_MULTITHREADING
                k_sleep(K_MSEC(250));
		#else
		k_busy_wait(250 * MSEC_PER_SEC);
		#endif
        }
}
