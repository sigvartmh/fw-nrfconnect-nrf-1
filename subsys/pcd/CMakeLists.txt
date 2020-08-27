#
# Copyright (c) 2020 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#


zephyr_library()

zephyr_library_sources(
  src/pcd.c
  )
if(CONFIG_MCUBOOT AND CONFIG_SOC_NRF5340_CPUAPP)
zephyr_library_sources(
  src/nrf53_cpunet_ctl.c
  )
zephyr_include_directories(./include)
endif()
