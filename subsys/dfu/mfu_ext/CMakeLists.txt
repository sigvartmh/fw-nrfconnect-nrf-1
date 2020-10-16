#
# Copyright (c) 2020 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

zephyr_library()

zephyr_library_sources(src/mfu_ext.c)
zephyr_library_sources(src/mfu_decode.c)
# TODO remove these
zephyr_library_sources(src/cbor_decode.c)
zephyr_library_sources(src/cbor_common.c)
zephyr_library_include_directories(include)
