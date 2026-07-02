# eth-rtt-link-rs

Rust rewrite of `../experimental-eth-rtt-link`, the "Ethernet over RTT" host tool for
`drivers/net/eth_rtt.c`. Same wire protocol (SLIP framing, CRC-16/CCITT, magic reset frame),
same `ETH_RTT` RTT channel discovery, same CLI shape - but it talks to the J-Link probe
directly over USB via [`probe-rs`](https://probe.rs) (which itself uses the pure-Rust
`jaylink` driver), instead of dynamically loading Nordic's `libnrfjprogdll.so` /
SEGGER's `libjlinkarm.so`. No vendor DLLs required at all.

## Build

```
cargo build --release
```

TAP device creation needs `CAP_NET_ADMIN`:

```
sudo setcap cap_net_admin=eip ./target/release/eth-rtt-link-rs
```

## Locating the RTT control block

`nrfjprog` used to find this by scanning RAM known to belong to the connected chip's family.
Since this tool has no chip database (see below), give it one of:

- `--rttcbaddr <addr>`: exact address (same option as the original tool).
- `--elf <path>`: reads the `_SEGGER_RTT` symbol out of the firmware ELF.
- neither: falls back to scanning the nRF54H20 application core's default RTT window
  (`0x22000000`, the `APP_RAM0`/TCM region every nRF54H20 cpuapp build uses by default - see
  `zephyr/soc/nordic/nrf54h/Kconfig.defconfig.nrf54h20_cpuapp`). Only useful for that target;
  pass one of the above for anything else.

## Options dropped from the original

`--family`, `--jlinklib` and `--nrfjproglib` are gone - there's no vendor DLL to select a
device family for or to dynamically load anymore.

## Why a custom probe-rs target

probe-rs has no built-in nRF54H20 target yet. Its generic, architecture-only targets (e.g.
`cortex-m33`) attach fine, but declare no RAM/flash regions at all, and probe-rs validates
every RTT channel's buffer pointers against the target's declared memory map before letting
you read/write. So this tool registers a minimal custom target at runtime (see
`build_target_registry` in `src/rtt.rs`) covering just the regions the `eth_rtt` driver
actually touches on the nRF54H20 application core, with no flash algorithm (this tool only
ever attaches non-invasively, never flashes).
