//! CPU clock fallback via WMI when sysinfo reports 0 MHz.

use std::time::{Duration, Instant};

/// Fallback CPU clock if sysinfo reports 0 (queried at most every 10 s).
pub struct ClockFallback {
    last: Instant,
    value: Option<u32>,
    #[cfg(windows)]
    conn: Option<wmi::WMIConnection>,
}

impl ClockFallback {
    pub fn new() -> Self {
        ClockFallback {
            last: Instant::now() - Duration::from_secs(3600),
            value: None,
            #[cfg(windows)]
            conn: None,
        }
    }

    #[cfg(windows)]
    pub fn get(&mut self) -> Option<u32> {
        if self.last.elapsed() < Duration::from_secs(10) {
            return self.value;
        }
        self.last = Instant::now();
        if self.conn.is_none() {
            self.conn = wmi::WMIConnection::new().ok();
        }
        self.value = self.conn.as_ref().and_then(|c| {
            let rows: Vec<std::collections::HashMap<String, wmi::Variant>> =
                c.raw_query("SELECT CurrentClockSpeed FROM Win32_Processor").ok()?;
            rows.iter()
                .filter_map(|r| match r.get("CurrentClockSpeed")? {
                    wmi::Variant::UI4(v) => Some(*v),
                    wmi::Variant::UI2(v) => Some(u32::from(*v)),
                    _ => None,
                })
                .max()
        });
        self.value
    }

    #[cfg(not(windows))]
    pub fn get(&mut self) -> Option<u32> {
        None
    }
}
