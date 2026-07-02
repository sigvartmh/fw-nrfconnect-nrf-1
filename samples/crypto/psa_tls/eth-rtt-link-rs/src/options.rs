//! Command line options, ported from `experimental-eth-rtt-link/options.c`.
//!
//! Compared to the original tool, `--family`, `--jlinklib` and `--nrfjproglib` are gone: there
//! is no vendor DLL to select a device family for or to dynamically load anymore, since J-Link
//! communication goes through `probe-rs` (backed by the pure-Rust `jaylink` USB driver) instead
//! of `libnrfjprogdll.so`/`libjlinkarm.so`. In their place, `--rttcbaddr` and the new `--elf`
//! option are how the RTT control block is located (see [`crate::rtt`] for the fallback used
//! when neither is given).

use std::net::{Ipv4Addr, Ipv6Addr};
use std::path::PathBuf;

use clap::Parser;

use crate::logs::options_fatal;

#[derive(Parser)]
#[command(version, about = "Ethernet over RTT link, using a J-Link probe directly (no nrfjprog).")]
struct RawArgs {
    /// Verbosity level: 0 disables all messages, 1 errors only (default), 2 also info, 3 also
    /// debug, 4 also probe-rs's own internal trace log.
    #[arg(short = 'v', long, num_args = 0..=1, default_missing_value = "2", value_parser = clap::value_parser!(u8).range(crate::logs::LOG_NONE as i64..=crate::logs::LOG_PROBE_RS as i64))]
    verbose: Option<u8>,

    /// IPv4 address. May include netmask bits count at the end, e.g. 192.168.1.1/24
    #[arg(long = "ipv4")]
    ipv4: Option<String>,

    /// IPv4 network mask.
    #[arg(long = "ipv4mask")]
    ipv4mask: Option<String>,

    /// IPv4 broadcast address.
    #[arg(long = "ipv4brdcst")]
    ipv4brdcst: Option<String>,

    /// IPv6 address. May include netmask bits count at the end, e.g. 2001:db8::/32
    #[arg(long = "ipv6")]
    ipv6: Option<String>,

    /// MAC address. By default the operating system will assign a random address.
    #[arg(short = 'm', long = "mac")]
    mac: Option<String>,

    /// Maximum transmission unit. Default 1500.
    #[arg(long = "mtu", value_parser = clap::value_parser!(u32).range(68..=65535))]
    mtu: Option<u32>,

    /// Sets network interface name. Default is tap0 where 0 can be replaced by a higher number
    /// if needed.
    #[arg(short = 'i', long = "iface")]
    iface: Option<String>,

    /// Selects the debugger with the given serial number among all those connected to the PC.
    #[arg(short = 's', long = "snr")]
    snr: Option<u32>,

    /// Sets the debugger SWD clock speed in kHz.
    #[arg(short = 'c', long = "clockspeed", default_value = "2000", value_parser = clap::value_parser!(u32).range(4..=50000))]
    clockspeed: u32,

    /// Sets the address where the RTT control block is located, skipping the scan. Accepts
    /// decimal or 0x-prefixed hexadecimal.
    #[arg(short = 'a', long = "rttcbaddr", value_parser = parse_int)]
    rttcbaddr: Option<u64>,

    /// Path to the firmware ELF file. If given (and --rttcbaddr is not), the `_SEGGER_RTT`
    /// symbol is read from it to locate the control block instead of scanning memory.
    #[arg(long = "elf")]
    elf: Option<PathBuf>,

    /// Interval time in milliseconds for the RTT polling loop.
    #[arg(long = "polltime", default_value = "5", value_parser = clap::value_parser!(u32).range(1..=999))]
    polltime: u32,

    /// Do not try to recover from RTT errors by restarting the entire process.
    #[arg(long = "norttretry")]
    norttretry: bool,

    /// File that will be polled. If it exists the link will be temporarily stopped until the
    /// file is deleted.
    #[arg(long = "hangfile")]
    hangfile: Option<String>,
}

fn parse_int(s: &str) -> Result<u64, String> {
    if let Some(hex) = s.strip_prefix("0x").or_else(|| s.strip_prefix("0X")) {
        u64::from_str_radix(hex, 16).map_err(|e| e.to_string())
    } else {
        s.parse::<u64>().map_err(|e| e.to_string())
    }
}

fn parse_subnet_bits(arg: &str, max_bits: u32) -> (String, Option<u32>) {
    match arg.split_once('/') {
        Some((addr, bits)) => {
            let bits: u32 = bits
                .parse()
                .unwrap_or_else(|_| options_fatal(&format!("Invalid subnet bits '{bits}'")));
            if bits > max_bits {
                options_fatal(&format!(
                    "Integer argument '{bits}' out of range [0-{max_bits}]"
                ));
            }
            (addr.to_string(), Some(bits))
        }
        None => (arg.to_string(), None),
    }
}

fn parse_mac(arg: &str) -> [u8; 6] {
    let hex: String = arg.chars().filter(|c| *c != ':' && *c != '-').collect();
    if hex.len() != 12 {
        options_fatal("Invalid MAC address");
    }
    let mut mac = [0u8; 6];
    for i in 0..6 {
        mac[i] = u8::from_str_radix(&hex[i * 2..i * 2 + 2], 16)
            .unwrap_or_else(|_| options_fatal("Invalid MAC address"));
    }
    mac
}

/// Parsed and validated command line options.
pub struct Options {
    pub verbose: u8,

    pub ipv4_address: Option<Ipv4Addr>,
    pub ipv4_netmask: Option<Ipv4Addr>,
    pub ipv4_broadcast: Option<Ipv4Addr>,

    pub ipv6_address: Option<Ipv6Addr>,
    pub ipv6_subnet_bits: Option<u32>,

    pub mac_addr: Option<[u8; 6]>,

    pub mtu: Option<u32>,
    pub iface: Option<String>,

    pub snr: Option<u32>,
    pub speed_khz: u32,
    pub rtt_cb_address: Option<u64>,
    pub elf_path: Option<PathBuf>,

    pub poll_time_us: u64,
    pub no_rtt_retry: bool,

    pub hang_file: Option<String>,
}

impl Options {
    pub fn parse() -> Options {
        let raw = RawArgs::parse();

        let mut ipv4_address = None;
        let mut ipv4_netmask = None;
        if let Some(arg) = &raw.ipv4 {
            let (addr, bits) = parse_subnet_bits(arg, 32);
            if let Some(bits) = bits {
                let subnet: u32 = (0xFFFFFFFFu64 << (32 - bits)) as u32;
                ipv4_netmask = Some(Ipv4Addr::from(subnet));
            }
            ipv4_address =
                Some(addr.parse().unwrap_or_else(|_| options_fatal("Invalid IPv4 address")));
        }

        if let Some(arg) = &raw.ipv4mask {
            ipv4_netmask =
                Some(arg.parse().unwrap_or_else(|_| options_fatal("Invalid IPv4 netmask")));
        }

        let ipv4_broadcast = raw.ipv4brdcst.as_ref().map(|arg| {
            arg.parse()
                .unwrap_or_else(|_| options_fatal("Invalid IPv4 broadcast address"))
        });

        let mut ipv6_address = None;
        let mut ipv6_subnet_bits = None;
        if let Some(arg) = &raw.ipv6 {
            let (addr, bits) = parse_subnet_bits(arg, 128);
            ipv6_subnet_bits = bits;
            ipv6_address =
                Some(addr.parse().unwrap_or_else(|_| options_fatal("Invalid IPv6 address")));
        }

        let mac_addr = raw.mac.as_deref().map(parse_mac);

        Options {
            verbose: raw.verbose.unwrap_or(1),
            ipv4_address,
            ipv4_netmask,
            ipv4_broadcast,
            ipv6_address,
            ipv6_subnet_bits,
            mac_addr,
            mtu: raw.mtu,
            iface: raw.iface,
            snr: raw.snr,
            speed_khz: raw.clockspeed,
            rtt_cb_address: raw.rttcbaddr,
            elf_path: raw.elf,
            poll_time_us: raw.polltime as u64 * 1000,
            no_rtt_retry: raw.norttretry,
            hang_file: raw.hangfile,
        }
    }
}
