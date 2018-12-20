#
# Copyright (c) 2018 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

add_subdirectory(bl_crypto)
add_subdirectory(c_runtime_setup)
add_subdirectory(provision)
add_subdirectory(fprotect)
add_subdirectory_ifdef(CONFIG_SECURE_BOOT_CRYPTO bl_crypto)
add_subdirectory_ifdef(CONFIG_SECURE_BOOT_DEBUG debug)
		set(PROVISION_ADDR 0x3f000)
	elseif(CONFIG_SOC_NRF51822_QFAB)
		set(PROVISION_ADDR 0x1f000)
	elseif(CONFIG_SOC_NRF51822_QFAC)
		set(PROVISION_ADDR 0x3f000)
	elseif(CONFIG_SOC_NRF52810_QFAA)
		set(PROVISION_ADDR 0x2f000)
	elseif(CONFIG_SOC_NRF52832)


include(${CMAKE_CURRENT_LIST_DIR}/cmake/fw_info_magic.cmake)
zephyr_include_directories(include)
