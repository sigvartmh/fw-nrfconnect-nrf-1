//! Verbosity-gated logging, ported from `experimental-eth-rtt-link/logs.h`.

use std::sync::atomic::{AtomicU8, Ordering};

pub const LOG_NONE: u8 = 0;
pub const LOG_ERROR: u8 = 1;
pub const LOG_INFO: u8 = 2;
pub const LOG_DEBUG: u8 = 3;
/// Kept for parity with the original verbosity levels. Gates probe-rs's own internal
/// `tracing` log output (there is no separate vendor DLL to give messages of its own anymore).
pub const LOG_PROBE_RS: u8 = 4;

pub const OPTIONS_EXIT_CODE: i32 = 1;
pub const RECOVERABLE_EXIT_CODE: i32 = 2;
pub const UNRECOVERABLE_EXIT_CODE: i32 = 3;
pub const TERMINATION_EXIT_CODE: i32 = 4;

static VERBOSITY: AtomicU8 = AtomicU8::new(LOG_ERROR);

pub fn set_verbosity(level: u8) {
    VERBOSITY.store(level, Ordering::Relaxed);
}

pub fn verbosity() -> u8 {
    VERBOSITY.load(Ordering::Relaxed)
}

pub fn print_error(msg: &str) {
    if verbosity() >= LOG_ERROR {
        eprintln!("error: {msg}");
    }
}

pub fn print_info(msg: &str) {
    if verbosity() >= LOG_INFO {
        eprintln!("info: {msg}");
    }
}

pub fn print_debug(msg: &str) {
    if verbosity() >= LOG_DEBUG {
        eprintln!("debug: {msg}");
    }
}

/// Fatal error while parsing command line options. Always printed, since verbosity has not
/// necessarily been established yet. Mirrors `O_FATAL`.
pub fn options_fatal(msg: &str) -> ! {
    eprintln!("fatal: {msg}");
    std::process::exit(OPTIONS_EXIT_CODE);
}

/// Fatal error from which the retry loop can recover by restarting the process. Mirrors
/// `R_FATAL`.
pub fn recoverable_fatal(msg: &str) -> ! {
    if verbosity() >= LOG_ERROR {
        eprintln!("fatal: {msg}");
    }
    std::process::exit(RECOVERABLE_EXIT_CODE);
}

/// Fatal, unrecoverable error (e.g. cannot set up the TAP interface), additionally reporting
/// `errno`. Mirrors `U_ERRNO_FATAL`.
pub fn unrecoverable_fatal_errno(msg: &str) -> ! {
    let err = std::io::Error::last_os_error();
    if verbosity() >= LOG_ERROR {
        eprintln!("fatal: {msg}\n    {err}");
    }
    std::process::exit(UNRECOVERABLE_EXIT_CODE);
}
