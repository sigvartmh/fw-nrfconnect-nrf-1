#
# Copyright (c) 2019 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

zephyr_library()
zephyr_library_include_directories(include)

zephyr_library_sources(
  src/dfu_context_handler.c
  )
zephyr_library_sources_ifdef(CONFIG_DFU_CTX_MODEM_UPDATE_SUPPORT
  src/dfu_ctx_modem.c
  )
zephyr_library_sources_ifdef(CONFIG_DFU_CTX_MCUBOOT_SUPPORT
  src/dfu_ctx_app.c
  )
