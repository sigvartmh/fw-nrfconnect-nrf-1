/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the Modem library.
 *
 * This function is synchrounous and it could block
 * for a few minutes when the modem firmware is being updated.
 *
 * If you application supports modem firmware updates, consider
 * initializing the library manually to have control of what
 * the application should do while initialization is ongoing.
 *
 * @return int Zero on success, non-zero otherwise.
 */
int libmodem_init(void);

/**
 * @brief Makes a thread sleep until next time libmodem_init() is called.
 *
 * When libmodem_shutdown() is called a thread can call this function to be
 * woken up next time libmodem_init() is called.
 */
void libmodem_shutdown_wait(void);

/**
 * @brief Get the last return value of libmodem_init.
 *
 * This function can be used to access the last return value of
 * libmodem_init. This can be used to check the state of a modem
 * firmware exchange when the Modem library was initialized at boot-time.
 *
 * @return int The last return value of libmodem_init.
 */
int libmodem_get_init_ret(void);

/**
 * @brief Shutdown the Modem library, releasing its resources.
 *
 * @return int Zero on success, non-zero otherwise.
 */
int libmodem_shutdown(void);

#ifdef __cplusplus
}
#endif
