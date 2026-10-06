//! Transport-independent session logic: wait (read-only) for HELLO, then stream TELEMETRY.

use crate::protocol::{Frame, FrameParser};
use crate::shared::{LinkKind, Shared};
use anyhow::Result;
use std::time::{Duration, Instant};

/// A byte pipe to the Flipper.
pub trait Link {
    /// Wait up to `timeout` for incoming bytes. An empty vector means nothing arrived; `Err`
    /// means the link is gone.
    fn recv(&mut self, timeout: Duration) -> Result<Vec<u8>>;
    /// Write one complete frame.
    fn send(&mut self, frame: &[u8]) -> Result<()>;
}

/// After the first HELLO the Flipper repeats it every 10 s; three misses means it is gone
/// (or, on USB, that the port now belongs to the Flipper CLI, where we must stop writing).
pub const HELLO_WATCHDOG: Duration = Duration::from_secs(25);

const POLL: Duration = Duration::from_millis(50);

#[derive(Debug, PartialEq, Eq)]
pub enum SessionEnd {
    /// Application is quitting.
    Stopped,
    /// No valid HELLO arrived in time (nothing was ever written).
    NoHello,
    /// A higher-priority link owns the Flipper.
    Busy,
    /// A higher-priority link took over, or the transport was disabled from the menu.
    Yielded,
    /// The Flipper stopped sending HELLO.
    HelloLost,
}

/// Run one session on `link`. Returns `Err` when the link breaks.
pub fn run(
    link: &mut dyn Link,
    kind: LinkKind,
    label: &str,
    shared: &Shared,
    hello_timeout: Duration,
) -> Result<SessionEnd> {
    let mut parser = FrameParser::new();

    // Phase 1: READ ONLY until a valid HELLO.
    let started = Instant::now();
    let first = loop {
        if shared.stopping() {
            return Ok(SessionEnd::Stopped);
        }
        if started.elapsed() >= hello_timeout {
            return Ok(SessionEnd::NoHello);
        }
        let data = link.recv(POLL)?;
        if let Some(h) = parser.push(&data).into_iter().find_map(|f| match f {
            Frame::Hello(h) => Some(h),
            Frame::Telemetry(_) => None,
        }) {
            break h;
        }
    };

    if !shared.claim(kind, label) {
        return Ok(SessionEnd::Busy);
    }
    log::info!(
        "{} link up: {label} (Flipper app v{}.{:02}, requested interval {} s)",
        kind.label(),
        first.app_version / 100,
        first.app_version % 100,
        first.interval_s
    );
    shared.set_interval_hint(first.interval_s);

    let result = stream(link, kind, shared, &mut parser);
    shared.release(kind);
    match &result {
        Ok(end) => log::info!("{} link closed: {end:?}", kind.label()),
        Err(e) => log::warn!("{} link error: {e:#}", kind.label()),
    }
    result
}

fn stream(link: &mut dyn Link, kind: LinkKind, shared: &Shared, parser: &mut FrameParser) -> Result<SessionEnd> {
    let mut seq: u8 = 0;
    let mut last_hello = Instant::now();
    // Start from a generation that never matches so the newest sample goes out right away.
    let mut sent_gen = u64::MAX;

    loop {
        if shared.stopping() {
            return Ok(SessionEnd::Stopped);
        }
        if !shared.owns(kind) || !shared.transport_allows(kind) {
            return Ok(SessionEnd::Yielded);
        }
        if last_hello.elapsed() > HELLO_WATCHDOG {
            return Ok(SessionEnd::HelloLost);
        }

        let data = link.recv(POLL)?;
        for f in parser.push(&data) {
            if let Frame::Hello(h) = f {
                last_hello = Instant::now();
                shared.set_interval_hint(h.interval_s);
            }
        }

        if let Some((gen, t)) = shared.newer_than(sent_gen) {
            link.send(&t.encode(seq))?;
            seq = seq.wrapping_add(1);
            sent_gen = gen;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::config::Config;
    use crate::protocol::{parse_frame, Hello, Telemetry};
    use std::collections::VecDeque;
    use std::sync::Arc;

    struct MockLink {
        incoming: VecDeque<Vec<u8>>,
        sent: Vec<Vec<u8>>,
        shared: Arc<Shared>,
        publish_after_polls: usize,
        polls: usize,
        sent_before_hello: bool,
        saw_hello: bool,
    }

    impl Link for MockLink {
        fn recv(&mut self, _t: Duration) -> Result<Vec<u8>> {
            self.polls += 1;
            if self.polls == self.publish_after_polls {
                self.shared.publish(Telemetry::default(), "x".into());
            }
            if self.polls > self.publish_after_polls.saturating_add(3) {
                self.shared.stop.store(true, std::sync::atomic::Ordering::Relaxed);
            }
            let d = self.incoming.pop_front().unwrap_or_default();
            if d.len() == crate::protocol::HELLO_LEN {
                self.saw_hello = true;
            }
            Ok(d)
        }
        fn send(&mut self, f: &[u8]) -> Result<()> {
            if !self.saw_hello {
                self.sent_before_hello = true;
            }
            self.sent.push(f.to_vec());
            Ok(())
        }
    }

    #[test]
    fn writes_nothing_before_hello_then_streams() {
        let shared = Arc::new(Shared::new(&Config::default(), false));
        // Telemetry is available from the start, but HELLO only arrives on the 4th poll.
        shared.publish(Telemetry::default(), "x".into());
        let hello = Hello { seq: 0, app_version: 100, transport: 1, interval_s: 2 }.encode();
        let mut incoming: VecDeque<Vec<u8>> = VecDeque::new();
        incoming.extend([vec![], vec![0x01, 0x02], vec![], hello.to_vec()]);
        let mut link = MockLink {
            incoming,
            sent: vec![],
            shared: shared.clone(),
            publish_after_polls: 6,
            polls: 0,
            sent_before_hello: false,
            saw_hello: false,
        };
        let end = run(&mut link, LinkKind::Usb, "COM9", &shared, Duration::from_secs(3)).unwrap();
        assert_eq!(end, SessionEnd::Stopped);
        assert!(!link.sent_before_hello, "wrote before HELLO");
        assert!(!link.sent.is_empty());
        for (i, f) in link.sent.iter().enumerate() {
            assert_eq!(f.len(), 48);
            assert!(parse_frame(f).is_some());
            assert_eq!(f[4] as usize, i);
        }
        assert!(shared.link().is_none(), "ownership released at the end");
    }

    #[test]
    fn no_hello_means_no_writes() {
        let shared = Arc::new(Shared::new(&Config::default(), false));
        shared.publish(Telemetry::default(), "x".into());
        let mut link = MockLink {
            incoming: VecDeque::new(),
            sent: vec![],
            shared: shared.clone(),
            publish_after_polls: usize::MAX,
            polls: 0,
            sent_before_hello: false,
            saw_hello: false,
        };
        let end = run(&mut link, LinkKind::Usb, "COM9", &shared, Duration::from_millis(120)).unwrap();
        assert_eq!(end, SessionEnd::NoHello);
        assert!(link.sent.is_empty());
    }
}
