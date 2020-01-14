#
# Copyright (c) 2018 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

# CMakeLists.txt - cJSON Library for Zephyr

set(UZLIB_PATH ${ZEPHYR_BASE}/../modules/lib/uzlib/src)
zephyr_include_directories(${UZLIB_PATH})
zephyr_library()
zephyr_library_sources(
	${UZLIB_PATH}/tinflate.c
	${UZLIB_PATH}/tinfgzip.c
	${UZLIB_PATH}/tinfzlib.c
	${UZLIB_PATH}/adler32.c
	${UZLIB_PATH}/crc32.c
	${UZLIB_PATH}/defl_static.c
	${UZLIB_PATH}/genlz77.c
)

