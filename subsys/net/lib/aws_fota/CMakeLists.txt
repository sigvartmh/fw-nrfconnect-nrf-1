zephyr_library()
zephyr_library_sources(
	src/aws_fota.c
	src/aws_fota_json.c
)
zephyr_include_directories(./include)
