//! State shared between the metrics collector, the transports and the tray/headless front end.

use crate::config::{Config, TransportMode};
use crate::protocol::Telemetry;
use std::sync::atomic::{AtomicBool, AtomicU8, Ordering};
use std::sync::{Mutex, MutexGuard};
use std::time::{Duration, Instant};

/// The Flipper marks the link lost after 5 s without TELEMETRY (docs/PROTOCOL.md), so even if
/// the HELLO asks for a slower interval we never leave more than this between frames.
pub const MAX_SEND_PERIOD_S: u8 = 4;

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum LinkKind {
    Ble,
    Usb,
}

impl LinkKind {
    pub fn label(self) -> &'static str {
        match self {
            LinkKind::Ble => "BLE",
            LinkKind::Usb => "USB",
        }
    }
}

#[derive(Clone, Debug)]
pub struct LinkInfo {
    pub kind: LinkKind,
    pub label: String,
}

#[derive(Default)]
struct Latest {
    generation: u64,
    telemetry: Option<Telemetry>,
    summary: String,
}

pub fn lock<T>(m: &Mutex<T>) -> MutexGuard<'_, T> {
    m.lock().unwrap_or_else(|e| e.into_inner())
}

pub struct Shared {
    pub stop: AtomicBool,
    transport: AtomicU8,
    interval_hint: AtomicU8,
    default_interval: AtomicU8,
    latest: Mutex<Latest>,
    link: Mutex<Option<LinkInfo>>,
    ble_address: Mutex<Option<u64>>,
    pairing: AtomicBool,
    notice: Mutex<(String, Instant)>,
    pub simulate: bool,
}

fn mode_to_u8(m: TransportMode) -> u8 {
    match m {
        TransportMode::Auto => 0,
        TransportMode::Ble => 1,
        TransportMode::Usb => 2,
    }
}

impl Shared {
    pub fn new(cfg: &Config, simulate: bool) -> Shared {
        Shared {
            stop: AtomicBool::new(false),
            transport: AtomicU8::new(mode_to_u8(cfg.transport)),
            interval_hint: AtomicU8::new(0),
            default_interval: AtomicU8::new(cfg.interval_s.clamp(1, 10)),
            latest: Mutex::new(Latest::default()),
            link: Mutex::new(None),
            ble_address: Mutex::new(cfg.device_address()),
            pairing: AtomicBool::new(false),
            notice: Mutex::new((String::new(), Instant::now())),
            simulate,
        }
    }

    pub fn stopping(&self) -> bool {
        self.stop.load(Ordering::Relaxed)
    }

    /// Sleep up to `d`, waking early when the app is quitting.
    pub fn sleep(&self, d: Duration) {
        let mut left = d;
        let slice = Duration::from_millis(100);
        while !left.is_zero() && !self.stopping() {
            let s = left.min(slice);
            std::thread::sleep(s);
            left -= s;
        }
    }

    // ---- transport selection -------------------------------------------------------------

    pub fn transport(&self) -> TransportMode {
        match self.transport.load(Ordering::Relaxed) {
            1 => TransportMode::Ble,
            2 => TransportMode::Usb,
            _ => TransportMode::Auto,
        }
    }

    pub fn set_transport(&self, m: TransportMode) {
        self.transport.store(mode_to_u8(m), Ordering::Relaxed);
    }

    pub fn transport_allows(&self, k: LinkKind) -> bool {
        matches!(
            (self.transport(), k),
            (TransportMode::Auto, _) | (TransportMode::Ble, LinkKind::Ble) | (TransportMode::Usb, LinkKind::Usb)
        )
    }

    // ---- link ownership (USB outranks BLE) ------------------------------------------------

    /// Try to become the active link. USB preempts BLE; BLE cannot preempt USB.
    pub fn claim(&self, kind: LinkKind, label: &str) -> bool {
        let mut g = lock(&self.link);
        if matches!(&*g, Some(cur) if cur.kind == LinkKind::Usb && kind == LinkKind::Ble) {
            return false;
        }
        *g = Some(LinkInfo { kind, label: label.to_string() });
        drop(g);
        self.set_notice("");
        true
    }

    pub fn owns(&self, kind: LinkKind) -> bool {
        matches!(&*lock(&self.link), Some(cur) if cur.kind == kind)
    }

    pub fn release(&self, kind: LinkKind) {
        let mut g = lock(&self.link);
        if matches!(&*g, Some(cur) if cur.kind == kind) {
            *g = None;
            self.interval_hint.store(0, Ordering::Relaxed);
        }
    }

    pub fn link(&self) -> Option<LinkInfo> {
        lock(&self.link).clone()
    }

    pub fn set_interval_hint(&self, s: u8) {
        self.interval_hint.store(s.clamp(1, 10), Ordering::Relaxed);
    }

    /// Effective period between telemetry frames.
    pub fn send_period(&self) -> Duration {
        let hint = self.interval_hint.load(Ordering::Relaxed);
        let s = if hint != 0 { hint } else { self.default_interval.load(Ordering::Relaxed) };
        Duration::from_secs(u64::from(s.clamp(1, MAX_SEND_PERIOD_S)))
    }

    // ---- BLE device / pairing -------------------------------------------------------------

    pub fn ble_address(&self) -> Option<u64> {
        *lock(&self.ble_address)
    }

    pub fn set_ble_address(&self, a: Option<u64>) {
        *lock(&self.ble_address) = a;
    }

    pub fn pairing(&self) -> bool {
        self.pairing.load(Ordering::Relaxed)
    }

    /// Returns false if a pairing run is already in progress.
    pub fn begin_pairing(&self) -> bool {
        !self.pairing.swap(true, Ordering::SeqCst)
    }

    pub fn end_pairing(&self) {
        self.pairing.store(false, Ordering::SeqCst);
    }

    /// Short transient message shown in the tray status line (e.g. a pairing result).
    pub fn set_notice(&self, s: &str) {
        *lock(&self.notice) = (s.to_string(), Instant::now());
    }

    /// The current notice, if it is less than a minute old.
    pub fn notice(&self) -> String {
        let g = lock(&self.notice);
        if g.1.elapsed() < Duration::from_secs(60) {
            g.0.clone()
        } else {
            String::new()
        }
    }

    // ---- telemetry hand-off -----------------------------------------------------------------

    pub fn publish(&self, t: Telemetry, summary: String) {
        let mut g = lock(&self.latest);
        g.generation += 1;
        g.telemetry = Some(t);
        g.summary = summary;
    }

    /// The newest telemetry sample if its generation differs from `seen`.
    pub fn newer_than(&self, seen: u64) -> Option<(u64, Telemetry)> {
        let g = lock(&self.latest);
        match (&g.telemetry, g.generation != seen) {
            (Some(t), true) => Some((g.generation, t.clone())),
            _ => None,
        }
    }

    pub fn summary(&self) -> String {
        lock(&self.latest).summary.clone()
    }

    /// Text for the tray status line.
    pub fn status_line(&self) -> String {
        if let Some(l) = self.link() {
            return format!("Connected via {} to {}", l.kind.label(), l.label);
        }
        if self.pairing() {
            return "Pairing with Flipper...".into();
        }
        let note = self.notice();
        if !note.is_empty() {
            return note;
        }
        "Waiting for Flipper".into()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn usb_outranks_ble() {
        let s = Shared::new(&Config::default(), false);
        assert!(s.claim(LinkKind::Ble, "PCHealth 1234"));
        assert!(s.claim(LinkKind::Usb, "COM5"));
        assert!(!s.claim(LinkKind::Ble, "PCHealth 1234"));
        assert!(!s.owns(LinkKind::Ble));
        s.release(LinkKind::Ble); // no-op: not the owner
        assert!(s.owns(LinkKind::Usb));
        s.release(LinkKind::Usb);
        assert!(s.link().is_none());
        assert!(s.claim(LinkKind::Ble, "x"));
    }

    #[test]
    fn period_is_hint_capped() {
        let s = Shared::new(&Config::default(), false);
        assert_eq!(s.send_period(), Duration::from_secs(1));
        s.set_interval_hint(2);
        assert_eq!(s.send_period(), Duration::from_secs(2));
        s.set_interval_hint(10);
        assert_eq!(s.send_period(), Duration::from_secs(u64::from(MAX_SEND_PERIOD_S)));
    }

    #[test]
    fn transport_filter() {
        let s = Shared::new(&Config { transport: TransportMode::Usb, ..Config::default() }, false);
        assert!(s.transport_allows(LinkKind::Usb));
        assert!(!s.transport_allows(LinkKind::Ble));
        s.set_transport(TransportMode::Auto);
        assert!(s.transport_allows(LinkKind::Ble));
    }
}
