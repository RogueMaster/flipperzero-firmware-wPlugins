//! System metrics collection and conversion to the wire format.

pub mod clock;
pub mod gpu;
pub mod pick;
pub mod power;
pub mod simulate;
pub mod temp;

use crate::protocol::{pack_name, Telemetry};
use serde::Serialize;
use std::collections::{BTreeMap, HashMap};
use std::time::{Duration, Instant};
use sysinfo::{Disks, ProcessRefreshKind, ProcessesToUpdate, System};

#[derive(Debug, Clone, Serialize, Default)]
pub struct ProcUse {
    pub name: String,
    pub pct: f32,
}

#[derive(Debug, Clone, Serialize, Default)]
pub struct Snapshot {
    pub cpu_load_pct: f32,
    pub cpu_temp_c: Option<f32>,
    pub cpu_clock_mhz: Option<u32>,
    pub ram_total_bytes: u64,
    pub ram_used_bytes: u64,
    pub disk_mount: String,
    pub disk_used_pct: f32,
    pub uptime_s: u64,
    pub top_cpu: Option<ProcUse>,
    pub top_ram: Option<ProcUse>,
    pub gpu: Option<gpu::GpuInfo>,
    pub fan_rpm: Option<u32>,
    pub battery: Option<power::Battery>,
    /// Where each value came from (or why it is missing).
    pub sources: BTreeMap<String, String>,
}

fn pct(v: f32) -> u8 {
    if v.is_finite() {
        v.round().clamp(0.0, 100.0) as u8
    } else {
        0
    }
}

/// Bytes -> 0.1 GiB units (RAM 16 GiB -> 160).
fn dgb(bytes: u64) -> u16 {
    ((bytes as f64 / (1u64 << 30) as f64) * 10.0).round().clamp(0.0, f64::from(u16::MAX)) as u16
}

fn ratio_pct(used: u64, total: u64) -> u8 {
    if total == 0 {
        0
    } else {
        pct(used as f32 * 100.0 / total as f32)
    }
}

impl Snapshot {
    pub fn to_telemetry(&self) -> Telemetry {
        let mut t = Telemetry {
            cpu_load: pct(self.cpu_load_pct),
            ram_load: ratio_pct(self.ram_used_bytes, self.ram_total_bytes),
            ram_total_dgb: dgb(self.ram_total_bytes),
            disk_load: pct(self.disk_used_pct),
            cpu_clock_mhz: self.cpu_clock_mhz.unwrap_or(0).min(u32::from(u16::MAX)) as u16,
            uptime_h: (self.uptime_s / 3600).min(u64::from(u16::MAX)) as u16,
            ..Telemetry::default()
        };
        if let Some(c) = self.cpu_temp_c {
            t.cpu_temp_valid = true;
            t.cpu_temp = c.round().clamp(0.0, 255.0) as u8;
        }
        if let Some(g) = &self.gpu {
            t.gpu_present = true;
            t.gpu_load = pct(g.load_pct.unwrap_or(0) as f32);
            if let Some(c) = g.temp_c {
                t.gpu_temp_valid = true;
                t.gpu_temp = c.min(255) as u8;
            }
            if let (Some(u), Some(tot)) = (g.vram_used_bytes, g.vram_total_bytes) {
                t.vram_load = ratio_pct(u, tot);
                t.vram_total_dgb = dgb(tot);
            }
        }
        if let Some(b) = &self.battery {
            t.battery_present = true;
            t.battery = b.percent.min(100);
            t.on_battery = b.on_battery;
            t.charging = b.charging;
        }
        if let Some(rpm) = self.fan_rpm.filter(|r| *r > 0) {
            t.fan_valid = true;
            t.fan_rpm = rpm.min(u32::from(u16::MAX)) as u16;
        }
        if let Some(p) = &self.top_cpu {
            t.top_cpu_pct = pct(p.pct);
            t.top_cpu_name = pack_name(&p.name);
        }
        if let Some(p) = &self.top_ram {
            t.top_ram_pct = pct(p.pct);
            t.top_ram_name = pack_name(&p.name);
        }
        t
    }

    /// One-line summary for the tray tooltip.
    pub fn summary(&self) -> String {
        let temp = |t: Option<f32>| t.map_or("--".to_string(), |v| format!("{v:.0}\u{b0}C"));
        let gpu = match &self.gpu {
            Some(g) => format!(
                "GPU {}% {}",
                g.load_pct.map_or("--".into(), |v| v.to_string()),
                temp(g.temp_c.map(|c| c as f32))
            ),
            None => "GPU n/a".to_string(),
        };
        format!(
            "CPU {:.0}% {} | {} | RAM {}%",
            self.cpu_load_pct,
            temp(self.cpu_temp_c),
            gpu,
            ratio_pct(self.ram_used_bytes, self.ram_total_bytes)
        )
    }
}

/// (top CPU: name + % of all cores, top RAM: name + resident bytes)
type Leaders = (Option<(String, f32)>, Option<(String, u64)>);

/// Windows' pseudo process that stands for idle time, never a real consumer.
fn is_idle_name(name: &str) -> bool {
    name.eq_ignore_ascii_case("System Idle Process") || name.eq_ignore_ascii_case("Idle")
}

/// CPU use of one process between two samples as a percentage of all logical cores
/// (`ncpu` fully busy cores = 100 %). `None` when the interval is too short to mean anything
/// or the counter went backwards.
fn cpu_percent(prev_ms: u64, cur_ms: u64, dt: Duration, ncpu: usize) -> Option<f32> {
    if dt < Duration::from_millis(100) || cur_ms < prev_ms {
        return None;
    }
    let wall_ms = dt.as_secs_f64() * 1000.0 * ncpu.max(1) as f64;
    Some(((cur_ms - prev_ms) as f64 * 100.0 / wall_ms).clamp(0.0, 100.0) as f32)
}

pub struct Collector {
    sys: System,
    /// pid -> (accumulated CPU ms, start time) at the previous tick.
    proc_prev: HashMap<u32, (u64, u64)>,
    proc_at: Option<Instant>,
    disks: Disks,
    last_disk: Instant,
    disk_cache: (String, f32),
    sensors: temp::SensorHub,
    gpu: gpu::Gpu,
    clock_fallback: clock::ClockFallback,
    first: bool,
}

fn system_drive_mount() -> String {
    #[cfg(windows)]
    {
        let d = std::env::var("SystemDrive").unwrap_or_else(|_| "C:".into());
        format!("{}\\", d.trim_end_matches(['\\', '/']))
    }
    #[cfg(not(windows))]
    {
        "/".to_string()
    }
}

impl Collector {
    pub fn new() -> Self {
        let mut sys = System::new();
        sys.refresh_cpu_usage();
        sys.refresh_memory();
        sys.refresh_processes_specifics(
            ProcessesToUpdate::All,
            true,
            ProcessRefreshKind::nothing().with_cpu().with_memory(),
        );
        let mut c = Collector {
            sys,
            proc_prev: HashMap::new(),
            proc_at: None,
            disks: Disks::new_with_refreshed_list(),
            last_disk: Instant::now() - Duration::from_secs(3600),
            disk_cache: (String::new(), 0.0),
            sensors: temp::SensorHub::new(),
            gpu: gpu::Gpu::new(),
            clock_fallback: clock::ClockFallback::new(),
            first: true,
        };
        // Baseline for the first per-process CPU delta.
        let _ = c.process_leaders();
        c
    }

    fn disk_usage(&mut self) -> (String, f32) {
        if self.last_disk.elapsed() >= Duration::from_secs(30) {
            self.last_disk = Instant::now();
            self.disks.refresh(true);
            let want = system_drive_mount();
            let pick = self
                .disks
                .list()
                .iter()
                .find(|d| d.mount_point().to_string_lossy().eq_ignore_ascii_case(&want))
                .or_else(|| self.disks.list().iter().max_by_key(|d| d.total_space()));
            if let Some(d) = pick {
                let total = d.total_space();
                let used = total.saturating_sub(d.available_space());
                let p = if total == 0 { 0.0 } else { used as f32 * 100.0 / total as f32 };
                self.disk_cache = (d.mount_point().to_string_lossy().into_owned(), p);
            }
        }
        self.disk_cache.clone()
    }

    /// Top CPU consumer (name, % of all cores) and top RAM consumer (name, resident bytes).
    /// Uses the already refreshed process table; remembers this tick's CPU times for the next.
    fn process_leaders(&mut self) -> Leaders {
        let now = Instant::now();
        let dt = self.proc_at.map(|t| now.duration_since(t));
        self.proc_at = Some(now);
        let ncpu = self.sys.cpus().len().max(1);
        let own = std::process::id();

        let mut next: HashMap<u32, (u64, u64)> = HashMap::with_capacity(self.proc_prev.len());
        let mut top_cpu: Option<(String, f32, u64)> = None;
        let mut top_ram: Option<(String, u64)> = None;
        for (pid, p) in self.sys.processes() {
            let id = pid.as_u32();
            let name = p.name().to_string_lossy();
            let cpu_ms = p.accumulated_cpu_time();
            next.insert(id, (cpu_ms, p.start_time()));
            if id == 0 || id == own || is_idle_name(&name) {
                continue;
            }
            let mem = p.memory();
            if top_ram.as_ref().is_none_or(|t| mem > t.1) {
                top_ram = Some((name.to_string(), mem));
            }
            // A pid that was reused by a new process (different start time) has no baseline.
            let prev = self.proc_prev.get(&id).filter(|(_, start)| *start == p.start_time()).map(|(ms, _)| *ms);
            let Some(pct) = prev.zip(dt).and_then(|(prev_ms, dt)| cpu_percent(prev_ms, cpu_ms, dt, ncpu)) else {
                continue;
            };
            if top_cpu.as_ref().is_none_or(|t| pct > t.1 || (pct == t.1 && mem > t.2)) {
                top_cpu = Some((name.to_string(), pct, mem));
            }
        }
        self.proc_prev = next;
        (top_cpu.map(|(n, c, _)| (n, c)), top_ram)
    }

    pub fn collect(&mut self) -> Snapshot {
        let mut s = Snapshot::default();
        self.sys.refresh_cpu_usage();
        self.sys.refresh_cpu_frequency();
        self.sys.refresh_memory();
        self.sys.refresh_processes_specifics(
            ProcessesToUpdate::All,
            true,
            ProcessRefreshKind::nothing().with_cpu().with_memory(),
        );

        s.cpu_load_pct = self.sys.global_cpu_usage();
        s.sources.insert("cpu_load".into(), "sysinfo".into());
        s.ram_total_bytes = self.sys.total_memory();
        s.ram_used_bytes = self.sys.used_memory();
        s.uptime_s = System::uptime();
        let (mount, used) = self.disk_usage();
        s.disk_mount = mount;
        s.disk_used_pct = used;

        // Per-process: CPU is the delta of each process's accumulated CPU time between two
        // consecutive samples, relative to wall-clock time x logical cores (so 100 % means every
        // core fully busy, like Task Manager); RAM is the resident set size.
        let total_ram = s.ram_total_bytes.max(1) as f32;
        let (top_cpu, top_ram) = self.process_leaders();
        s.top_cpu = top_cpu.map(|(n, c)| ProcUse { name: crate::protocol::strip_exe(&n).to_string(), pct: c });
        s.top_ram = top_ram.map(|(n, m)| ProcUse {
            name: crate::protocol::strip_exe(&n).to_string(),
            pct: m as f32 * 100.0 / total_ram,
        });
        s.sources.insert("top_process".into(), "sysinfo (CPU-time delta per tick, RSS)".into());

        // CPU clock.
        let mhz = self.sys.cpus().iter().map(|c| c.frequency()).max().unwrap_or(0) as u32;
        if mhz > 0 {
            s.cpu_clock_mhz = Some(mhz);
            s.sources.insert("cpu_clock".into(), "sysinfo".into());
        } else if let Some(v) = self.clock_fallback.get() {
            s.cpu_clock_mhz = Some(v);
            s.sources.insert("cpu_clock".into(), "WMI Win32_Processor.CurrentClockSpeed".into());
        } else {
            s.sources.insert("cpu_clock".into(), "unavailable".into());
        }

        // Temperature + fan.
        let r = self.sensors.read();
        s.cpu_temp_c = r.cpu_temp.map(|v| v as f32);
        s.sources.insert("cpu_temp".into(), r.temp_source.map_or_else(|| "unavailable".to_string(), str::to_string));
        s.fan_rpm = r.fan_rpm.map(|v| v.round() as u32);
        s.sources.insert(
            "fan_rpm".into(),
            r.fan_source
                .map_or_else(|| "unavailable (needs a LibreHardwareMonitor fan sensor)".to_string(), str::to_string),
        );
        for (i, d) in r.diagnostics.iter().enumerate() {
            s.sources.insert(format!("probe_{}", i + 1), d.clone());
        }

        // GPU.
        s.gpu = self.gpu.read();
        s.sources.insert(
            "gpu".into(),
            match (&s.gpu, self.gpu.last_error()) {
                (Some(_), _) => "NVML".to_string(),
                (None, Some(e)) => format!("not present ({e})"),
                (None, None) => "not present".to_string(),
            },
        );

        // Battery.
        s.battery = power::read();
        s.sources.insert(
            "battery".into(),
            if s.battery.is_some() { "GetSystemPowerStatus / sysfs".into() } else { "no battery".into() },
        );

        if self.first {
            self.first = false;
            log::debug!("first sample collected (CPU load needs a second sample to be meaningful)");
        }
        s
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn cpu_percent_is_relative_to_all_cores() {
        // One core fully busy for 2 s on a 12-core machine = 8.3 % overall.
        let p = cpu_percent(1_000, 3_000, Duration::from_secs(2), 12).unwrap();
        assert!((p - 8.333).abs() < 0.01, "{p}");
        // Everything busy.
        assert_eq!(cpu_percent(0, 4_000, Duration::from_secs(1), 4), Some(100.0));
        // No progress.
        assert_eq!(cpu_percent(500, 500, Duration::from_secs(1), 8), Some(0.0));
    }

    #[test]
    fn cpu_percent_rejects_bad_intervals() {
        assert_eq!(cpu_percent(0, 10, Duration::from_millis(10), 4), None);
        assert_eq!(cpu_percent(10, 5, Duration::from_secs(1), 4), None);
    }

    #[test]
    fn idle_pseudo_processes_are_recognised() {
        assert!(is_idle_name("System Idle Process"));
        assert!(is_idle_name("idle"));
        assert!(!is_idle_name("chrome.exe"));
    }
}