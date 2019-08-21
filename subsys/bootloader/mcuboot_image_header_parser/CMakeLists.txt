#
# Copyright (c) 2019 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

zephyr_library()
zephyr_library_sources(mcuboot_image_header_parser.c)
zephyr_library_include_directories($ENV{ZEPHYR_BASE}/../mcuboot/boot/bootutil/include/bootutil)
