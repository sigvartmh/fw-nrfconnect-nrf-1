#
# Copyright (c) 2019 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

if (CONFIG_SECURE_BOOT)
  zephyr_add_executable(b0 require_build)
  if (${require_build})
    add_subdirectory(bootloader)
  endif ()
endif ()
