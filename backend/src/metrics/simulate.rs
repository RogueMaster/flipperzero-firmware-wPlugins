//! Synthetic, continuously changing values for testing alerts on the Flipper.

use super::gpu::GpuInfo;
use super::power::Battery;
use super::{ProcUse, Snapshot};
use std::collections::BTreeMap;

const GIB: u64 = 1 << 30;

/// Triangle wave in `lo..=hi` with the given period (seconds).
fn tri(t: f64, period: f64, lo: f64, hi: f64) -> f64 {
    let x = (t / period).fract();
    let k = if x < 0.5 { x * 2.0 } else { (1.0 - x) * 2.0 };
    lo + (hi - lo) * k
}

/// Snapshot `t` seconds after start. Everything sweeps through its alert range:
/// CPU/GPU temps 40..100 C, load 5..100 %, RAM 40..98 %, battery drains and recharges,
/// AC/battery toggles every minute.
pub fn snapshot(t: f64) -> Snapshot {
    const NAMES: [&str; 5] = ["chrome", "Code", "firefox", "obs64", "SomeLongGame"];
    let idx = (t / 15.0) as usize % NAMES.len();
    let ram_total = 16 * GIB;
    let vram_total = 6 * GIB;
    let on_battery = (t / 60.0) as u64 % 2 == 1;
    let mut sources = BTreeMap::new();
    sources.insert("all".to_string(), "simulated".to_string());
    Snapshot {
        cpu_load_pct: tri(t, 40.0, 5.0, 100.0) as f32,
        cpu_temp_c: Some(tri(t, 60.0, 40.0, 100.0) as f32),
        cpu_clock_mhz: Some(tri(t, 20.0, 800.0, 4500.0) as u32),
        ram_total_bytes: ram_total,
        ram_used_bytes: (tri(t, 90.0, 0.40, 0.98) * ram_total as f64) as u64,
        disk_mount: "C:\\".into(),
        disk_used_pct: 71.0,
        uptime_s: 3600 * 50 + t as u64,
        top_cpu: Some(ProcUse { name: NAMES[idx].into(), pct: tri(t, 25.0, 1.0, 60.0) as f32 }),
        top_ram: Some(ProcUse { name: NAMES[(idx + 1) % NAMES.len()].into(), pct: tri(t, 35.0, 2.0, 30.0) as f32 }),
        gpu: Some(GpuInfo {
            name: "Simulated GPU".into(),
            load_pct: Some(tri(t, 30.0, 0.0, 100.0) as u32),
            temp_c: Some(tri(t, 50.0, 40.0, 100.0) as u32),
            vram_used_bytes: Some((tri(t, 70.0, 0.1, 0.99) * vram_total as f64) as u64),
            vram_total_bytes: Some(vram_total),
            fan_pct: Some(50),
        }),
        fan_rpm: Some(tri(t, 45.0, 1200.0, 5200.0) as u32),
        battery: Some(Battery { percent: tri(t, 300.0, 3.0, 100.0) as u8, on_battery, charging: !on_battery }),
        sources,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn values_stay_in_range_and_change() {
        let a = snapshot(0.0).to_telemetry();
        let mut changed = false;
        for i in 0..600 {
            let t = snapshot(f64::from(i)).to_telemetry();
            assert!(t.cpu_load <= 100 && t.gpu_load <= 100 && t.ram_load <= 100 && t.vram_load <= 100);
            assert!(t.cpu_temp_valid && t.gpu_temp_valid && t.fan_valid && t.gpu_present);
            changed |= t != a;
        }
        assert!(changed);
        // The sweep must reach the typical alert range.
        let max_temp = (0..600).map(|i| snapshot(f64::from(i)).to_telemetry().cpu_temp).max().unwrap();
        assert!(max_temp >= 95);
    }
}
