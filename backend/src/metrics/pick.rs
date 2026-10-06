//! Platform-independent sensor selection rules (unit-tested on every OS).

/// One row of a LibreHardwareMonitor `Sensor` WMI query.
#[derive(Debug, Clone, PartialEq)]
pub struct SensorRow {
    pub name: String,
    pub identifier: String,
    pub sensor_type: String,
    pub value: f64,
}

/// Anything outside this window is treated as a bogus reading.
pub fn plausible_temp(c: f64) -> bool {
    c.is_finite() && (5.0..=125.0).contains(&c)
}

/// WMI thermal zone values are tenths of a Kelvin.
pub fn tenths_kelvin_to_c(v: f64) -> f64 {
    v / 10.0 - 273.15
}

/// Highest plausible temperature of a list of tenth-Kelvin readings.
pub fn max_from_tenths_kelvin(vals: impl IntoIterator<Item = f64>) -> Option<f64> {
    vals.into_iter()
        .map(tenths_kelvin_to_c)
        .filter(|c| plausible_temp(*c))
        .fold(None, |acc: Option<f64>, c| Some(acc.map_or(c, |a| a.max(c))))
}

fn is_cpu_ident(id: &str) -> bool {
    id.to_ascii_lowercase().contains("cpu")
}

/// Choose the CPU temperature from LibreHardwareMonitor sensors:
/// "CPU Package", else "Core Max", else the hottest sensor whose identifier mentions a CPU.
pub fn lhm_cpu_temp(rows: &[SensorRow]) -> Option<f64> {
    let temps: Vec<&SensorRow> =
        rows.iter().filter(|r| r.sensor_type.eq_ignore_ascii_case("Temperature") && plausible_temp(r.value)).collect();
    let by_name = |n: &str| temps.iter().find(|r| r.name.eq_ignore_ascii_case(n) && is_cpu_ident(&r.identifier));
    if let Some(r) = by_name("CPU Package").or_else(|| by_name("Core Max")) {
        return Some(r.value);
    }
    temps
        .iter()
        .filter(|r| is_cpu_ident(&r.identifier))
        .map(|r| r.value)
        .fold(None, |acc: Option<f64>, v| Some(acc.map_or(v, |a| a.max(v))))
}

/// Choose a fan speed (RPM) from LibreHardwareMonitor sensors: a CPU fan if named so,
/// otherwise the fastest spinning fan. Zero/negative readings are ignored.
pub fn lhm_fan_rpm(rows: &[SensorRow]) -> Option<f64> {
    let fans: Vec<&SensorRow> = rows
        .iter()
        .filter(|r| r.sensor_type.eq_ignore_ascii_case("Fan") && r.value.is_finite() && r.value > 0.0)
        .collect();
    let max = |it: &mut dyn Iterator<Item = &&SensorRow>| {
        it.map(|r| r.value).fold(None, |a: Option<f64>, v| Some(a.map_or(v, |x| x.max(v))))
    };
    let cpu = max(&mut fans.iter().filter(|r| r.name.to_ascii_lowercase().contains("cpu")));
    cpu.or_else(|| max(&mut fans.iter()))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn row(name: &str, id: &str, ty: &str, v: f64) -> SensorRow {
        SensorRow { name: name.into(), identifier: id.into(), sensor_type: ty.into(), value: v }
    }

    #[test]
    fn kelvin_conversion() {
        // 3231 deci-kelvin = 323.1 K = 49.95 C
        let c = tenths_kelvin_to_c(3231.0);
        assert!((c - 49.95).abs() < 0.01);
        assert_eq!(max_from_tenths_kelvin([2732.0, 3231.0, 3000.0]).map(|c| c.round()), Some(50.0));
        // 0 K and 300 C are bogus
        assert_eq!(max_from_tenths_kelvin([0.0, 5731.0]), None);
    }

    #[test]
    fn lhm_prefers_package_then_core_max() {
        let rows = vec![
            row("Core #1", "/intelcpu/0/temperature/1", "Temperature", 60.0),
            row("Core Max", "/intelcpu/0/temperature/10", "Temperature", 64.0),
            row("CPU Package", "/intelcpu/0/temperature/11", "Temperature", 62.0),
            row("GPU Core", "/gpu-nvidia/0/temperature/0", "Temperature", 80.0),
        ];
        assert_eq!(lhm_cpu_temp(&rows), Some(62.0));
        assert_eq!(lhm_cpu_temp(&rows[..2]), Some(64.0));
        assert_eq!(lhm_cpu_temp(&rows[..1]), Some(60.0));
        assert_eq!(lhm_cpu_temp(&rows[3..]), None);
    }

    #[test]
    fn lhm_ignores_bogus_values() {
        let rows = vec![
            row("CPU Package", "/intelcpu/0/temperature/11", "Temperature", 0.0),
            row("Core #1", "/intelcpu/0/temperature/1", "Temperature", 150.0),
            row("Core #2", "/intelcpu/0/temperature/2", "Temperature", 55.0),
        ];
        assert_eq!(lhm_cpu_temp(&rows), Some(55.0));
    }

    #[test]
    fn lhm_fan() {
        let rows = vec![
            row("Fan #1", "/lpc/it8688e/0/fan/0", "Fan", 2100.0),
            row("GPU Fan", "/gpu-nvidia/0/fan/0", "Fan", 0.0),
            row("CPU Fan", "/lpc/it8688e/0/fan/1", "Fan", 1800.0),
            row("Core Max", "/intelcpu/0/temperature/10", "Temperature", 64.0),
        ];
        assert_eq!(lhm_fan_rpm(&rows), Some(1800.0));
        assert_eq!(lhm_fan_rpm(&rows[..2]), Some(2100.0));
        assert_eq!(lhm_fan_rpm(&rows[3..]), None);
    }
}
