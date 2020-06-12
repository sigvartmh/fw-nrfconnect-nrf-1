#
# Copyright (c) 2020 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

cmake_minimum_required(VERSION 3.8.2)

set(mcuboot_CONF_FILE ${CMAKE_CURRENT_SOURCE_DIR}/mcuboot_prj.conf)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(smp_svr_external_flash)

set(SMP_SVR_PATH $ENV{ZEPHYR_BASE}/samples/subsys/mgmt/mcumgr/smp_svr)
target_sources(app PRIVATE ${SMP_SVR_PATH}/src/main.c)
target_sources_ifdef(CONFIG_MCUMGR_SMP_BT app PRIVATE ${SMP_SVR_PATH}/src/bluetooth.c)
target_sources_ifdef(CONFIG_MCUMGR_SMP_UDP app PRIVATE ${SMP_SVR_PATH}/src/udp.c)
target_include_directories(app PRIVATE $ENV{ZEPHYR_BASE}/../modules/fs/littlefs)
