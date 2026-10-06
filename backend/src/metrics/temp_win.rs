//! Windows temperature sources via WMI. Connections are COM objects bound to the creating
//! thread, so the hub must be created and used on the metrics thread.

use super::pick::{self, SensorRow};
use super::SensorReadings;
use std::collections::HashMap;
use std::time::{Duration, Instant};
use wmi::{Variant, WMIConnection};

type Row = HashMap<String, Variant>;

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
enum Src {
    Lhm,
    ThermalZone,
    Acpi,
}

const ORDER: [Src; 3] = [Src::Lhm, Src::ThermalZone, Src::Acpi];
/// How often a lower-priority cached source re-checks the better ones.
const REPROBE: Duration = Duration::from_secs(60);
/// How often to retry when no source worked at all.
const RETRY: Duration = Duration::from_secs(30);
/// Consecutive failures before the cached choice is dropped.
const MAX_FAILS: u32 = 5;

impl Src {
    fn name(self) -> &'static str {
        match self {
            Src::Lhm => "LibreHardwareMonitor WMI (root\\LibreHardwareMonitor)",
            Src::ThermalZone => "WMI Win32_PerfFormattedData_Counters_ThermalZoneInformation",
            Src::Acpi => "WMI MSAcpi_ThermalZoneTemperature (root\\WMI)",
        }
    }

    fn namespace(self) -> &'static str {
        match self {
            Src::Lhm => "ROOT\\LibreHardwareMonitor",
            Src::ThermalZone => "ROOT\\CIMV2",
            Src::Acpi => "ROOT\\WMI",
        }
    }

    fn idx(self) -> usize {
        self as usize
    }
}

#[derive(Default)]
struct Reading {
    temp: Option<f64>,
    fan: Option<f64>,
}

pub struct Imp {
    conns: [Option<WMIConnection>; 3],
    cached: Option<Src>,
    fails: u32,
    next_probe: Instant,
    next_reprobe: Instant,
}

fn variant_f64(v: &Variant) -> Option<f64> {
    Some(match v {
        Variant::I1(x) => f64::from(*x),
        Variant::I2(x) => f64::from(*x),
        Variant::I4(x) => f64::from(*x),
        Variant::I8(x) => *x as f64,
        Variant::UI1(x) => f64::from(*x),
        Variant::UI2(x) => f64::from(*x),
        Variant::UI4(x) => f64::from(*x),
        Variant::UI8(x) => *x as f64,
        Variant::R4(x) => f64::from(*x),
        Variant::R8(x) => *x,
        Variant::String(s) => s.trim().parse().ok()?,
        _ => return None,
    })
}

fn variant_string(v: &Variant) -> String {
    match v {
        Variant::String(s) => s.clone(),
        _ => String::new(),
    }
}

impl Imp {
    pub fn new() -> Self {
        let now = Instant::now();
        Imp { conns: [None, None, None], cached: None, fails: 0, next_probe: now, next_reprobe: now + REPROBE }
    }

    fn query(&mut self, s: Src, wql: &str) -> Result<Vec<Row>, String> {
        if self.conns[s.idx()].is_none() {
            let c =
                WMIConnection::with_namespace_path(s.namespace()).map_err(|e| format!("cannot open namespace: {e}"))?;
            self.conns[s.idx()] = Some(c);
        }
        let res = match self.conns[s.idx()].as_ref() {
            Some(c) => c.raw_query::<Row>(wql),
            None => return Err("no connection".into()),
        };
        res.map_err(|e| {
            // Drop the connection so the next attempt starts clean.
            self.conns[s.idx()] = None;
            format!("query failed: {e}")
        })
    }

    fn try_source(&mut self, s: Src) -> Result<Reading, String> {
        match s {
            Src::Lhm => {
                let rows = self.query(
                    s,
                    "SELECT Name, Identifier, SensorType, Value FROM Sensor WHERE SensorType = 'Temperature' OR SensorType = 'Fan'",
                )?;
                let sensors: Vec<SensorRow> = rows
                    .iter()
                    .filter_map(|r| {
                        Some(SensorRow {
                            name: variant_string(r.get("Name")?),
                            identifier: variant_string(r.get("Identifier")?),
                            sensor_type: variant_string(r.get("SensorType")?),
                            value: variant_f64(r.get("Value")?)?,
                        })
                    })
                    .collect();
                if sensors.is_empty() {
                    return Err("no sensors published (is LibreHardwareMonitor running with WMI enabled?)".into());
                }
                let temp = pick::lhm_cpu_temp(&sensors);
                if temp.is_none() {
                    return Err(format!("{} sensors but no usable CPU temperature", sensors.len()));
                }
                Ok(Reading { temp, fan: pick::lhm_fan_rpm(&sensors) })
            }
            Src::ThermalZone => {
                let rows = self.query(
                    s,
                    "SELECT Name, HighPrecisionTemperature, Temperature FROM Win32_PerfFormattedData_Counters_ThermalZoneInformation",
                )?;
                let vals = rows.iter().filter_map(|r| {
                    r.get("HighPrecisionTemperature")
                        .and_then(variant_f64)
                        .filter(|v| *v > 0.0)
                        .or_else(|| r.get("Temperature").and_then(variant_f64).map(|k| k * 10.0))
                });
                let temp = pick::max_from_tenths_kelvin(vals);
                if temp.is_none() {
                    return Err("no plausible thermal zone value".into());
                }
                Ok(Reading { temp, fan: None })
            }
            Src::Acpi => {
                let rows = self.query(s, "SELECT CurrentTemperature FROM MSAcpi_ThermalZoneTemperature")?;
                let temp = pick::max_from_tenths_kelvin(
                    rows.iter().filter_map(|r| r.get("CurrentTemperature").and_then(variant_f64)),
                );
                if temp.is_none() {
                    return Err("no plausible value (needs administrator rights on most PCs)".into());
                }
                Ok(Reading { temp, fan: None })
            }
        }
    }

    pub fn read(&mut self) -> SensorReadings {
        let now = Instant::now();
        let mut out = SensorReadings::default();
        let mut chosen: Option<(Src, Reading)> = None;

        if let Some(cur) = self.cached {
            // A lower-priority source periodically checks whether a better one came alive
            // (e.g. LibreHardwareMonitor was started after us).
            if cur != Src::Lhm && now >= self.next_reprobe {
                self.next_reprobe = now + REPROBE;
                for s in ORDER {
                    if s == cur {
                        break;
                    }
                    match self.try_source(s) {
                        Ok(r) => {
                            chosen = Some((s, r));
                            break;
                        }
                        Err(e) => log::debug!("re-probe {}: {e}", s.name()),
                    }
                }
            }
            if chosen.is_none() {
                match self.try_source(cur) {
                    Ok(r) => {
                        self.fails = 0;
                        chosen = Some((cur, r));
                    }
                    Err(e) => {
                        self.fails += 1;
                        log::debug!("{} failed ({}/{MAX_FAILS}): {e}", cur.name(), self.fails);
                        if self.fails >= MAX_FAILS {
                            log::info!("temperature source {} lost; probing again", cur.name());
                            self.cached = None;
                            self.next_probe = now;
                        }
                    }
                }
            }
        } else if now >= self.next_probe {
            for s in ORDER {
                match self.try_source(s) {
                    Ok(r) => {
                        out.diagnostics.push(format!("{}: ok", s.name()));
                        chosen = Some((s, r));
                        break;
                    }
                    Err(e) => out.diagnostics.push(format!("{}: {e}", s.name())),
                }
            }
            if chosen.is_none() {
                self.next_probe = now + RETRY;
                log::warn!("no CPU temperature source available: {}", out.diagnostics.join("; "));
            }
        }

        if let Some((s, r)) = chosen {
            if self.cached != Some(s) {
                log::info!("CPU temperature source: {}", s.name());
            }
            self.cached = Some(s);
            self.fails = 0;
            out.cpu_temp = r.temp;
            out.temp_source = Some(s.name());
            if s == Src::Lhm {
                out.fan_rpm = r.fan;
                out.fan_source = r.fan.map(|_| "LibreHardwareMonitor WMI fan sensor");
            }
        }
        out
    }
}
