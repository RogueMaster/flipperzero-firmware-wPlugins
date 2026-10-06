//! NVIDIA GPU metrics via NVML (nvml.dll / libnvidia-ml.so loaded dynamically at runtime).

use nvml_wrapper::enum_wrappers::device::TemperatureSensor;
use nvml_wrapper::Nvml;
use std::time::{Duration, Instant};

#[derive(Debug, Clone, Default, serde::Serialize)]
pub struct GpuInfo {
    pub name: String,
    pub load_pct: Option<u32>,
    pub temp_c: Option<u32>,
    pub vram_used_bytes: Option<u64>,
    pub vram_total_bytes: Option<u64>,
    /// NVML reports a percentage, not RPM; informational only, never sent as fan RPM.
    pub fan_pct: Option<u32>,
}

pub struct Gpu {
    nvml: Option<Nvml>,
    next_try: Instant,
    last_error: Option<String>,
}

const RETRY: Duration = Duration::from_secs(30);

fn init_nvml() -> Result<Nvml, nvml_wrapper::error::NvmlError> {
    let first = Nvml::init();
    if first.is_err() && cfg!(windows) {
        // Older drivers ship nvml.dll only in the NVSMI folder.
        let base = std::env::var_os("ProgramW6432")
            .or_else(|| std::env::var_os("ProgramFiles"))
            .unwrap_or_else(|| "C:\\Program Files".into());
        let p = std::path::Path::new(&base).join("NVIDIA Corporation").join("NVSMI").join("nvml.dll");
        if p.exists() {
            if let Ok(n) = Nvml::builder().lib_path(p.as_os_str()).init() {
                return Ok(n);
            }
        }
    }
    first
}

impl Gpu {
    pub fn new() -> Self {
        Gpu { nvml: None, next_try: Instant::now(), last_error: None }
    }

    pub fn last_error(&self) -> Option<&str> {
        self.last_error.as_deref()
    }

    /// Current readings, or `None` if there is no NVIDIA GPU / NVML is unavailable.
    pub fn read(&mut self) -> Option<GpuInfo> {
        if self.nvml.is_none() {
            if Instant::now() < self.next_try {
                return None;
            }
            match init_nvml() {
                Ok(n) => {
                    log::info!("NVML initialised");
                    self.nvml = Some(n);
                    self.last_error = None;
                }
                Err(e) => {
                    if self.last_error.is_none() {
                        log::info!("NVML unavailable ({e}); GPU marked not present");
                    }
                    self.last_error = Some(e.to_string());
                    self.next_try = Instant::now() + RETRY;
                    return None;
                }
            }
        }
        let nvml = self.nvml.as_ref()?;
        let dev = match nvml.device_by_index(0) {
            Ok(d) => d,
            Err(e) => {
                log::warn!("NVML device 0: {e}");
                self.last_error = Some(e.to_string());
                self.nvml = None;
                self.next_try = Instant::now() + RETRY;
                return None;
            }
        };
        let mem = dev.memory_info().ok();
        Some(GpuInfo {
            name: dev.name().unwrap_or_else(|_| "NVIDIA GPU".into()),
            load_pct: dev.utilization_rates().ok().map(|u| u.gpu),
            temp_c: dev.temperature(TemperatureSensor::Gpu).ok(),
            vram_used_bytes: mem.as_ref().map(|m| m.used),
            vram_total_bytes: mem.as_ref().map(|m| m.total),
            fan_pct: dev.fan_speed(0).ok(),
        })
    }
}
