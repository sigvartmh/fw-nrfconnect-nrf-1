//! SLIP encoder/decoder, ported from `experimental-eth-rtt-link/slip.c`.

const SLIP_END: u8 = 0o300;
const SLIP_ESC: u8 = 0o333;
const SLIP_ESC_END: u8 = 0o334;
const SLIP_ESC_ESC: u8 = 0o335;

/// Size of the decoder's internal buffer. Matches `SLIP_DECODER_BUFFER_SIZE` in the original.
const DECODER_BUFFER_SIZE: usize = 256 * 1024;

/// SLIP-encodes `input`, appending the result to `output`, surrounded by `SLIP_END` markers.
pub fn slip_encode(input: &[u8], output: &mut Vec<u8>) {
    output.push(SLIP_END);
    for &byte in input {
        match byte {
            SLIP_END => {
                output.push(SLIP_ESC);
                output.push(SLIP_ESC_END);
            }
            SLIP_ESC => {
                output.push(SLIP_ESC);
                output.push(SLIP_ESC_ESC);
            }
            _ => output.push(byte),
        }
    }
    output.push(SLIP_END);
}

/// Incremental SLIP decoder. Bytes are fed in with [`SlipDecoder::put`], and completed,
/// unstuffed packets are retrieved with [`SlipDecoder::read`].
pub struct SlipDecoder {
    buffer: Box<[u8; DECODER_BUFFER_SIZE]>,
    write_index: usize,
    read_index: usize,
}

impl SlipDecoder {
    pub fn new() -> Self {
        SlipDecoder {
            buffer: Box::new([0u8; DECODER_BUFFER_SIZE]),
            write_index: 0,
            read_index: 0,
        }
    }

    /// Appends raw (still SLIP-encoded) bytes to the decoder. Returns the number of complete
    /// packets (i.e. `SLIP_END` bytes) now available. If the internal buffer is full, excess
    /// input bytes are dropped and a warning is logged.
    pub fn put(&mut self, input: &[u8]) -> usize {
        if self.read_index > 0 && self.read_index < self.write_index {
            self.buffer
                .copy_within(self.read_index..self.write_index, 0);
        }
        self.write_index -= self.read_index;
        self.read_index = 0;

        let mut result = 0;
        let mut consumed = 0;
        for &byte in input {
            if self.write_index >= self.buffer.len() {
                break;
            }
            self.buffer[self.write_index] = byte;
            self.write_index += 1;
            consumed += 1;
            if byte == SLIP_END {
                result += 1;
            }
        }

        if consumed < input.len() {
            crate::logs::print_error(&format!(
                "SLIP decoder buffer overflow. Lost {} bytes.",
                input.len() - consumed
            ));
        }

        result
    }

    /// Reads and unstuffs the next complete packet, if any. Returns `None` if no full packet
    /// (terminated by `SLIP_END`) is currently buffered.
    pub fn read(&mut self) -> Option<Vec<u8>> {
        let start = self.read_index;
        let end_of_input = self.write_index;

        let mut end_of_packet = None;
        for i in start..end_of_input {
            if self.buffer[i] == SLIP_END {
                end_of_packet = Some(i);
                break;
            }
        }
        let end_of_packet = end_of_packet?;

        self.read_index = end_of_packet + 1;

        let mut output = Vec::with_capacity(end_of_packet - start);
        let mut i = start;
        while i < end_of_packet {
            let mut byte = self.buffer[i];
            i += 1;
            if byte == SLIP_ESC {
                if i >= end_of_packet {
                    crate::logs::print_info(
                        "Invalid byte stuffing indicator at the end of packet",
                    );
                    break;
                }
                let escaped = self.buffer[i];
                if escaped == SLIP_ESC_END {
                    byte = SLIP_END;
                } else if escaped == SLIP_ESC_ESC {
                    byte = SLIP_ESC;
                } else {
                    crate::logs::print_info(&format!(
                        "Invalid byte stuffing indicator \"\\x{escaped:02X}\""
                    ));
                }
                i += 1;
            }
            output.push(byte);
        }

        Some(output)
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn encode_stuffs_special_bytes() {
        let input = b"\xC0xyz\xDBuvw\xDC\xDD\xC0--\xC0";
        let expected = b"\xC0\xDB\xDCxyz\xDB\xDDuvw\xDC\xDD\xDB\xDC--\xDB\xDC\xC0";
        let mut output = Vec::new();
        slip_encode(input, &mut output);
        assert_eq!(output, expected);
    }

    /// A "" expectation means no packet is available yet, which `read()` reports as `None`
    /// (this matches the original C decoder, which returns a 0-length result in both the
    /// no-packet and genuinely-empty-packet cases).
    fn expect_read(ctx: &mut SlipDecoder, expected: &[u8]) {
        if expected.is_empty() {
            assert!(ctx.read().is_none());
        } else {
            assert_eq!(ctx.read().unwrap(), expected);
        }
    }

    #[test]
    fn decode_handles_split_and_escaped_frames() {
        let mut ctx = SlipDecoder::new();

        assert_eq!(
            ctx.put(b"\xDB\xDCxyz\xDB\xDDuvw\xDC\xDD\xDB\xDC--\xDB\xDC\xC0"),
            1
        );
        expect_read(&mut ctx, b"\xC0xyz\xDBuvw\xDC\xDD\xC0--\xC0");
        expect_read(&mut ctx, b"");

        assert_eq!(ctx.put(b"abc"), 0);
        expect_read(&mut ctx, b"");

        assert_eq!(ctx.put(b"abc\xDB\xDC"), 0);
        expect_read(&mut ctx, b"");

        assert_eq!(ctx.put(b"\xC0xyz"), 1);
        expect_read(&mut ctx, b"abcabc\xC0");
        expect_read(&mut ctx, b"");

        assert_eq!(ctx.put(b"\xC0one"), 1);
        expect_read(&mut ctx, b"xyz");
        expect_read(&mut ctx, b"");

        assert_eq!(ctx.put(b"\xC0two\xC0three\xC0four\xC0"), 4);
        expect_read(&mut ctx, b"one");
        expect_read(&mut ctx, b"two");
        expect_read(&mut ctx, b"three");
        expect_read(&mut ctx, b"four");
        expect_read(&mut ctx, b"");
        expect_read(&mut ctx, b"");

        assert_eq!(ctx.put(b"x\xC0y\xC0"), 2);

        assert_eq!(ctx.put(b"z\xC0"), 1);
        expect_read(&mut ctx, b"x");
        expect_read(&mut ctx, b"y");
        expect_read(&mut ctx, b"z");
        expect_read(&mut ctx, b"");
    }
}
