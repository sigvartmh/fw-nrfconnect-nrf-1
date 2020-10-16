/* Generated with cddl_gen.py (https://github.com/oyvindronningstad/cddl_gen)
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "cbor_decode.h"
#include "mfu_decode.h"


static bool decode_Segment(
		cbor_state_t *p_state, Segment_t * p_result)
{
	cbor_print("decode_Segment\n");

	bool result = (((((uintx32_decode(p_state, (&(*p_result)._Segment_target_addr))))
	&& ((uintx32_decode(p_state, (&(*p_result)._Segment_len)))))));

	if (!result)
	{
		cbor_trace();
	}

	return result;
}

static bool decode_Segments(
		cbor_state_t *p_state, Segments_t * p_result)
{
	cbor_print("decode_Segments\n");
	bool int_res;

	bool result = (((list_start_decode(p_state) && (int_res = (multi_decode(3, 5, &(*p_result)._Segments__Segment_count, (void*)decode_Segment, p_state, (&(*p_result)._Segments__Segment), sizeof(Segment_t))), ((list_end_decode(p_state)) && int_res)))));

	if (!result)
	{
		cbor_trace();
	}

	return result;
}



bool cbor_decode_Segments(
		const uint8_t * p_payload, size_t payload_len,
		Segments_t * p_result,
		bool complete, size_t *p_payload_len_out)
{
	cbor_state_t state = {
		.p_payload = p_payload,
		.p_payload_end = p_payload + payload_len,
		.elem_count = 1
	};

	cbor_state_t state_backups[2];

	cbor_state_backups_t backups = {
		.p_backup_list = state_backups,
		.current_backup = 0,
		.num_backups = 2,
	};

	state.p_backups = &backups;

	bool result = decode_Segments(&state, p_result);

	if (p_payload_len_out != NULL) {
		*p_payload_len_out = ((size_t)state.p_payload - (size_t)p_payload);
	}

	if (!complete) {
		return result;
	}
	if (result) {
		if (state.p_payload == state.p_payload_end) {
			return true;
		} else {
			cbor_state_t *p_state = &state; // for printing.
			cbor_print("p_payload_end: 0x%x\r\n", (uint32_t)state.p_payload_end);
			cbor_trace();
		}
	}

	return result;
}
