/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <logging/log.h>
#include <mgmt/mgmt.h>
#include <nrf_fmfu.h>
#include "mgmt_mfu.h"
#include "cborattr/cborattr.h"
#include <stats/stats.h>
//#include "mgmt_mfu.h"
/* Define modem mgmt spesific SMP return codes */
#define MGMT_ERR_EMODEM_INVALID_COMMAND 200
#define MGMT_ERR_EMODEM_FAULT           201


/* Define the buffer sizes used for smp server */
#define SMP_PACKET_MAXIMUM_TRANSMISSION_UNIT \
	CONFIG_MCUMGR_BUF_SIZE - \
	((CONFIG_UART_MCUMGR_RX_BUF_SIZE/CONFIG_MCUMGR_BUF_SIZE) + 1) * 4
#define SMP_UART_BUFFER_SIZE CONFIG_UART_MCUMGR_RX_BUF_SIZE

int mgmt_mfu_init(void);

LOG_MODULE_REGISTER(mgmt_mfu, CONFIG_MGMT_MFU_LOG_LEVEL);

#define MODEM_MGMT_ID_GET_HASH 0
#define MODEM_MGMT_ID_UPLOAD 1

/* Function return codes */
typedef enum {
    MODEM_RET_SUCCESS                   =  0,   /* SUCCESS */
    MODEM_RET_IPC_FAULT_EVENT           = -1,   /* The modem signaled a fault on the fault IPC channel (IPCEVENT_FAULT_RECEIVE_CHANNEL) */
    MODEM_RET_RPC_UNEXPECTED_RESPONSE   = -2,   /* The modem response code in the RPC buffer (rpc_buffer->response.id) did not match the expected value */
    MODEM_RET_RPC_COMMAND_FAILED        = -3,   /* The modem replied with MODEM_RPC_RESP_CMD_ERROR to an RPC command */
    MODEM_RET_RPC_COMMAND_FAULT         = -4,   /* The modem replied with MODEM_RPC_RESP_UNKNOWN_CMD to an RPC command*/
    MODEM_RET_RPC_TIMEOUT               = -5,   /* Timout while waiting for modem to respond on IPC channel */
    MODEM_RET_INVALID_ARGUMENT          = -6,   /* An invalid argument was passed to the function */
    MODEM_RET_INVALID_OPERATION         = -7    /* The function/operation is not allowed with the current modem state */
} modem_err_t; 

typedef struct {
    uint32_t target_address;
    uint32_t data_len;
    uint8_t * data;
} modem_memory_chunk_t;

uint8_t buf[SMP_PACKET_MAXIMUM_TRANSMISSION_UNIT];
static int modem_mgmt_get_memory_hash(struct mgmt_ctxt * ctxt);
static int modem_mgmt_firmware_upload(struct mgmt_ctxt *ctxt);
/* Struct type used when parsing upload requests. */

typedef struct {
    modem_memory_chunk_t firmware_chunk;
    uint8_t data[SMP_PACKET_MAXIMUM_TRANSMISSION_UNIT];
    unsigned long long offset;
} firmware_packet_t;

static int unpack(struct mgmt_ctxt *ctxt, firmware_packet_t *fw_packet,
		  bool *whole_file_received);

static const struct mgmt_handler mgmt_mfu_handlers[] = {
	[MODEM_MGMT_ID_GET_HASH] = {
		.mh_read  = modem_mgmt_get_memory_hash,
		.mh_write = modem_mgmt_get_memory_hash,
	},
	[MODEM_MGMT_ID_UPLOAD] = {
		.mh_read  = NULL,
		.mh_write = modem_mgmt_firmware_upload
	},
};

static struct mgmt_group modem_mgmt_group = {
	.mg_handlers = (struct mgmt_handler *)mgmt_mfu_handlers,
	.mg_handlers_count = ARRAY_SIZE(mgmt_mfu_handlers),
	.mg_group_id = (MGMT_GROUP_ID_PERUSER + 1),
};

static int get_mgmt_err_from_modem_ret_err(modem_err_t modem_error_return){
	switch(modem_error_return){
		case MODEM_RET_RPC_COMMAND_FAILED:
			LOG_INF("RET_RPC_COMMAND_FAILED");
			return MGMT_ERR_EMODEM_INVALID_COMMAND;
		case MODEM_RET_RPC_COMMAND_FAULT:
			LOG_INF("RET_RPC_COMMAND_FAULT");
			return MGMT_ERR_EMODEM_FAULT;
		default:
			LOG_INF("BAD_STATE");
			return MGMT_ERR_EBADSTATE;
	}
}

int mgmt_mfu_init(void) {
	struct nrf_fmfu_digest digest;
	LOG_INF("Initialized MFU mgmt module");
	mgmt_register_group(&modem_mgmt_group);
	int err = nrf_fmfu_init(&digest, NRF_FMFU_MODEM_BUFFER_SIZE,
		       	(uint8_t *)0x20010000);
	if (err != 0) {
		LOG_ERR("nrf_fmfu_init failed: %d\n", err);
		LOG_ERR("nrf_fmfu_init failed, errno: %d\n.", errno);
		return err;
	}
	LOG_INF("Initialized MFU mgmt module done");
	return err; 
}

static int encode_response(struct mgmt_ctxt *ctx, uint32_t expected_offset) {
	CborError err = 0;
	int status = 0;

	/* TODO should we return this after the whole file has been received?*/
	err |= cbor_encode_text_stringz(&ctx->encoder, "rc");
	err |= cbor_encode_int(&ctx->encoder, status);
	err |= cbor_encode_text_stringz(&ctx->encoder, "off");
	err |= cbor_encode_uint(&ctx->encoder, expected_offset);

	if (err != 0) {
		return MGMT_ERR_ENOMEM;
	}

	return 0;
}
static bool bootloader = true;
static bool first_chunk = true;
static int modem_mgmt_firmware_upload(struct mgmt_ctxt *ctx)
{
	
	int ret;
	bool whole_file_received;
	static firmware_packet_t firmware_packet;
	uint32_t next_expected_offset;
	CborError err;
	
	int modem_state = nrf_fmfu_modem_state_get();
	LOG_INF("Modem state: %d", modem_state);

	ret = unpack(ctx, &firmware_packet, 
		     &whole_file_received);
	if (ret < 0){
		return ret;
	} else {
		next_expected_offset = ret;
		LOG_INF("New offset: 0x%x", next_expected_offset);
	}

	if (firmware_packet.firmware_chunk.data_len > 0) {
		if (first_chunk) {
			err = nrf_fmfu_transfer_start();
			if (err != 0){
				return get_mgmt_err_from_modem_ret_err(err);
			}
			first_chunk = false;
		}

		LOG_INF("target_addr: 0x%x", firmware_packet.firmware_chunk.target_address);
		LOG_INF("Writing firmware packet with len:%d",firmware_packet.firmware_chunk.data_len);
		err = nrf_fmfu_memory_chunk_write(firmware_packet.firmware_chunk.target_address,
						  firmware_packet.firmware_chunk.data_len,
						  &firmware_packet.firmware_chunk.data);
		if (err != 0){
			LOG_INF("Error in writing data");
			return get_mgmt_err_from_modem_ret_err(err);
		}
	}

	if (whole_file_received) {
		LOG_INF("whole file recived");
		err = nrf_fmfu_transfer_end();
		if (err != 0) {
			LOG_INF("error in transfer_end");
			return get_mgmt_err_from_modem_ret_err(err);
		}
		first_chunk = true;
	}

	return encode_response(ctx, next_expected_offset);
}

static int modem_mgmt_get_memory_hash(struct mgmt_ctxt * ctxt){
	LOG_INF("get hash");
	unsigned long long start;
	unsigned long long end;
	struct nrf_fmfu_digest digest;

	/* We expect two variables: the start and end address of a memory region
	 * that we want to verify (obtain a hash for) */
	const struct cbor_attr_t off_attr[] = {
		[0] = {
			.attribute = "start",
			.type = CborAttrUnsignedIntegerType,
			.addr.uinteger = &start,
			.nodefault = true
		},
		[1] = {
			.attribute = "end",
			.type = CborAttrUnsignedIntegerType,
			.addr.uinteger = &end,
			.nodefault = true
		},
		[2] = { 0 },
	};
	start = 0;
	end = 0;

	/* Parse JSON object */
	int rc = cbor_read_object(&ctxt->it, off_attr);
	if (rc || start == 0 || end == 0) {
		return MGMT_ERR_EINVAL;
	}

	modem_err_t err = nrf_fmfu_memory_hash_get(start, end, &digest);
	if (err != MODEM_RET_SUCCESS){
		return get_mgmt_err_from_modem_ret_err(err);
	}

	/* Put the digest response in the response */
	CborError cbor_err = 0;
	cbor_err |= cbor_encode_text_stringz(&ctxt->encoder, "digest");
	cbor_err |= cbor_encode_byte_string(&ctxt->encoder,
			(uint8_t *)digest.data, NRF_FMFU_DIGEST_BUFFER_LEN);
	cbor_err |= cbor_encode_text_stringz(&ctxt->encoder, "rc");
	cbor_err |= cbor_encode_int(&ctxt->encoder, err);
	if (cbor_err != 0) {
		return MGMT_ERR_ENOMEM;
	}

	return MGMT_ERR_EOK;
}

#define FIRMWARE_SHA_LENGTH 32
static int unpack(struct mgmt_ctxt *ctxt, firmware_packet_t *fw_packet,
		  bool *whole_file_received){
	int err;
	unsigned long long fw_target_address = 0;
	unsigned long long offset;
	unsigned long long file_length;
	size_t firmware_sha_len;
	uint8_t firmware_sha[FIRMWARE_SHA_LENGTH];
    
	/* Description/skeleton of CBOR object */
	const struct cbor_attr_t off_attr[] = {
		[0] = {
			.attribute = "data",
			.type = CborAttrByteStringType,
			//TODO: Fix this so that it copes data in to buffer
			.addr.bytestring.data = fw_packet->data,
			.addr.bytestring.len = &fw_packet->firmware_chunk.data_len,
			.len = SMP_PACKET_MAXIMUM_TRANSMISSION_UNIT
		},
		[1] = {
			.attribute = "len",
			.type = CborAttrUnsignedIntegerType,
			.addr.uinteger = &file_length,
			.nodefault = true
		},
		[2] = {
			.attribute = "off",
			.type = CborAttrUnsignedIntegerType,
			.addr.uinteger = &offset,
			.nodefault = true
		},
		[3] = {
			.attribute = "sha",
			.type = CborAttrByteStringType,
			.addr.bytestring.data = firmware_sha,
			.addr.bytestring.len = &firmware_sha_len,
			.len = FIRMWARE_SHA_LENGTH
		},
		[4] = {
			.attribute = "addr",
			.type = CborAttrUnsignedIntegerType,
			.addr.uinteger = &fw_target_address,
			.nodefault = true
		},
		[6] = { 0 },
	};


	err = cbor_read_object(&ctxt->it, off_attr);
	if (err) {
		return -MGMT_ERR_EINVAL;
	}
	
	fw_packet->firmware_chunk.target_address  = (uint32_t)fw_target_address;
	fw_packet->firmware_chunk.data = fw_packet->data;
	
	if ((offset + fw_packet->firmware_chunk.data_len) == file_length) {
		*whole_file_received = true;
	} else {
		*whole_file_received = false;
	}

	return offset + fw_packet->firmware_chunk.data_len;
}
