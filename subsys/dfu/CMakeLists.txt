#
# Copyright (c) 2019 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

zephyr_library()
zephyr_library_include_directories(include)

zephyr_library_sources(
  src/dfu_target.c
  )
zephyr_library_sources_ifdef(CONFIG_DFU_CTX_MODEM_UPDATE
  src/dfu_target_modem.c
  )
zephyr_library_sources_ifdef(CONFIG_DFU_CTX_MCUBOOT
  src/dfu_target_mcuboot.c
  )
