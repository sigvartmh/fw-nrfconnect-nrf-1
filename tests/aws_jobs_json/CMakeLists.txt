# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)
include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)
project(json)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})

target_sources(app
  PRIVATE
  ${ZEPHYR_BASE}/../nrf/subsys/net/lib/aws_fota/src/aws_fota_json.c
  )

target_include_directories(app
  PRIVATE
  ${ZEPHYR_BASE}/../nrf/subsys/net/lib/aws_fota/include/
  )

target_compile_options(app
  PRIVATE
  -DCONFIG_AWS_IOT_FOTA_HOSTNAME_MAX_LEN=1024
  -DCONFIG_AWS_IOT_FOTA_FILE_PATH_MAX_LEN=1024
  )
