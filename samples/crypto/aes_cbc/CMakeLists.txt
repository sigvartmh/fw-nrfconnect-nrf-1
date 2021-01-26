#
# Copyright (c) 2021 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.13.1)

# Override the binary used by qemu
set(QEMU_KERNEL_OPTION "-device;loader,file=${CMAKE_BINARY_DIR}/tfm_qemu.hex")

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(aes_cbc)

target_sources(app PRIVATE
        src/main.c)
