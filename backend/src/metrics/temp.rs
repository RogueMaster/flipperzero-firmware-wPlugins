//! CPU temperature and (optional) fan RPM sources.
//!
//! Windows: LibreHardwareMonitor WMI -> ThermalZoneInformation perf counter -> MSAcpi (admin).
//! Linux: sysinfo components (hwmon).

#[cfg(windows)]
#[path = "temp_win.rs"]
mod imp;

#[cfg(not(windows))]
mod imp {
    use super::SensorReadings;

    #[cfg(target_os = "linux")]
    pub struct Imp {
        components: sysinfo::Components,
    }

    #[cfg(target_os = "linux")]
    impl Imp {
        pub fn new() -> Self {
            Imp { components: sysinfo::Components::new_with_refreshed_list() }
        }

        pub fn read(&mut self) -> SensorReadings {
            self.components.refresh(false);
            let mut best: Option<(u8, f64)> = None; // (priority, temp)
            for c in self.components.list() {
                let Some(t) = c.temperature().map(f64::from) else { continue };
                if !super::pick::plausible_temp(t) {
                    continue;
                }
                let l = c.label().to_ascii_lowercase();
                let prio = if l.contains("package") || l.contains("tctl") || l.contains("tdie") {
                    3
                } else if l.contains("coretemp") || l.contains("k10temp") || l.contains("cpu") {
                    2
                } else if l.contains("core") {
                    1
                } else {
                    continue;
                };
                best = match best {
                    Some((p, v)) if p > prio || (p == prio && v >= t) => Some((p, v)),
                    _ => Some((prio, t)),
                };
            }
            SensorReadings {
                cpu_temp: best.map(|b| b.1),
                temp_source: best.map(|_| "sysinfo components (hwmon)"),
                ..SensorReadings::default()
            }
        }
    }

    #[cfg(not(target_os = "linux"))]
    pub struct Imp;

    #[cfg(not(target_os = "linux"))]
    impl Imp {
        pub fn new() -> Self {
            Imp
        }

        pub fn read(&mut self) -> SensorReadings {
            SensorReadings::default()
        }
    }
}

pub use super::pick;

#[derive(Debug, Clone, Default)]
pub struct SensorReadings {
    pub cpu_temp: Option<f64>,
    pub temp_source: Option<&'static str>,
    /// Fan speed in RPM; only ever provided by LibreHardwareMonitor.
    pub fan_rpm: Option<f64>,
    pub fan_source: Option<&'static str>,
    /// Human-readable probe results, filled when sources are (re)probed.
    pub diagnostics: Vec<String>,
}

pub struct SensorHub {
    imp: imp::Imp,
}

impl SensorHub {
    pub fn new() -> Self {
        SensorHub { imp: imp::Imp::new() }
    }

    pub fn read(&mut self) -> SensorReadings {
        self.imp.read()
    }
}
