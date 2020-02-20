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
#include "test_data/app_update.h"
#include "test_data/app_update.gz.h"
#include "test_data/app_update.lzip.h"
#include "test_data/app_update.lz.h"
#include "test_data/a.h"
#include "test_data/a.gz.h"
#include "test_data/aaa.h"
#include "test_data/aaa.gz.h"
#include "test_data/alphabet.h"
#include "test_data/alphabet.gz.h"
#include "test_data/alice29.h"
#include "test_data/alice29.gz.h"

#ifdef CONFIG_BOARD_NATIVE_POSIX
#endif

#define FLASH_SIZE DT_SOC_NV_FLASH_0_SIZE
#define FLASH_NAME DT_FLASH_DEV_NAME 

#define FLASH_BASE (64*1024)

static struct device *flash_dev;
static size_t layout_size;
static int page_size;
static const struct flash_driver_api *fapi;
static const struct flash_pages_layout *layout;

#define K32 32768
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

unsigned char dict[K32];
int decompress_stream(const unsigned char * source, size_t len, const unsigned char *expected)
{
	struct uzlib_uncomp data;
	unsigned int dlen = 4*len;
	unsigned int outlen = dlen;
	uzlib_uncompress_init(&data, dict, K32);
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
		zassert_equal(expected[decompressed], buf[pos], "expected[%d]: "
			      "%c != %c out_buf[%d]", decompressed,
			      expected[decompressed], buf[pos], pos);
		pos++;
		decompressed += 1;
		if (pos == 256) {
			pos = 0;
		}

	}
	if (res != TINF_DONE){
		zassert_not_equal(res, TINF_DONE, "decompress CRC failure: %d\n", res);
	}
	printk("Decompressed %d bytes\n", decompressed);
	return decompressed;
}

int decompress_stream_zlib(const unsigned char * source, size_t len, const unsigned char *expected)
{
	struct uzlib_uncomp data;
	unsigned int dlen = 4*len;
	unsigned int outlen = dlen;
	uzlib_uncompress_init(&data, dict, K32);
	data.source = source;
	data.source_limit = source + len - 4;
	data.source_read_cb = read_source_cb;
	printk("Decompress stream\n");
	
	int res = uzlib_zlib_parse_header(&data);
	if (res != 7) {
		zassert_equal(res, 7, "Error parsing header: %d\n", res);
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
		zassert_equal(expected[decompressed], buf[pos], "expected[%d]: "
			      "%c != %c out_buf[%d]", decompressed,
			      expected[decompressed], buf[pos], pos);
		pos++;
		decompressed += 1;
		if (pos == 256) {
			pos = 0;
		}

	}
	if (res != TINF_DONE){
		zassert_not_equal(res, TINF_DONE, "decompress CRC failure: %d\n", res);
	}
	printk("Decompressed %d bytes\n", decompressed);
	return decompressed;
}

int decompress(const unsigned char * source, size_t len, unsigned char *dest)
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
		zassert_equal(res, TINF_OK, "Error parsing header: %d\n", res);
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
	return data.dest -dest;
}

int decompress_zlib(const unsigned char * source, size_t len, unsigned char *dest)
{
	struct uzlib_uncomp data;
	unsigned int dlen = 4*len;
	unsigned int outlen = dlen;
	unsigned char dict[32*1024];
	uzlib_uncompress_init(&data, dict, 32*1024);
	data.source = source;
	data.source_limit = source + len - 4;
	//data.source_read_cb = read_source_cb;
	int res = uzlib_zlib_parse_header(&data);
	if (res != TINF_OK) {
		zassert_equal(res, 7, "Error parsing header: %d\n", res);
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
	return data.dest -dest;
}

static void test_compressed_alphabet(void) {
	int decompressed;
	decompressed = decompress_stream(alphabet_txt_gz, alphabet_txt_gz_len,
				      alphabet_txt);
	zassert_equal(decompressed, alphabet_txt_len,
		     "Decompressed: %d bytes expected %d bytes", decompressed,
		     alphabet_txt_len);
#ifdef  CONFIG_BOARD_NATIVE_POSIX
	unsigned char out_buf[] = {[0 ... alphabet_txt_len] = 'x'};
	decompressed = decompress(alphabet_txt_gz, alphabet_txt_gz_len, out_buf);
	for (size_t i = 0; i < alphabet_txt_len; i++) {
		zassert_equal(alphabet_txt[i], out_buf[i], "alphabet_txt[i]: %c != %c out_buf[i]", alphabet_txt[i], out_buf[i]);
	}
	zassert_equal(decompressed, alphabet_txt_len,
		     "Decompressed: %d bytes expected %d bytes", decompressed,
		     alphabet_txt_len);
#endif
}

static void test_compressed_alice_stream(void)
{
	int decompressed;

	decompressed = decompress_stream(alice29_txt_gz, alice29_txt_gz_len,
					     alice29_txt);
	zassert_equal(decompressed, alice29_txt_len,
		     "Decompressed: %d bytes expected %d bytes", decompressed,
		     alice29_txt_len);
#ifdef  CONFIG_BOARD_NATIVE_POSIX
	unsigned char out_buf[] = {[0 ... alice29_txt_len] = 'x'};
	decompressed = decompress(alice29_txt_gz, alice29_txt_gz_len, out_buf);
	zassert_equal(decompressed, alice29_txt_len,
		     "Decompressed: %d bytes expected %d bytes", decompressed,
		     alice29_txt_len);
	for (size_t i = 0; i < alice29_txt_len; i++) {
		zassert_equal(alice29_txt[i], out_buf[i], "alice29_txt[%d]: %c "
		"!= %c out_buf[%d]", i, alice29_txt[i], out_buf, i);
	}
#endif
}
static void test_compressed_gz_firmware(void)
{
	int decompressed;
	decompressed = decompress_stream(app_update_bin_gz, app_update_bin_gz_len, app_update_bin);
	zassert_equal(decompressed, app_update_bin_len,
		     "Decompressed: %d bytes expected %d bytes", decompressed,
		     app_update_bin_len);

#ifdef  CONFIG_BOARD_NATIVE_POSIX
	unsigned char out_buf[] = {[0 ... app_update_bin_len] = 'x'};
	decompressed = decompress(app_update_bin_gz, app_update_bin_gz_len, out_buf);
	zassert_equal(decompressed, app_update_bin_len,
		     "Decompressed: %d bytes expected %d bytes", decompressed,
		     app_update_bin_len);
	for (size_t i = 0; i < app_update_bin_len; i++) {
		zassert_equal(app_update_bin[i], out_buf[i], "app_update_bin[i]: %c != %c out_buf[i]", app_update_bin[i], out_buf[i]);
	}
#endif
}

/* 2 Bytes compressed to something bigger */
static void test_compressed_a(void)
{
	int decompressed;
	decompressed = decompress_stream(a_txt_gz, a_txt_gz_len, a_txt);
	zassert_equal(decompressed, a_txt_len,
		     "Decompressed: %d bytes expected %d bytes", decompressed,
		     a_txt_len);
#if 1 
#ifdef  CONFIG_BOARD_NATIVE_POSIX
	unsigned char out_buf[] = {[0 ... a_txt_len] = 'x'};
	decompressed = decompress(a_txt_gz, a_txt_gz_len, out_buf);
	zassert_equal(decompressed, a_txt_len,
		     "Decompressed: %d bytes expected %d bytes", decompressed,
		     a_txt_len);
	for (size_t i = 0; i < a_txt_len; i++) {
		zassert_equal(a_txt[i], out_buf[i], "a_txt[i]: %c != %c out_buf[i]", app_update_bin[i], out_buf[i]);
	}
#endif
#endif
}

static void test_compressed_aaa(void)
{
	int decompressed;
	decompressed = decompress_stream(aaa_txt_gz, aaa_txt_gz_len, aaa_txt);
	zassert_equal(decompressed, aaa_txt_len,
		     "Decompressed: %d bytes expected %d bytes", decompressed, aaa_txt_len);

#ifdef  CONFIG_BOARD_NATIVE_POSIX
	FILE * fin = fopen("../src/test_data/app_update.bin.gz", "r");
	if (fin == NULL) {
		printk("File not found\n");
		exit(1);
	}
	printk("fseek\n");
	fseek(fin, 0, SEEK_END);
	int len = ftell(fin);
	fseek(fin, 0, SEEK_SET);
	printk("fseek set\n");
	const unsigned char * source = k_malloc(len);
	printk("malloc\n");
	if(source == NULL)
	{
		printk("No memory");
		exit(1);
	}
	printk("read\n");
	if(fread(source, 1, len, fin) != len)
	{
		printk("Can't read file");
	}
	fclose(fin);
	printk("Close\n");

	
    unsigned int dlen =            source[len - 1];
    dlen = 256*dlen + source[len - 2];
    dlen = 256*dlen + source[len - 3];
    dlen = 256*dlen + source[len - 4];
    dlen++;
    printk("dlen: %d", dlen);
	unsigned char * out_buf = k_malloc(aaa_txt_len);
	decompressed = decompress(source, len, out_buf);
	zassert_equal(decompressed, aaa_txt_len,
		     "Decompressed: %d bytes expected %d bytes", decompressed,
		     aaa_txt_len);
	for (size_t i = 0; i < aaa_txt_len; i++) {
		zassert_equal(aaa_txt[i], out_buf[i], "aaa_txt[i]: %c != %c out_buf[i]", app_update_bin[i], out_buf[i]);
	}
#endif
}

static void test_compressed_zlib_firmware(void)
{
	int decompressed;
	decompressed = decompress_stream_zlib(app_update_bin_lzip, app_update_bin_lzip_len, app_update_bin);
#ifdef  CONFIG_BOARD_NATIVE_POSIX
	unsigned char out_buf[] = {[0 ... app_update_bin_len] = 'x'};
	decompressed = decompress_zlib(app_update_bin_lzip, app_update_bin_lzip_len, out_buf);
	zassert_equal(decompressed, app_update_bin_len,
		     "Decompressed: %d bytes expected %d bytes", decompressed,
		     app_update_bin_len);
	for (size_t i = 0; i < app_update_bin_len; i++) {
		zassert_equal(app_update_bin[i], out_buf[i], "app_update_bin[i]: %c != %c out_buf[i]", app_update_bin[i], out_buf[i]);
	}
#endif
}

static void test_compressed_lz_firmware(void)
{
	int decompressed;
	decompressed = decompress_stream_zlib(app_update_bin_lz, app_update_bin_lz_len, app_update_bin);
#ifdef  CONFIG_BOARD_NATIVE_POSIX
	unsigned char out_buf[app_update_bin_len] = {'x'};
	decompressed = decompress_zlib(app_update_bin_lz, app_update_bin_lz_len, out_buf);
	zassert_equal(decompressed, app_update_bin_len,
		     "Decompressed: %d bytes expected %d bytes", decompressed,
		     app_update_bin_len);
	for (size_t i = 0; i < app_update_bin_len; i++) {
		zassert_equal(app_update_bin[i], out_buf[i], "app_update_bin[i]: %c != %c out_buf[i]", app_update_bin[i], out_buf[i]);
	}
#endif
}

static void setup_flash(void)
{
}
#include <stdio.h>

void test_main(void)
{

	flash_dev = device_get_binding(FLASH_NAME);
	fapi = flash_dev->driver_api;
	fapi->page_layout(flash_dev, &layout, &layout_size);
	uzlib_init();
	ztest_test_suite(decompress_test,
		ztest_unit_test(test_compressed_a),
		ztest_unit_test(test_compressed_aaa),
		ztest_unit_test(test_compressed_alphabet),
		ztest_unit_test(test_compressed_alice_stream),
		ztest_unit_test(test_compressed_gz_firmware),
		ztest_unit_test(test_compressed_zlib_firmware),
		ztest_unit_test(test_compressed_lz_firmware)
	);
	ztest_run_test_suite(decompress_test);
}
