/// CRC-16/CCITT with initial seed 0xFFFF and no final xoring, matching the
/// algorithm implemented by the `eth_rtt` Zephyr driver (`drivers/net/eth_rtt.c`).
pub fn crc16_ccitt(seed: u16, data: &[u8]) -> u16 {
    let mut seed = seed;
    for &byte in data {
        let e = (seed as u8) ^ byte;
        let f = e ^ (e << 4);
        seed = (seed >> 8) ^ ((f as u16) << 8) ^ ((f as u16) << 3) ^ ((f as u16) >> 4);
    }
    seed
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn matches_reference_value() {
        // Not a named standard CRC (the nibble-swap trick used here is SEGGER's own, not
        // e.g. CRC-16/CCITT-FALSE) - this value was cross-checked against a direct
        // reimplementation of the same algorithm as used in `eth_rtt.c`/the original C tool.
        assert_eq!(crc16_ccitt(0xFFFF, b"123456789"), 0x6F91);
    }
}
