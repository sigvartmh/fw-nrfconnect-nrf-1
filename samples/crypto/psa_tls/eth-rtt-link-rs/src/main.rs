//! Rust rewrite of `experimental-eth-rtt-link`, talking to the J-Link probe directly via
//! `probe-rs`/`jaylink` instead of `libnrfjprogdll.so` + `libjlinkarm.so`.

mod crc;
mod logs;
mod options;
mod rtt;
mod slip;
mod tap;

use std::mem;
use std::sync::atomic::{AtomicBool, Ordering};
use std::time::Duration;

use crc::crc16_ccitt;
use logs::{
    print_error, print_info, unrecoverable_fatal_errno, RECOVERABLE_EXIT_CODE, TERMINATION_EXIT_CODE,
};
use options::Options;
use rtt::JlinkRtt;
use slip::{slip_encode, SlipDecoder};
use tap::Tap;

/// Frame sent when the link (re)starts, so the other end can tell that this side was reset.
/// Must match `reset_frame_data` in `drivers/net/eth_rtt.c` exactly.
const RESET_FRAME_DATA: [u8; 32] = [
    0, 0, 0, 0, 0, 0, // dummy destination MAC address
    0, 0, 0, 0, 0, 0, // dummy source MAC address
    254, 255, // custom eth type
    216, 33, 105, 148, 78, 111, // randomly generated magic payload
    203, 53, 32, 137, 247, 122, // randomly generated magic payload
    100, 72, 129, 255, 204, 173, // randomly generated magic payload
];

static EXIT_LOOP: AtomicBool = AtomicBool::new(false);

fn exit_loop() -> bool {
    EXIT_LOOP.load(Ordering::SeqCst)
}

fn set_exit_loop(value: bool) {
    EXIT_LOOP.store(value, Ordering::SeqCst);
}

extern "C" fn handle_sigint(sig: libc::c_int) {
    logs::print_debug(&format!("Caught signal {sig} in {}", unsafe { libc::getpid() }));
    if EXIT_LOOP.load(Ordering::SeqCst) {
        logs::print_error("Forcing process to terminate.");
        std::process::exit(TERMINATION_EXIT_CODE);
    }
    EXIT_LOOP.store(true, Ordering::SeqCst);
}

fn now_secs() -> u64 {
    std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs()
}

fn is_hang(options: &Options) -> bool {
    if let Some(path) = &options.hang_file {
        if std::path::Path::new(path).exists() {
            print_info("Link is hang");
            return true;
        }
    }
    false
}

fn send_frame_to_rtt(rtt: &mut JlinkRtt, frame: &[u8]) {
    let crc = crc16_ccitt(0xFFFF, frame);
    let mut with_crc = Vec::with_capacity(frame.len() + 2);
    with_crc.extend_from_slice(frame);
    with_crc.push((crc >> 8) as u8);
    with_crc.push((crc & 0xFF) as u8);

    let mut encoded = Vec::with_capacity(with_crc.len() + 2);
    slip_encode(&with_crc, &mut encoded);

    print_info(&format!("TO RTT:   {}", encoded.len()));
    rtt.buffered_write(&encoded);
}

/// If retries are enabled, forks a child for each attempt so that a fatal RTT error only tears
/// down the worker, not the TAP interface. Only returns (in the child, or immediately if
/// retries are disabled) once this process should run the actual link.
fn maybe_retry_fork(tap: &mut Tap, options: &Options) {
    if options.no_rtt_retry {
        return;
    }

    loop {
        while is_hang(options) && !exit_loop() {
            std::thread::sleep(Duration::from_secs(1));
        }

        if exit_loop() {
            print_info("TERMINATED parent process");
            std::process::exit(TERMINATION_EXIT_CODE);
        }

        let pid = unsafe { libc::fork() };
        if pid < 0 {
            unrecoverable_fatal_errno("Cannot fork process!");
        } else if pid == 0 {
            if is_hang(options) {
                std::process::exit(RECOVERABLE_EXIT_CODE);
            }
            return;
        } else {
            let mut status: libc::c_int = 0;
            let r = unsafe { libc::waitpid(pid, &mut status, 0) };
            tap.set_state(false);
            print_info(&format!("PID {r}, child {pid}, status {status}"));

            if libc::WIFSIGNALED(status) && !exit_loop() {
                print_error(&format!(
                    "Unexpected child {pid} exit, signal {}",
                    libc::WTERMSIG(status)
                ));
                std::thread::sleep(Duration::from_secs(1));
            } else if libc::WEXITSTATUS(status) == TERMINATION_EXIT_CODE && r >= 0 {
                set_exit_loop(true);
            } else if libc::WEXITSTATUS(status) != RECOVERABLE_EXIT_CODE && r >= 0 {
                std::process::exit(libc::WEXITSTATUS(status));
            } else if !exit_loop() {
                std::thread::sleep(Duration::from_secs(1));
            }
        }
    }
}

fn run_worker(tap: &mut Tap, options: &Options) {
    let mut rtt = JlinkRtt::connect(options);
    tap.set_state(true);
    let mut decoder = SlipDecoder::new();

    print_info("BEGIN");
    send_frame_to_rtt(&mut rtt, &RESET_FRAME_DATA);

    let mut last_hang_check = now_secs();
    let mut buf = vec![0u8; 65536];

    while !exit_loop() {
        let tap_fd = tap.as_raw_fd();
        let mut readfds: libc::fd_set = unsafe { mem::zeroed() };
        unsafe {
            libc::FD_ZERO(&mut readfds);
            libc::FD_SET(tap_fd, &mut readfds);
        }
        let mut tv = libc::timeval {
            tv_sec: 0,
            tv_usec: options.poll_time_us as libc::suseconds_t,
        };
        let ret = unsafe {
            libc::select(
                tap_fd + 1,
                &mut readfds,
                std::ptr::null_mut(),
                std::ptr::null_mut(),
                &mut tv,
            )
        };

        if options.hang_file.is_some() {
            let t = now_secs();
            if last_hang_check != t && is_hang(options) {
                std::process::exit(RECOVERABLE_EXIT_CODE);
            }
            last_hang_check = t;
        }

        if ret > 0 {
            if let Ok(n) = tap.read(&mut buf) {
                if n > 0 {
                    print_info(&format!("FROM ETH: {n}"));
                    send_frame_to_rtt(&mut rtt, &buf[..n]);
                }
            }
        }

        let len = rtt.read(&mut buf);
        if len > 0 {
            print_info(&format!("FROM RTT: {len}"));
            let packets = decoder.put(&buf[..len]);
            for _ in 0..packets {
                let Some(packet) = decoder.read() else {
                    continue;
                };
                if packet.len() <= 2 {
                    continue;
                }

                let crc = crc16_ccitt(0xFFFF, &packet[..packet.len() - 2]);
                let (hi, lo) = (packet[packet.len() - 2], packet[packet.len() - 1]);
                if hi != (crc >> 8) as u8 || lo != (crc & 0xFF) as u8 {
                    print_info("CRC ERROR");
                } else if packet.len() - 2 == RESET_FRAME_DATA.len()
                    && packet[..RESET_FRAME_DATA.len()] == RESET_FRAME_DATA
                {
                    print_info("RESET PACKET");
                    tap.set_state(false);
                    tap.set_state(true);
                } else {
                    let size = packet.len() - 2;
                    print_info(&format!("TO ETH:   {size}"));
                    if tap.write(&packet[..size]).is_err() {
                        print_info("TAP write error");
                    }
                }
            }
        }

        rtt.flush_queue();
    }

    print_info("TERMINATED");
    std::process::exit(TERMINATION_EXIT_CODE);
}

fn main() {
    let options = Options::parse();
    logs::set_verbosity(options.verbose);

    if options.verbose >= logs::LOG_PROBE_RS {
        tracing_subscriber::fmt()
            .with_max_level(tracing_subscriber::filter::LevelFilter::DEBUG)
            .init();
    }

    let mut tap = Tap::create(&options);

    unsafe {
        libc::signal(libc::SIGINT, handle_sigint as *const () as libc::sighandler_t);
    }

    maybe_retry_fork(&mut tap, &options);

    run_worker(&mut tap, &options);
}
