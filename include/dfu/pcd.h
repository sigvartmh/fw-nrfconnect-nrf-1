/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/** @file pcd.h
 *
 * @defgroup pcd Peripheral Core DFU
 * @{
 * @brief API for handling DFU of peripheral cores.
 *
 * The PCD provides functions for sending/receiving DFU images between
 * cores on a multi core system.
 */

#ifndef PCD_H__
#define PCD_H__

#ifdef __cplusplus
extern "C" {
#endif
#include <device.h>

#ifdef CONFIG_SOC_SERIES_NRF53X

/* These must be hard coded as this code is preprocessed for both net and app
 * core.
 */
#define APP_CORE_SRAM_START 0x20000000
#define APP_CORE_SRAM_SIZE KB(512)
#define RAM_SECURE_ATTRIBUTION_REGION_SIZE 0x2000
#define PCD_CMD_ADDRESS (APP_CORE_SRAM_START \
			+ APP_CORE_SRAM_SIZE \
			- RAM_SECURE_ATTRIBUTION_REGION_SIZE)
#endif


/** @brief Opaque type */
struct pcd_cmd;

/** @brief Get a PCD CMD from the specified address.
 *
 * @param addr The address to check for a valid PCD CMD.
 *
 * @retval A pointer to the PCD CMD if successful.
 *           Otherwise, NULL is returned.
 */
struct pcd_cmd *pcd_cmd_get(void *addr);

/** @brief Write a PCD CMD to a specified address.
 *
 * @param cmd_addr The address to write the CMD to.
 * @param src_addr The address to which the CMD copy data from.
 * @param len      The number of bytes that should be copied.
 * @param offset   The offset within the flash device to write the data to.
 *
 * @retval A pointer to the written PCD CMD if successful.
 *           Otherwise, NULL is returned.
 */
struct pcd_cmd *pcd_cmd_write(void *cmd_addr, const void *src_addr, size_t len,
		  size_t offset);

/** @brief Update the PCD CMD to invalidate the magic value, indicating that
 * the copy failed.
 *
 * @param cmd The PCD CMD to invalidate.
 *
 * @retval non-negative integer on success, negative errno code on failure.
 */
int pcd_invalidate(struct pcd_cmd *cmd);

/** @brief Update the PCD CMD to invalidate the magic value, indicating that
 * the copy failed.
 *
 * @param cmd The PCD CMD to invalidate.
 *
 * @return 0 if operation is not complete
 * @return negative integer on failure
 * @return positive integer on success
 */
int pcd_status(struct pcd_cmd *cmd);

/** @brief Perform the DFU image transfer.
 *
 * Use the information in the provided PCD CMD to load a DFU image to the
 * provided flash device.
 *
 * @param cmd The PCD CMD whose configuration will be used for the transfer.
 * @param fdev The flash device to transfer the DFU image to.
 *
 * @retval non-negative integer on success, negative errno code on failure.
 */
int pcd_fetch(struct pcd_cmd *cmd, struct device *fdev);

#endif /* PCD_H__ */

/**@} */
