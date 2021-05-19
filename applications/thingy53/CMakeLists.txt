# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(hello_world)

target_sources(app PRIVATE src/main.c)

# Define whole ram as 0x80000
# Could use 0x40000 for 256kb
# then do 0x2000_0000 + 0x40000 = 0x20040000
add_region(
	NAME ram_flash
	SIZE ${0x40000}
	BASE ${0x20040000}
	PLACEMENT complex
	DEVICE "nordic_ram_flash"
	)
