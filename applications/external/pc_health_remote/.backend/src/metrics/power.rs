//! Battery / AC state.

#[derive(Debug, Clone, Copy, PartialEq, Eq, serde::Serialize)]
pub struct Battery {
    pub percent: u8,
    pub on_battery: bool,
    pub charging: bool,
}

/// Interpret a Win32 `SYSTEM_POWER_STATUS`.
/// `ac`: 0 offline, 1 online, 255 unknown. `flag`: bit3 charging, bit7 no battery, 255 unknown.
/// `percent`: 0..=100 or 255 unknown.
pub fn interpret_win32(ac: u8, flag: u8, percent: u8) -> Option<Battery> {
    let no_battery = flag != 255 && flag & 128 != 0;
    if no_battery || percent > 100 {
        return None;
    }
    Some(Battery { percent, on_battery: ac == 0, charging: flag != 255 && flag & 8 != 0 })
}

#[cfg(windows)]
pub fn read() -> Option<Battery> {
    use windows::Win32::System::Power::{GetSystemPowerStatus, SYSTEM_POWER_STATUS};
    let mut s = SYSTEM_POWER_STATUS::default();
    // SAFETY: `s` is a valid, writable SYSTEM_POWER_STATUS.
    unsafe { GetSystemPowerStatus(&mut s) }.ok()?;
    interpret_win32(s.ACLineStatus, s.BatteryFlag, s.BatteryLifePercent)
}

#[cfg(all(target_os = "linux", not(windows)))]
pub fn read() -> Option<Battery> {
    let base = std::path::Path::new("/sys/class/power_supply");
    let mut bat: Option<(u8, String)> = None;
    let mut ac_online: Option<bool> = None;
    for e in std::fs::read_dir(base).ok()?.flatten() {
        let p = e.path();
        let kind = std::fs::read_to_string(p.join("type")).unwrap_or_default();
        match kind.trim() {
            "Battery" if bat.is_none() => {
                let cap: u8 = std::fs::read_to_string(p.join("capacity")).ok()?.trim().parse().ok()?;
                let st = std::fs::read_to_string(p.join("status")).unwrap_or_default();
                bat = Some((cap.min(100), st.trim().to_string()));
            }
            "Mains" | "USB" => {
                if let Ok(v) = std::fs::read_to_string(p.join("online")) {
                    ac_online = Some(ac_online.unwrap_or(false) || v.trim() == "1");
                }
            }
            _ => {}
        }
    }
    let (percent, status) = bat?;
    let charging = status.eq_ignore_ascii_case("Charging");
    let on_battery = match ac_online {
        Some(ac) => !ac,
        None => status.eq_ignore_ascii_case("Discharging"),
    };
    Some(Battery { percent, on_battery, charging })
}

#[cfg(not(any(windows, target_os = "linux")))]
pub fn read() -> Option<Battery> {
    None
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn win32_status() {
        // Plugged in and charging at 80 %.
        assert_eq!(interpret_win32(1, 8, 80), Some(Battery { percent: 80, on_battery: false, charging: true }));
        // On battery, 42 %.
        assert_eq!(interpret_win32(0, 0, 42), Some(Battery { percent: 42, on_battery: true, charging: false }));
        // Desktop without a battery.
        assert_eq!(interpret_win32(1, 128, 255), None);
        // Unknown charge.
        assert_eq!(interpret_win32(1, 255, 255), None);
        // Plugged in, charge limit reached: not charging, not on battery.
        assert_eq!(interpret_win32(1, 0, 80), Some(Battery { percent: 80, on_battery: false, charging: false }));
    }
}
