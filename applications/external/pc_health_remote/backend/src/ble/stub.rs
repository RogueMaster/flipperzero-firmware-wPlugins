//! BLE is not implemented on this platform.

use super::Found;
use crate::shared::Shared;
use anyhow::{bail, Result};
use std::sync::Arc;
use std::thread::JoinHandle;
use std::time::Duration;

pub fn spawn(_shared: Arc<Shared>) -> Option<JoinHandle<()>> {
    log::info!("BLE is not supported on this platform; USB only");
    None
}

pub fn scan(_d: Duration) -> Result<Vec<Found>> {
    bail!("BLE is not supported on this platform")
}

pub fn pair_device(_addr: u64, _force: bool) -> Result<bool> {
    bail!("BLE is not supported on this platform")
}

pub fn unpair_device(_addr: u64) -> Result<()> {
    bail!("BLE is not supported on this platform")
}

pub fn paired_state(_addr: u64) -> Option<bool> {
    None
}
