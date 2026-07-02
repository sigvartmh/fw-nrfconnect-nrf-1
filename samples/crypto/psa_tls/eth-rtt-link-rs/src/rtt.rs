//! J-Link + RTT transport, replacing `experimental-eth-rtt-link/rtt.c`'s use of
//! `libnrfjprogdll.so`/`libjlinkarm.so` with `probe-rs`, which talks to J-Link probes directly
//! over USB via the pure-Rust `jaylink` driver (no vendor DLL needed at all).

use std::collections::VecDeque;
use std::ops::Range;
use std::path::Path;
use std::time::Duration;

use probe_rs::config::{
    Chip, ChipFamily, CoreType, MemoryRegion, NvmRegion, RamRegion, Registry,
    TargetDescriptionSource,
};
use probe_rs::probe::list::Lister;
use probe_rs::rtt::{Rtt, ScanRegion};
use probe_rs::Permissions;

use crate::logs::{print_debug, print_error, print_info, recoverable_fatal};
use crate::options::Options;

/// RTT channel name used by the `eth_rtt` Zephyr driver to identify the Ethernet transfer
/// channel (`drivers/net/eth_rtt.c`).
const CHANNEL_NAME: &str = "ETH_RTT";

const MAX_WRITE_QUEUE_SIZE: usize = 2 * 1024 * 1024;

/// Name of the custom target registered with probe-rs (see [`build_target_registry`]), since
/// there is no nRF54H20 target built into probe-rs to attach to.
const TARGET_NAME: &str = "nRF54H20-cpuapp-rtt";

/// Default RTT scan window, used when neither `--rttcbaddr` nor `--elf` is given: the nRF54H20
/// application core's globally-addressable "APP_RAM0" (local TCM) window. Every nRF54H20
/// cpuapp Zephyr build places its RTT control block here by default (see
/// `zephyr/soc/nordic/nrf54h/Kconfig.defconfig.nrf54h20_cpuapp`, which selects
/// `SEGGER_RTT_SECTION_CUSTOM_DTS_REGION`), specifically so it stays reachable from a debug
/// probe regardless of where the rest of RAM ends up mapped.
const NRF54H20_CPUAPP_RTT_WINDOW: Range<u64> = 0x2200_0000..0x2200_8000;

/// The application core's main RAM region (`cpuapp_data`, from
/// `zephyr/boards/nordic/nrf54h20dk/nrf54h20dk_nrf54h20-memory_map.dtsi`). The RTT channel
/// buffers themselves (as opposed to the control block, which lives in
/// [`NRF54H20_CPUAPP_RTT_WINDOW`]) are ordinary statics and end up here.
const NRF54H20_CPUAPP_MAIN_RAM: Range<u64> = 0x2f00_0000..0x2f0b_e000;

/// The application core's MRAM (flash) window. RTT channel *names* are string literals living
/// in `.rodata`, i.e. here rather than in RAM, so this needs to be a known region too or
/// `probe-rs` refuses to read them. Wide enough to comfortably cover this sample's own image;
/// override with `--rttcbaddr`/`--elf` if a much larger application needs more.
const NRF54H20_CPUAPP_MRAM: Range<u64> = 0x0e00_0000..0x0e20_0000;

/// probe-rs has no built-in nRF54H20 target, and its generic architecture-only targets (e.g.
/// `cortex-m33`) declare no RAM regions at all - which is fine for a plain `attach`, but RTT
/// channel reads/writes fail validation ("buffer doesn't fully fit in any known ram region")
/// without at least one declared RAM region. So we register a minimal custom target covering
/// just the two RAM windows the `eth_rtt` driver actually uses, with no flash algorithm (we
/// never flash, only attach non-invasively).
fn build_target_registry() -> Registry {
    let mut chip = Chip::generic_arm(TARGET_NAME, CoreType::Armv8m);
    chip.memory_map = vec![
        MemoryRegion::Ram(RamRegion {
            name: Some("APP_RAM0".to_string()),
            range: NRF54H20_CPUAPP_RTT_WINDOW,
            cores: vec!["main".to_string()],
            access: None,
        }),
        MemoryRegion::Ram(RamRegion {
            name: Some("cpuapp_data".to_string()),
            range: NRF54H20_CPUAPP_MAIN_RAM,
            cores: vec!["main".to_string()],
            access: None,
        }),
        MemoryRegion::Nvm(NvmRegion {
            name: Some("cpuapp_mram".to_string()),
            range: NRF54H20_CPUAPP_MRAM,
            cores: vec!["main".to_string()],
            is_alias: false,
            access: None,
        }),
    ];

    let family = ChipFamily {
        name: TARGET_NAME.to_string(),
        manufacturer: None,
        chip_detection: vec![],
        generated_from_pack: false,
        pack_file_release: None,
        variants: vec![chip],
        flash_algorithms: vec![],
        source: TargetDescriptionSource::External,
    };

    let mut registry = Registry::from_builtin_families();
    registry
        .add_target_family(family)
        .expect("hand-built chip family should be valid");
    registry
}

pub struct JlinkRtt {
    session: probe_rs::Session,
    rtt: Rtt,
    up_channel: usize,
    down_channel: usize,
    write_queue: VecDeque<u8>,
}

impl JlinkRtt {
    pub fn connect(options: &Options) -> JlinkRtt {
        let lister = Lister::new();
        let mut probes = lister.list_all();
        if let Some(snr) = options.snr {
            let snr = snr.to_string();
            probes.retain(|p| p.serial_number.as_deref() == Some(snr.as_str()));
        }

        let probe_info = match probes.len() {
            0 => recoverable_fatal("No J-Link probe found."),
            1 => probes.remove(0),
            _ => recoverable_fatal(&format!(
                "Multiple debug probes found ({}). Use --snr to select one.",
                probes
                    .iter()
                    .filter_map(|p| p.serial_number.as_deref())
                    .collect::<Vec<_>>()
                    .join(", ")
            )),
        };

        print_info(&format!(
            "Using probe: {} (S/N {})",
            probe_info.identifier,
            probe_info.serial_number.as_deref().unwrap_or("?")
        ));

        let mut probe = probe_info
            .open()
            .unwrap_or_else(|e| recoverable_fatal(&format!("Cannot open J-Link probe: {e}")));

        if let Err(e) = probe.set_speed(options.speed_khz) {
            print_error(&format!("Cannot set clock speed: {e}"));
        }

        let registry = build_target_registry();
        let mut session = probe
            .attach_with_registry(TARGET_NAME, Permissions::default(), &registry)
            .unwrap_or_else(|e| recoverable_fatal(&format!("Cannot connect to device: {e}")));

        let scan_region = Self::resolve_scan_region(options);

        let rtt = {
            let mut core = session
                .core(0)
                .unwrap_or_else(|e| recoverable_fatal(&format!("Cannot access core: {e}")));
            probe_rs::rtt::try_attach_to_rtt(&mut core, Duration::from_secs(3), &scan_region)
                .unwrap_or_else(|e| recoverable_fatal(&format!("Cannot find RTT control block: {e}")))
        };

        for (i, ch) in rtt.up_channels.iter().enumerate() {
            print_info(&format!(
                "RTT[{i}] UP:   \"{}\" ({} bytes)",
                ch.name().unwrap_or(""),
                ch.buffer_size()
            ));
        }
        for (i, ch) in rtt.down_channels.iter().enumerate() {
            print_info(&format!(
                "RTT[{i}] DOWN: \"{}\" ({} bytes)",
                ch.name().unwrap_or(""),
                ch.buffer_size()
            ));
        }

        let up_channel = rtt.up_channels.iter().position(|c| c.name() == Some(CHANNEL_NAME));
        let down_channel = rtt
            .down_channels
            .iter()
            .position(|c| c.name() == Some(CHANNEL_NAME));

        let (up_channel, down_channel) = match (up_channel, down_channel) {
            (Some(u), Some(d)) => (u, d),
            _ => recoverable_fatal("Cannot find ethernet RTT channel."),
        };

        JlinkRtt {
            session,
            rtt,
            up_channel,
            down_channel,
            write_queue: VecDeque::new(),
        }
    }

    fn resolve_scan_region(options: &Options) -> ScanRegion {
        if let Some(addr) = options.rtt_cb_address {
            print_info(&format!("Scanning at exact address: {addr:#010x}"));
            return ScanRegion::Exact(addr);
        }
        if let Some(path) = &options.elf_path {
            match elf_rtt_symbol(path) {
                Ok(addr) => {
                    print_info(&format!(
                        "Found _SEGGER_RTT symbol at {addr:#010x} in {}",
                        path.display()
                    ));
                    return ScanRegion::Exact(addr);
                }
                Err(e) => print_error(&format!(
                    "Cannot read _SEGGER_RTT symbol from {}: {e}",
                    path.display()
                )),
            }
        }
        ScanRegion::range(NRF54H20_CPUAPP_RTT_WINDOW)
    }

    fn core(session: &mut probe_rs::Session) -> probe_rs::Core<'_> {
        session
            .core(0)
            .unwrap_or_else(|e| recoverable_fatal(&format!("Cannot access core: {e}")))
    }

    /// Reads whatever is currently available from the up channel. Never blocks.
    pub fn read(&mut self, buf: &mut [u8]) -> usize {
        let mut core = Self::core(&mut self.session);
        self.rtt
            .up_channel(self.up_channel)
            .expect("up channel")
            .read(&mut core, buf)
            .unwrap_or_else(|e| recoverable_fatal(&format!("RTT READ ERROR: {e}")))
    }

    /// Tries to drain the pending write queue into the down channel. Never blocks.
    pub fn flush_queue(&mut self) {
        loop {
            if self.write_queue.is_empty() {
                return;
            }

            let len_before = self.write_queue.len();
            let written = {
                let mut core = Self::core(&mut self.session);
                let slice = self.write_queue.make_contiguous();
                self.rtt
                    .down_channel(self.down_channel)
                    .expect("down channel")
                    .write(&mut core, slice)
                    .unwrap_or_else(|e| recoverable_fatal(&format!("RTT WRITE ERROR: {e}")))
            };

            if written == 0 {
                return;
            }
            self.write_queue.drain(..written);
            if written < len_before {
                return;
            }
        }
    }

    /// Queues `data` for transmission on the down channel, first trying to send as much of it
    /// (and of anything already queued) immediately. Never blocks.
    pub fn buffered_write(&mut self, mut data: &[u8]) {
        self.flush_queue();

        if self.write_queue.is_empty() {
            let written = {
                let mut core = Self::core(&mut self.session);
                self.rtt
                    .down_channel(self.down_channel)
                    .expect("down channel")
                    .write(&mut core, data)
                    .unwrap_or_else(|e| recoverable_fatal(&format!("RTT WRITE ERROR: {e}")))
            };
            if written >= data.len() {
                return;
            }
            data = &data[written..];
        }

        if self.write_queue.len() + data.len() > MAX_WRITE_QUEUE_SIZE {
            let overflow = self.write_queue.len() + data.len() - MAX_WRITE_QUEUE_SIZE;
            let drop_count = overflow.min(self.write_queue.len());
            if drop_count > 0 {
                print_error(&format!("Write queue overflow. Removing {drop_count} bytes."));
                self.write_queue.drain(..drop_count);
            }
        }

        self.write_queue.extend(data);
        print_debug(&format!(
            "Written to queue {}, total {}",
            data.len(),
            self.write_queue.len()
        ));
    }
}

fn elf_rtt_symbol(path: &Path) -> Result<u64, String> {
    use object::{Object, ObjectSymbol};

    let data = std::fs::read(path).map_err(|e| e.to_string())?;
    let file = object::File::parse(&*data).map_err(|e| e.to_string())?;
    file.symbols()
        .find(|s| s.name() == Ok("_SEGGER_RTT"))
        .map(|s| s.address())
        .ok_or_else(|| "symbol not found".to_string())
}
