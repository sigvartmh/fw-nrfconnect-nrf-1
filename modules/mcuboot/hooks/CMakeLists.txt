#
# Copyright (c) 2022 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

if(CONFIG_BOOT_IMAGE_ACCESS_HOOKS)
  zephyr_library()
  if(!CONFIG_USE_CUSTOM_BOOT_IMAGE_ACCESS_HOOKS)
    zephyr_library_sources(nrf53_hooks.c)
  else()
    zephyr_library_sources(${CONFIG_CUSTOM_BOOT_IMAGE_ACCESS_HOOKS_PATH})
  endif()
  zephyr_library_link_libraries(MCUBOOT_BOOTUTIL)
endif()
