/* Generated with cddl_gen.py (https://github.com/oyvindronningstad/cddl_gen)
 */

#ifndef MFU_DECODE_H__
#define MFU_DECODE_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "cbor_decode.h"


typedef struct {
	uint32_t _Segment_target_addr;
	uint32_t _Segment_len;
} Segment_t;

typedef struct {
	Segment_t _Segments__Segment[5];
	size_t _Segments__Segment_count;
} Segments_t;


bool cbor_decode_Segments(
		const uint8_t * p_payload, size_t payload_len,
		Segments_t * p_result,
		bool complete, size_t *p_payload_len_out);


#endif // MFU_DECODE_H__
