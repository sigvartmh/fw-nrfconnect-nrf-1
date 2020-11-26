#
# Copyright (c) 2020 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

zephyr_library()

zephyr_library_include_directories(include)
zephyr_library_include_directories(src)

zephyr_library_include_directories($ENV{ZEPHYR_BASE}/../nrfxlib/nrf_modem/include)
zephyr_include_directories($ENV{ZEPHYR_BASE}/../modules/lib/mcumgr/cborattr/include)
zephyr_include_directories($ENV{ZEPHYR_BASE}/../modules/lib/mcumgr/mgmt/include)
zephyr_include_directories($ENV{ZEPHYR_BASE}/../nrfxlib/nrf_modem/include)

zephyr_library_sources(
  src/mgmt_mfu.c
  src/modem_mgmt_stat.c
  )
