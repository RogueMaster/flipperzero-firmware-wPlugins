//! CPU temperature and (optional) fan RPM sources.
//!
//! Windows: LibreHardwareMonitor WMI -> ThermalZoneInformation perf counter -> MSAcpi (admin).
//! Linux: sysinfo components (hwmon); fan RPM from hwmon `fan*_input`.

#[cfg(windows)]
#[path = "temp_win.rs"]
mod imp;

#[cfg(not(windows))]
mod imp {
    use super::SensorReadings;

    #[cfg(target_os = "linux")]
    pub struct Imp {
        components: sysinfo::Components,
        /// `(label, fan*_input path)` for every hwmon fan, found once at startup.
        fans: Vec<(String, std::path::PathBuf)>,
    }

    #[cfg(target_os = "linux")]
    fn hwmon_fans() -> Vec<(String, std::path::PathBuf)> {
        let mut out = Vec::new();
        let Ok(dirs) = std::fs::read_dir("/sys/class/hwmon") else { return out };
        for dir in dirs.flatten().map(|d| d.path()) {
            let chip = std::fs::read_to_string(dir.join("name")).unwrap_or_default();
            let Ok(files) = std::fs::read_dir(&dir) else { continue };
            for f in files.flatten() {
                let name = f.file_name().to_string_lossy().into_owned();
                let Some(n) = name.strip_prefix("fan").and_then(|r| r.strip_suffix("_input")) else { continue };
                let label = std::fs::read_to_string(dir.join(format!("fan{n}_label"))).unwrap_or_default();
                out.push((format!("{} {}", chip.trim(), label.trim()), f.path()));
            }
        }
        out
    }

    #[cfg(target_os = "linux")]
    impl Imp {
        pub fn new() -> Self {
            Imp { components: sysinfo::Components::new_with_refreshed_list(), fans: hwmon_fans() }
        }

        /// Prefer a fan labelled CPU, otherwise the fastest one.
        fn fan_rpm(&self) -> Option<f64> {
            let read = |p: &std::path::Path| std::fs::read_to_string(p).ok()?.trim().parse::<f64>().ok();
            let rpms: Vec<(bool, f64)> = self
                .fans
                .iter()
                .filter_map(|(l, p)| Some((l.to_ascii_lowercase().contains("cpu"), read(p)?)))
                .filter(|(_, r)| *r > 0.0)
                .collect();
            let max = |cpu_only: bool| rpms.iter().filter(|(c, _)| *c || !cpu_only).map(|(_, r)| *r).reduce(f64::max);
            max(true).or_else(|| max(false))
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
            let fan_rpm = self.fan_rpm();
            SensorReadings {
                cpu_temp: best.map(|b| b.1),
                temp_source: best.map(|_| "sysinfo components (hwmon)"),
                fan_rpm,
                fan_source: fan_rpm.map(|_| "hwmon fan*_input"),
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
    /// Fan speed in RPM: LibreHardwareMonitor on Windows, hwmon on Linux.
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
