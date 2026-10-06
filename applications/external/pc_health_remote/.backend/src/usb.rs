//! USB CDC transport (Flipper Zero VID 0x0483 / PID 0x5740).
//!
//! The Flipper CLI uses the same VID/PID, so the port is opened read-only in spirit: nothing is
//! ever written until a valid HELLO frame has been received (enforced in `session::run`).

use crate::config::TransportMode;
use crate::session::{self, Link, SessionEnd};
use crate::shared::{LinkKind, Shared};
use anyhow::{Context, Result};
use serialport::{SerialPort, SerialPortType};
use std::collections::HashMap;
use std::io::{Read, Write};
use std::sync::Arc;
use std::thread::JoinHandle;
use std::time::{Duration, Instant};

const VID: u16 = 0x0483;
const PID: u16 = 0x5740;
const BAUD: u32 = 115_200;
const HELLO_TIMEOUT: Duration = Duration::from_secs(3);
const RESCAN: Duration = Duration::from_secs(2);
/// A port that stayed silent (e.g. the Flipper is in CLI mode) is left alone for a while.
const NO_HELLO_BACKOFF: Duration = Duration::from_secs(3);
/// Auto mode: how long a probe waits for HELLO before closing the port again (the Flipper app
/// sends HELLO every second until it gets TELEMETRY).
const AUTO_PROBE_HELLO: Duration = Duration::from_millis(1500);
/// Auto mode: minimum time between two probe attempts (the port is free in between, so
/// qFlipper and the Flipper CLI can use it).
const AUTO_PROBE_EVERY: Duration = Duration::from_secs(5);

/// What the USB worker may do right now.
#[derive(Debug, PartialEq, Eq)]
enum Plan {
    /// Do not touch the COM port.
    Idle,
    /// Open the port and wait up to `hello` for HELLO. `throttle_all` limits every attempt
    /// (also failed opens) to one per `gap`; otherwise only silent ports are rate-limited.
    Listen { hello: Duration, gap: Duration, throttle_all: bool },
}

fn plan(shared: &Shared) -> Plan {
    match shared.transport() {
        TransportMode::Ble => Plan::Idle,
        // Explicit USB: the port belongs to us, keep listening.
        TransportMode::Usb => Plan::Listen { hello: HELLO_TIMEOUT, gap: NO_HELLO_BACKOFF, throttle_all: false },
        // Auto: never touch the port while BLE is up; otherwise only short, rare probes.
        TransportMode::Auto if shared.owns(LinkKind::Ble) => Plan::Idle,
        TransportMode::Auto => Plan::Listen { hello: AUTO_PROBE_HELLO, gap: AUTO_PROBE_EVERY, throttle_all: true },
    }
}

struct SerialLink {
    port: Box<dyn SerialPort>,
    buf: [u8; 256],
}

impl Link for SerialLink {
    fn recv(&mut self, _timeout: Duration) -> Result<Vec<u8>> {
        // The read timeout is configured on the port when it is opened.
        match self.port.read(&mut self.buf) {
            Ok(n) => Ok(self.buf[..n].to_vec()),
            Err(e) if matches!(e.kind(), std::io::ErrorKind::TimedOut | std::io::ErrorKind::WouldBlock) => {
                Ok(Vec::new())
            }
            Err(e) => Err(e).context("USB read failed"),
        }
    }

    fn send(&mut self, frame: &[u8]) -> Result<()> {
        self.port.write_all(frame).context("USB write failed")?;
        self.port.flush().context("USB flush failed")
    }
}

/// Names of all serial ports that look like a Flipper.
pub fn flipper_ports() -> Vec<String> {
    match serialport::available_ports() {
        Ok(list) => list
            .into_iter()
            .filter(|p| matches!(&p.port_type, SerialPortType::UsbPort(u) if u.vid == VID && u.pid == PID))
            .map(|p| p.port_name)
            .collect(),
        Err(e) => {
            log::debug!("serial port enumeration failed: {e}");
            Vec::new()
        }
    }
}

fn open(name: &str) -> Result<SerialLink> {
    let mut port = serialport::new(name, BAUD)
        .timeout(Duration::from_millis(50))
        .open()
        .with_context(|| format!("opening {name}"))?;
    port.write_data_terminal_ready(true).context("setting DTR")?;
    Ok(SerialLink { port, buf: [0; 256] })
}

pub fn spawn(shared: Arc<Shared>) -> JoinHandle<()> {
    std::thread::Builder::new().name("usb".into()).spawn(move || worker(&shared)).expect("spawn usb thread")
}

fn worker(shared: &Shared) {
    log::debug!("USB worker started");
    // Ports that answered nothing recently are not hammered again immediately.
    let mut quiet_until: HashMap<String, Instant> = HashMap::new();
    while !shared.stopping() {
        if let Plan::Listen { hello, gap, throttle_all } = plan(shared) {
            for name in flipper_ports() {
                // Re-check the plan before every open: BLE may have come up meanwhile.
                if shared.stopping() || plan(shared) == Plan::Idle {
                    break;
                }
                if quiet_until.get(&name).is_some_and(|t| Instant::now() < *t) {
                    continue;
                }
                if throttle_all {
                    quiet_until.insert(name.clone(), Instant::now() + gap);
                }
                match open(&name) {
                    Ok(mut link) => {
                        log::debug!("listening on {name} for HELLO");
                        match session::run(&mut link, LinkKind::Usb, &name, shared, hello) {
                            Ok(SessionEnd::NoHello) => {
                                log::debug!("{name}: no HELLO within {hello:?}");
                                quiet_until.insert(name.clone(), Instant::now() + gap);
                            }
                            Ok(_) => {}
                            Err(e) => log::debug!("{name}: {e:#}"),
                        }
                        // `link` (and the COM port) is dropped here -> closed.
                    }
                    Err(e) => log::debug!("{e:#}"),
                }
            }
        }
        shared.sleep(RESCAN);
    }
    log::debug!("USB worker stopped");
}

#[cfg(test)]
mod plan_tests {
    use super::*;
    use crate::config::Config;

    fn shared(mode: TransportMode) -> Shared {
        Shared::new(&Config { transport: mode, ..Config::default() }, false)
    }

    #[test]
    fn auto_leaves_the_port_alone_while_ble_is_up() {
        let s = shared(TransportMode::Auto);
        assert_eq!(
            plan(&s),
            Plan::Listen { hello: Duration::from_millis(1500), gap: Duration::from_secs(5), throttle_all: true }
        );
        assert!(s.claim(LinkKind::Ble, "PCHealth"));
        assert_eq!(plan(&s), Plan::Idle);
        s.release(LinkKind::Ble);
        assert_ne!(plan(&s), Plan::Idle);
    }

    #[test]
    fn explicit_modes_keep_their_behavior() {
        let s = shared(TransportMode::Usb);
        assert!(s.claim(LinkKind::Ble, "x"));
        assert_eq!(plan(&s), Plan::Listen { hello: HELLO_TIMEOUT, gap: NO_HELLO_BACKOFF, throttle_all: false });
        assert_eq!(plan(&shared(TransportMode::Ble)), Plan::Idle);
    }
}

#[cfg(all(test, unix))]
mod tests {
    use super::*;
    use crate::config::Config;
    use crate::protocol::{parse_frame, Frame, FrameParser, Hello, Telemetry};
    use serialport::TTYPort;
    use std::sync::atomic::Ordering;

    /// Drive a full session over a pseudo-terminal pair: the "Flipper" side stays silent for a
    /// while (nothing may be written), sends HELLO, then must receive valid TELEMETRY frames.
    #[test]
    fn session_over_pty() {
        let (mut master, mut flipper) = TTYPort::pair().expect("pty pair");
        master.set_timeout(Duration::from_millis(50)).unwrap();
        flipper.set_timeout(Duration::from_millis(50)).unwrap();

        let shared = Arc::new(Shared::new(&Config::default(), false));
        shared.publish(Telemetry { cpu_load: 77, ..Telemetry::default() }, String::new());

        let sh = shared.clone();
        let session = std::thread::spawn(move || {
            let mut link = SerialLink { port: Box::new(master), buf: [0; 256] };
            session::run(&mut link, LinkKind::Usb, "pty", &sh, Duration::from_secs(3))
        });

        // Silence: the backend must not write anything yet.
        std::thread::sleep(Duration::from_millis(400));
        let mut buf = [0u8; 256];
        assert!(flipper.read(&mut buf).is_err(), "backend wrote before HELLO");

        flipper.write_all(&Hello { seq: 0, app_version: 100, transport: 1, interval_s: 1 }.encode()).unwrap();
        let mut parser = FrameParser::new();
        let mut telemetry = Vec::new();
        let t0 = Instant::now();
        while telemetry.is_empty() && t0.elapsed() < Duration::from_secs(3) {
            if let Ok(n) = flipper.read(&mut buf) {
                telemetry.extend(parser.push(&buf[..n]));
            }
        }
        shared.stop.store(true, Ordering::Relaxed);
        assert_eq!(session.join().unwrap().unwrap(), SessionEnd::Stopped);
        match telemetry.first() {
            Some(Frame::Telemetry(t)) => assert_eq!(t.cpu_load, 77),
            other => panic!("expected telemetry, got {other:?}"),
        }
        assert!(parse_frame(&Telemetry::default().encode(0)).is_some());
    }
}
