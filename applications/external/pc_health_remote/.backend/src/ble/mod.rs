//! Bluetooth LE transport: Windows via WinRT, Linux via BlueZ (btleplug, best effort).

use anyhow::{bail, Result};
use std::time::Duration;

#[cfg(windows)]
#[path = "win.rs"]
mod imp;

#[cfg(all(target_os = "linux", not(windows)))]
#[path = "linux.rs"]
mod imp;

#[cfg(not(any(windows, target_os = "linux")))]
#[path = "stub.rs"]
mod imp;

pub use imp::spawn;

/// Advertised local name prefix of the PC Health Remote Flipper app.
pub const NAME_PREFIX: &str = "PCHealth";

// Flipper serial GATT service (flipperdevices/flipperzero-firmware,
// targets/f7/ble_glue/services/serial_service_uuid.inc + serial_service.c).
// The .inc file stores the UUIDs as little-endian byte arrays; these are the canonical forms.
/// Service 8fe5b3d5-2e7f-4a98-2a48-7acc60fe0000
pub const SERIAL_SERVICE_UUID: u128 = 0x8fe5_b3d5_2e7f_4a98_2a48_7acc_60fe_0000;
/// "RX" (from the Flipper's point of view): the characteristic the PC WRITES to.
/// 19ed82ae-ed21-4c9d-4145-228e62fe0000 (write, write without response)
pub const RX_CHAR_UUID: u128 = 0x19ed_82ae_ed21_4c9d_4145_228e_62fe_0000;
/// "TX": the Flipper's output; the PC subscribes. The firmware declares it INDICATE (not NOTIFY).
/// 19ed82ae-ed21-4c9d-4145-228e61fe0000
pub const TX_CHAR_UUID: u128 = 0x19ed_82ae_ed21_4c9d_4145_228e_61fe_0000;

/// A Flipper seen while scanning.
#[derive(Debug, Clone)]
pub struct Found {
    pub address: u64,
    pub name: String,
    pub rssi: i16,
}

pub fn is_target_name(name: &str) -> bool {
    name.starts_with(NAME_PREFIX)
}

/// A `PCHealth*` device Windows already has a bond with.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct PairedDevice {
    pub address: u64,
    pub name: String,
    /// Currently connected to this PC (and therefore not advertising).
    pub connected: bool,
}

/// Already-paired `PCHealth*` devices, in the order Windows lists them.
#[cfg(windows)]
fn paired_targets() -> Result<Vec<PairedDevice>> {
    imp::paired_targets()
}

#[cfg(not(windows))]
fn paired_targets() -> Result<Vec<PairedDevice>> {
    Ok(Vec::new())
}

/// The one to use when several are paired: a connected one wins (it is the device in use right
/// now), otherwise the most recently added one (Windows lists devices in pairing order).
fn pick_paired(list: &[PairedDevice]) -> Option<&PairedDevice> {
    list.iter().rev().find(|p| p.connected).or_else(|| list.last())
}

#[derive(Debug, Clone)]
pub struct PairOutcome {
    pub address: u64,
    pub name: String,
    pub already_paired: bool,
}

/// Scan for `PCHealth*` devices. Returns them strongest first.
pub fn scan_targets(duration: Duration) -> Result<Vec<Found>> {
    let mut v: Vec<Found> = imp::scan(duration)?.into_iter().filter(|f| is_target_name(&f.name)).collect();
    v.sort_by_key(|f| std::cmp::Reverse(f.rssi));
    Ok(v)
}

/// Find (or use the given) device and pair with it programmatically, without any Windows UI.
pub fn pair(address: Option<u64>, force: bool, scan_for: Duration) -> Result<PairOutcome> {
    let (addr, name) = match address {
        Some(a) => {
            // Let the OS see the device first; a never-seen address cannot be opened.
            let name = imp::scan(scan_for)?.into_iter().find(|f| f.address == a).map(|f| f.name).unwrap_or_default();
            (a, name)
        }
        None => {
            let found = scan_targets(scan_for)?;
            match found.first() {
                Some(f) => {
                    log::info!(
                        "found {} device(s): {}",
                        found.len(),
                        found.iter().map(|f| format!("{} {} ({} dBm)", f.name, crate::config::format_address(f.address), f.rssi)).collect::<Vec<_>>().join(", ")
                    );
                    (f.address, f.name.clone())
                }
                None => {
                    // No advertisement: the Flipper may already be connected to Windows (a
                    // connected peripheral stops advertising). Look at the paired devices.
                    let known = paired_targets().unwrap_or_else(|e| {
                        log::warn!("cannot list paired BLE devices: {e:#}");
                        Vec::new()
                    });
                    match pick_paired(&known) {
                        Some(p) => {
                            log::info!(
                                "no advertisement seen; using already-paired {} {}{}",
                                p.name,
                                crate::config::format_address(p.address),
                                if p.connected { " (currently connected)" } else { "" }
                            );
                            (p.address, p.name.clone())
                        }
                        None => bail!("no BLE device named \"{NAME_PREFIX}*\" found (neither advertising nor already paired). Open the PC Health Remote app on the Flipper and keep it close."),
                    }
                }
            }
        }
    };
    let already_paired = imp::pair_device(addr, force)?;
    Ok(PairOutcome { address: addr, name, already_paired })
}

/// Whether Windows considers the device paired (`None` if unknown / unsupported).
pub fn paired_state(address: u64) -> Option<bool> {
    imp::paired_state(address)
}

/// Remove the Windows pairing of the given device.
pub fn unpair(address: u64) -> Result<()> {
    imp::unpair_device(address)
}

#[cfg(test)]
mod tests {
    use super::*;

    /// The firmware .inc file lists UUIDs as little-endian byte arrays.
    fn from_le(b: [u8; 16]) -> u128 {
        u128::from_le_bytes(b)
    }

    #[test]
    fn uuids_match_firmware_byte_arrays() {
        assert_eq!(
            from_le([0x00, 0x00, 0xfe, 0x60, 0xcc, 0x7a, 0x48, 0x2a, 0x98, 0x4a, 0x7f, 0x2e, 0xd5, 0xb3, 0xe5, 0x8f]),
            SERIAL_SERVICE_UUID
        );
        assert_eq!(
            from_le([0x00, 0x00, 0xfe, 0x61, 0x8e, 0x22, 0x45, 0x41, 0x9d, 0x4c, 0x21, 0xed, 0xae, 0x82, 0xed, 0x19]),
            TX_CHAR_UUID
        );
        assert_eq!(
            from_le([0x00, 0x00, 0xfe, 0x62, 0x8e, 0x22, 0x45, 0x41, 0x9d, 0x4c, 0x21, 0xed, 0xae, 0x82, 0xed, 0x19]),
            RX_CHAR_UUID
        );
    }

    #[test]
    fn uuid_text_forms() {
        let s = |u: u128| {
            let h = format!("{u:032x}");
            format!("{}-{}-{}-{}-{}", &h[..8], &h[8..12], &h[12..16], &h[16..20], &h[20..])
        };
        assert_eq!(s(SERIAL_SERVICE_UUID), "8fe5b3d5-2e7f-4a98-2a48-7acc60fe0000");
        assert_eq!(s(TX_CHAR_UUID), "19ed82ae-ed21-4c9d-4145-228e61fe0000");
        assert_eq!(s(RX_CHAR_UUID), "19ed82ae-ed21-4c9d-4145-228e62fe0000");
    }

    #[test]
    fn paired_fallback_prefers_connected_then_newest() {
        let d = |a: u64, connected: bool| PairedDevice { address: a, name: format!("PCHealth {a}"), connected };
        assert_eq!(pick_paired(&[]), None);
        assert_eq!(pick_paired(&[d(1, false), d(2, false)]).map(|p| p.address), Some(2));
        assert_eq!(pick_paired(&[d(1, true), d(2, false)]).map(|p| p.address), Some(1));
        assert_eq!(pick_paired(&[d(1, true), d(2, true), d(3, false)]).map(|p| p.address), Some(2));
    }

    #[test]
    fn name_filter() {
        assert!(is_target_name("PCHealth 1A2B"));
        assert!(!is_target_name("Flipper Abcd"));
    }
}
