//! Linux TAP interface management, ported from `experimental-eth-rtt-link/tap.c`.

use std::ffi::CString;
use std::fs::File;
use std::io::{Read, Write};
use std::mem;
use std::net::{Ipv4Addr, Ipv6Addr};
use std::os::fd::{AsRawFd, FromRawFd, OwnedFd, RawFd};

use crate::logs::unrecoverable_fatal_errno;
use crate::options::Options;

const TUN_DEV_NAME: &str = "/dev/net/tun\0";
const SUPPORTED_FLAGS: i16 = (libc::IFF_UP | libc::IFF_BROADCAST | libc::IFF_RUNNING) as i16;

#[repr(C)]
struct In6Ifreq {
    ifr6_addr: libc::in6_addr,
    ifr6_prefixlen: u32,
    ifr6_ifindex: libc::c_int,
}

fn ifreq_with_name(name: &[u8; libc::IFNAMSIZ]) -> libc::ifreq {
    let mut ifr: libc::ifreq = unsafe { mem::zeroed() };
    ifr.ifr_name = name.map(|b| b as libc::c_char);
    ifr
}

fn checked_ioctl(fd: RawFd, request: libc::Ioctl, ifr: *mut libc::ifreq, what: &str) {
    let res = unsafe { libc::ioctl(fd, request, ifr) };
    if res < 0 {
        unrecoverable_fatal_errno(&format!("Cannot {what}."));
    }
}

fn name_bytes(name: &str) -> [u8; libc::IFNAMSIZ] {
    let mut buf = [0u8; libc::IFNAMSIZ];
    let bytes = name.as_bytes();
    let len = bytes.len().min(libc::IFNAMSIZ - 1);
    buf[..len].copy_from_slice(&bytes[..len]);
    buf
}

fn ifr_name_str(ifr: &libc::ifreq) -> String {
    let bytes: [u8; libc::IFNAMSIZ] = ifr.ifr_name.map(|c| c as u8);
    let len = bytes.iter().position(|&b| b == 0).unwrap_or(bytes.len());
    String::from_utf8_lossy(&bytes[..len]).into_owned()
}

/// Owns the TAP file descriptor and the interface's configuration socket.
pub struct Tap {
    fd: File,
    conf_socket: Option<OwnedFd>,
    iface_name: [u8; libc::IFNAMSIZ],
    tap_flags: i16,
    ipv6_address: Option<(Ipv6Addr, u32)>,
}

fn setup_ipv4_data(socket: RawFd, cmd: libc::Ioctl, name: &[u8; libc::IFNAMSIZ], addr: Ipv4Addr, what: &str) {
    let mut ifr = ifreq_with_name(name);
    let sockaddr_in = libc::sockaddr_in {
        sin_family: libc::AF_INET as libc::sa_family_t,
        sin_port: 0,
        sin_addr: libc::in_addr {
            s_addr: u32::from_ne_bytes(addr.octets()),
        },
        sin_zero: [0; 8],
    };
    unsafe {
        std::ptr::write(
            &mut ifr.ifr_ifru.ifru_addr as *mut libc::sockaddr as *mut libc::sockaddr_in,
            sockaddr_in,
        );
    }
    checked_ioctl(socket, cmd, &mut ifr, &format!("change IPv4 {what}"));
}

impl Tap {
    pub fn create(options: &Options) -> Tap {
        let dev_name = CString::new(&TUN_DEV_NAME[..TUN_DEV_NAME.len() - 1]).unwrap();
        let raw_fd = unsafe { libc::open(dev_name.as_ptr(), libc::O_RDWR) };
        if raw_fd < 0 {
            unrecoverable_fatal_errno("Cannot open TUN/TAP driver.");
        }
        let fd = unsafe { File::from_raw_fd(raw_fd) };

        let requested_name = options.iface.as_deref().unwrap_or("");
        let mut ifr = ifreq_with_name(&name_bytes(requested_name));
        ifr.ifr_ifru.ifru_flags = (libc::IFF_TAP | libc::IFF_NO_PI) as i16;
        checked_ioctl(fd.as_raw_fd(), libc::TUNSETIFF, &mut ifr, "create TAP interface");

        let iface_name = ifr.ifr_name.map(|c| c as u8);

        crate::logs::print_info(&format!("TAP interface on \"{}\".", ifr_name_str(&ifr)));

        if let Some(mac) = options.mac_addr {
            let mut ifr = ifreq_with_name(&iface_name);
            let hwaddr = libc::sockaddr {
                sa_family: libc::ARPHRD_ETHER,
                sa_data: {
                    let mut data = [0i8; 14];
                    for i in 0..6 {
                        data[i] = mac[i] as i8;
                    }
                    data
                },
            };
            ifr.ifr_ifru.ifru_hwaddr = hwaddr;
            checked_ioctl(fd.as_raw_fd(), libc::SIOCSIFHWADDR, &mut ifr, "setup MAC address");
        }

        let need_ipv4_socket = options.ipv4_address.is_some()
            || options.ipv4_netmask.is_some()
            || options.ipv4_broadcast.is_some()
            || options.mtu.is_some()
            || options.ipv6_address.is_none();

        let mut ipv4_socket: Option<RawFd> = None;
        if need_ipv4_socket {
            let s = unsafe { libc::socket(libc::AF_INET, libc::SOCK_DGRAM, 0) };
            if s < 0 {
                unrecoverable_fatal_errno("Cannot create socket for IPv4.");
            }
            ipv4_socket = Some(s);
        }

        let mut tap_flags = 0i16;

        if let Some(mtu) = options.mtu {
            let mut ifr = ifreq_with_name(&iface_name);
            ifr.ifr_ifru.ifru_mtu = mtu as libc::c_int;
            checked_ioctl(ipv4_socket.unwrap(), libc::SIOCSIFMTU, &mut ifr, "set interface MTU");
        }

        if let Some(addr) = options.ipv4_address {
            setup_ipv4_data(ipv4_socket.unwrap(), libc::SIOCSIFADDR, &iface_name, addr, "address");
        }

        if let Some(mask) = options.ipv4_netmask {
            setup_ipv4_data(ipv4_socket.unwrap(), libc::SIOCSIFNETMASK, &iface_name, mask, "netmask");
        }

        if let Some(bcast) = options.ipv4_broadcast {
            setup_ipv4_data(
                ipv4_socket.unwrap(),
                libc::SIOCSIFBRDADDR,
                &iface_name,
                bcast,
                "broadcast address",
            );
            tap_flags |= libc::IFF_BROADCAST as i16;
        }

        let ipv6_address = options
            .ipv6_address
            .map(|addr| (addr, options.ipv6_subnet_bits.unwrap_or(64)));

        let conf_socket = if let Some((_, _)) = ipv6_address {
            let s = unsafe { libc::socket(libc::AF_INET6, libc::SOCK_DGRAM, 0) };
            if s < 0 {
                unrecoverable_fatal_errno("Cannot create IPv6 socket.");
            }
            if let Some(v4) = ipv4_socket {
                unsafe {
                    libc::close(v4);
                }
            }
            Some(unsafe { OwnedFd::from_raw_fd(s) })
        } else {
            ipv4_socket.map(|s| unsafe { OwnedFd::from_raw_fd(s) })
        };

        let tap = Tap {
            fd,
            conf_socket,
            iface_name,
            tap_flags,
            ipv6_address,
        };
        tap.renew_ipv6_address();
        tap
    }

    fn renew_ipv6_address(&self) {
        let Some((addr, prefix_len)) = self.ipv6_address else {
            return;
        };
        let Some(conf_socket) = &self.conf_socket else {
            return;
        };

        let mut ifr = ifreq_with_name(&self.iface_name);
        checked_ioctl(
            conf_socket.as_raw_fd(),
            libc::SIOCGIFINDEX,
            &mut ifr,
            "get interface index",
        );
        let ifindex = unsafe { ifr.ifr_ifru.ifru_ifindex };

        let mut ifr6 = In6Ifreq {
            ifr6_addr: libc::in6_addr {
                s6_addr: addr.octets(),
            },
            ifr6_prefixlen: prefix_len,
            ifr6_ifindex: ifindex,
        };
        let res = unsafe {
            libc::ioctl(
                conf_socket.as_raw_fd(),
                libc::SIOCSIFADDR,
                &mut ifr6 as *mut In6Ifreq,
            )
        };
        if res < 0 {
            let err = std::io::Error::last_os_error();
            if err.raw_os_error() != Some(libc::EEXIST) {
                unrecoverable_fatal_errno("Cannot set IPv6 address.");
            }
        }
    }

    fn set_flags(&self) {
        let Some(conf_socket) = &self.conf_socket else {
            return;
        };

        let mut ifr = ifreq_with_name(&self.iface_name);
        checked_ioctl(
            conf_socket.as_raw_fd(),
            libc::SIOCGIFFLAGS,
            &mut ifr,
            "read interface flags",
        );
        let old_flags = unsafe { ifr.ifr_ifru.ifru_flags } & !SUPPORTED_FLAGS;

        let mut ifr = ifreq_with_name(&self.iface_name);
        ifr.ifr_ifru.ifru_flags = old_flags | self.tap_flags;
        checked_ioctl(
            conf_socket.as_raw_fd(),
            libc::SIOCSIFFLAGS,
            &mut ifr,
            "set interface flags",
        );
    }

    pub fn set_state(&mut self, up: bool) {
        if up {
            self.tap_flags |= (libc::IFF_UP | libc::IFF_RUNNING) as i16;
        } else {
            self.tap_flags &= !(libc::IFF_UP as i16);
        }

        self.set_flags();

        if up {
            self.renew_ipv6_address();
        }
    }

    pub fn read(&mut self, buf: &mut [u8]) -> std::io::Result<usize> {
        self.fd.read(buf)
    }

    pub fn write(&mut self, buf: &[u8]) -> std::io::Result<usize> {
        self.fd.write(buf)
    }

    pub fn as_raw_fd(&self) -> RawFd {
        self.fd.as_raw_fd()
    }
}
