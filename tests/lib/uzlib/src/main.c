/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <devicetree.h>
#include <drivers/flash.h>
#include <ztest.h>
#include <sys/printk.h>
#include <kernel.h>
#include <uzlib.h>
#include <string.h>
/*
#include "test_data/app_update.h"
#include "test_data/app_update.gz.h"
#include "test_data/app_update.lzip.h"
#include "test_data/a.h"
#include "test_data/a.gz.h"
#include "test_data/aaa.h"
#include "test_data/aaa.gz.h"
#include "test_data/alphabet.h"
#include "test_data/alphabet.gz.h"
*/
#include "test_data/alice29.h"
#include "test_data/alice29.gz.h"

#define FLASH_SIZE DT_SOC_NV_FLASH_0_SIZE
#define FLASH_NAME DT_FLASH_DEV_NAME 

#define FLASH_BASE (64*1024)

static struct device *flash_dev;
static size_t layout_size;
static int page_size;
static const struct flash_driver_api *fapi;
static const struct flash_pages_layout *layout;

#define K32 32768
unsigned char look_back_buffer[K32];
unsigned char temp_buf[256];
unsigned int offset = 0;

#define BUFFER_SIZE
unsigned int counter = 0;
int read_source_cb(struct uzlib_uncomp *data)
{
	if (data->source >= (alice29_txt + alice29_txt_len-1)){
		return -1;
	}else {
		return 4;
	}
}

int decompress_stream(unsigned char * source, size_t len, unsigned char *dest)
{
	struct uzlib_uncomp data;
	unsigned char dict[32*1024];
	unsigned int dlen = 4*len;
	unsigned int outlen = dlen;
	uzlib_uncompress_init(&data, dict, 32*1024);
	data.source = source;
	data.source_limit = source + len - 4;
	data.source_read_cb = read_source_cb;
	printk("Decompress stream\n");
	
	int res = uzlib_gzip_parse_header(&data);
	if (res != TINF_OK) {
		printk("Error parsing header: %d\n", res);
		zassert_true(false, "dummy");
	}
	unsigned char buf[256];
	data.dest_start = buf;
	data.dest_limit = buf + 1;
	/* destination is larger than compressed */
	/* Add extra byte incase header is out of bounds */
	unsigned int i = 0;
	unsigned int pos = 0;
	unsigned int decompressed = 0;
	while(1){
		data.dest = &buf[pos];
		unsigned int chunk_len = dlen < 1 ? dlen : 1;
		res = uzlib_uncompress_chksum(&data);
		dlen -= chunk_len;
		if(res != TINF_OK){
			break;
		}
		zassert_equal(alice29_txt[decompressed], buf[pos], "alice29_txt[%d]: %c != %c out_buf[%d]", decompressed, alice29_txt[decompressed], buf[pos], pos);
		pos++;
		decompressed += 1;
		if (pos == 256) {
			//memcpy(dest+56, buf, 256);
			pos = 0;
		}

	}
	if (res != TINF_DONE){
		zassert_not_equal(res, TINF_DONE, "decompress CRC failure: %d\n", res);
	}
	printk("Decompressed %d bytes\n", decompressed);
	return 0;
}

int decompress(unsigned char * source, size_t len, unsigned char *dest)
{
	struct uzlib_uncomp data;
	unsigned int dlen = 4*len;
	unsigned int outlen = dlen;
	unsigned char dict[32*1024];
	uzlib_uncompress_init(&data, dict, 32*1024);
	data.source = source;
	data.source_limit = source + len - 4;
	//data.source_read_cb = read_source_cb;
	int res = uzlib_gzip_parse_header(&data);
	if (res != TINF_OK) {
		printk("Error parsing header: %d\n", res);
		zassert_true(false, "dummy");
	}
	data.dest_start = data.dest = dest;
	/* destination is larger than compressed */
	/* Add extra byte incase header is out of bounds */
	unsigned int start = 0;
	while(1){
		unsigned int chunk_len = dlen < 12 ? dlen : 12;
		data.dest_limit = data.dest + chunk_len;
		res = uzlib_uncompress_chksum(&data);
		if(res != TINF_OK){
			break;
		}
		dlen -= chunk_len;
	}

	if (res != TINF_DONE){
		zassert_not_equal(res, TINF_DONE, "decompress CRC failure: %d\n", res);
	}
	printk("Decompressed %d bytes\n", data.dest - dest);
	return 0;
}

int decompress_lzip(unsigned char * source, size_t len, unsigned char *dest)
{
	struct uzlib_uncomp data;
	unsigned int dlen = 4*len;
	unsigned int outlen = dlen;
	uzlib_uncompress_init(&data, NULL, 0);
	data.source = source;
	data.source_limit = source + len - 4;
	//data.source_read_cb = read_source_cb;
	
	int res = uzlib_zlib_parse_header(&data);
	if (res != TINF_OK) {
		printk("Error parsing header: %d\n", res);
	}
	data.dest_start = data.dest = dest;
	/* destination is larger than compressed */
	/* Add extra byte incase header is out of bounds */
	while(1){
		unsigned int chunk_len = dlen < 32 ? dlen : 32;
		data.dest_limit = data.dest + chunk_len;
		res = uzlib_uncompress_chksum(&data);
		dlen -= chunk_len;
		if(res != TINF_OK){
			break;
		}
	}
	if (res != TINF_DONE){
		zassert_not_equal(res, TINF_DONE, "decompress CRC failure: %d\n", res);
	}
	printk("Decompressed %d bytes\n", data.dest - dest);
	return 0;
}

/*
static void test_compressed_a(void)
{
	unsigned char out_buf[] = {[0 ... ARRAY_SIZE(a_txt) - 1] = 'b'};
	decompress(a_txt_gz, a_txt_gz_len, out_buf);
	zassert_true(true, "dummy");
}

static void test_compressed_aaa(void)
{
	unsigned char out_buf[] = {[0 ... ARRAY_SIZE(aaa_txt) - 1] = 'b'};
	decompress(aaa_txt_gz, aaa_txt_gz_len, out_buf);
	for (size_t i = 0; i < aaa_txt_len; i++) {
		zassert_equal(aaa_txt[i], out_buf[i], "aaa_txt[i]: %c != %c out_buf[i]", aaa_txt[i], out_buf[i]);
	}
}

static void test_compressed_alphabet(void)
{
	unsigned char out_buf[] = {[0 ... ARRAY_SIZE(alphabet_txt) - 1] = 'x'};
	decompress(alphabet_txt_gz, alphabet_txt_gz_len, out_buf);
	for (size_t i = 0; i < alphabet_txt_len; i++) {
		zassert_equal(alphabet_txt[i], out_buf[i], "alphabet_txt[i]: %c != %c out_buf[i]", alphabet_txt[i], out_buf[i]);
	}
}
*/
static void test_compressed_alice_stream(void)
{
	decompress_stream(alice29_txt_gz, alice29_txt_gz_len, NULL);
}
/*
static void test_compressed_gz_firmware(void)
{
	unsigned char out_buf[app_update_bin_len];
	decompress(app_update_bin_gz, app_update_bin_gz_len, out_buf);
	for (size_t i = 0; i < app_update_bin_len; i++) {
		zassert_equal(app_update_bin[i], out_buf[i], "app_update_bin[i]: %c != %c out_buf[i]", app_update_bin[i], out_buf[i]);
	}
}

static void test_compressed_lzip_firmware(void)
{
	unsigned char out_buf[app_update_bin_len];
	decompress_lzip(app_update_bin_lzip, app_update_bin_lzip_len, out_buf);
	for (size_t i = 0; i < app_update_bin_len; i++) {
		zassert_equal(app_update_bin[i], out_buf[i], "app_update_bin[i]: %c != %c out_buf[i]", app_update_bin[i], out_buf[i]);
	}
}

static void setup_flash(void)
{
}
*/

void test_main(void)
{
	flash_dev = device_get_binding(FLASH_NAME);
	fapi = flash_dev->driver_api;
	fapi->page_layout(flash_dev, &layout, &layout_size);
	uzlib_init();
	ztest_test_suite(decompress_test,
			/*
		ztest_unit_test(test_compressed_a),
		ztest_unit_test(test_compressed_aaa),
		*/
			/*
		ztest_unit_test(test_compressed_alphabet),
		*/
		ztest_unit_test(test_compressed_alice_stream)
		//ztest_unit_test(test_compressed_gz_firmware)
		/*
		ztest_unit_test(test_compressed_lzip_firmware)
		*/
	);
	ztest_run_test_suite(decompress_test);
}
