//! Linux BLE via BlueZ (btleplug). Best effort: the bond must be created with `bluetoothctl`
//! (`pair`, `trust`), after which reconnection is automatic.

use super::{Found, RX_CHAR_UUID, SERIAL_SERVICE_UUID, TX_CHAR_UUID};
use crate::config::format_address;
use crate::session::{self, Link, SessionEnd};
use crate::shared::{LinkKind, Shared};
use anyhow::{anyhow, bail, Context, Result};
use btleplug::api::{Central, CharPropFlags, Manager as _, Peripheral as _, ScanFilter, WriteType};
use btleplug::platform::{Adapter, Manager, Peripheral};
use futures::{Stream, StreamExt};
use std::pin::Pin;
use std::sync::Arc;
use std::thread::JoinHandle;
use std::time::{Duration, Instant};
use tokio::runtime::{Builder, Runtime};
use uuid::Uuid;

const HELLO_TIMEOUT: Duration = Duration::from_secs(10);
const BACKOFF_MIN: Duration = Duration::from_secs(2);
const BACKOFF_MAX: Duration = Duration::from_secs(30);
const HEALTHY_AFTER: Duration = Duration::from_secs(15);
const FIND_TIMEOUT: Duration = Duration::from_secs(12);

type Notifications = Pin<Box<dyn Stream<Item = btleplug::api::ValueNotification> + Send>>;

fn runtime() -> Result<Runtime> {
    Builder::new_current_thread().enable_all().build().context("creating async runtime")
}

async fn adapter() -> Result<Adapter> {
    let manager = Manager::new().await.context("connecting to BlueZ (is bluetoothd running?)")?;
    manager.adapters().await?.into_iter().next().ok_or_else(|| anyhow!("no Bluetooth adapter found"))
}

pub fn scan(duration: Duration) -> Result<Vec<Found>> {
    let rt = runtime()?;
    rt.block_on(async {
        let adapter = adapter().await?;
        adapter.start_scan(ScanFilter::default()).await?;
        tokio::time::sleep(duration).await;
        let mut out = Vec::new();
        for p in adapter.peripherals().await? {
            if let Some(props) = p.properties().await? {
                out.push(Found {
                    address: u64::from(props.address),
                    name: props.local_name.or(props.advertisement_name).unwrap_or_default(),
                    rssi: props.rssi.unwrap_or(-127),
                });
            }
        }
        let _ = adapter.stop_scan().await;
        Ok(out)
    })
}

pub fn pair_device(addr: u64, _force: bool) -> Result<bool> {
    bail!(
        "programmatic pairing is only implemented on Windows. On Linux run:\n  bluetoothctl\n  scan on\n  pair {0}\n  trust {0}\nthen put last_device = \"{0}\" into config.toml",
        format_address(addr)
    )
}

pub fn unpair_device(addr: u64) -> Result<()> {
    bail!("on Linux remove the bond with: bluetoothctl remove {}", format_address(addr))
}

pub fn paired_state(_addr: u64) -> Option<bool> {
    None
}

struct BleLink<'a> {
    rt: &'a Runtime,
    periph: Peripheral,
    rx: btleplug::api::Characteristic,
    write_type: WriteType,
    notifications: Notifications,
}

impl Link for BleLink<'_> {
    fn recv(&mut self, timeout: Duration) -> Result<Vec<u8>> {
        let next = self.rt.block_on(tokio::time::timeout(timeout, self.notifications.next()));
        match next {
            Ok(Some(n)) if n.uuid == Uuid::from_u128(TX_CHAR_UUID) => Ok(n.value),
            Ok(Some(_)) => Ok(Vec::new()),
            Ok(None) => bail!("BLE notification stream closed"),
            Err(_) => {
                // Idle: make sure the link is still up.
                if self.rt.block_on(self.periph.is_connected())? {
                    Ok(Vec::new())
                } else {
                    bail!("BLE link disconnected")
                }
            }
        }
    }

    fn send(&mut self, frame: &[u8]) -> Result<()> {
        self.rt.block_on(self.periph.write(&self.rx, frame, self.write_type)).context("BLE write failed")
    }
}

async fn find_peripheral(adapter: &Adapter, addr: u64) -> Result<Option<Peripheral>> {
    adapter.start_scan(ScanFilter::default()).await?;
    let t0 = Instant::now();
    let mut found = None;
    while found.is_none() && t0.elapsed() < FIND_TIMEOUT {
        for p in adapter.peripherals().await? {
            if u64::from(p.address()) == addr {
                found = Some(p);
                break;
            }
        }
        if found.is_none() {
            tokio::time::sleep(Duration::from_millis(500)).await;
        }
    }
    let _ = adapter.stop_scan().await;
    Ok(found)
}

async fn connect(
    adapter: &Adapter,
    addr: u64,
) -> Result<Option<(Peripheral, btleplug::api::Characteristic, Notifications, String)>> {
    let Some(p) = find_peripheral(adapter, addr).await? else { return Ok(None) };
    if !p.is_connected().await? {
        p.connect().await.context("connect")?;
    }
    p.discover_services().await.context("service discovery")?;
    let (svc, rx_id, tx_id) =
        (Uuid::from_u128(SERIAL_SERVICE_UUID), Uuid::from_u128(RX_CHAR_UUID), Uuid::from_u128(TX_CHAR_UUID));
    let chars = p.characteristics();
    let find = |id: Uuid| chars.iter().find(|c| c.uuid == id && c.service_uuid == svc).cloned();
    let rx = find(rx_id).ok_or_else(|| anyhow!("RX characteristic not found"))?;
    let tx = find(tx_id).ok_or_else(|| anyhow!("TX characteristic not found"))?;
    let notifications = p.notifications().await?;
    p.subscribe(&tx).await.context("subscribe to TX")?;
    let name = p.properties().await?.and_then(|pr| pr.local_name).unwrap_or_else(|| format_address(addr));
    Ok(Some((p, rx, notifications, name)))
}

pub fn spawn(shared: Arc<Shared>) -> Option<JoinHandle<()>> {
    Some(
        std::thread::Builder::new()
            .name("ble".into())
            .spawn(move || match runtime() {
                Ok(rt) => worker(&shared, &rt),
                Err(e) => log::error!("BLE disabled: {e:#}"),
            })
            .expect("spawn ble thread"),
    )
}

fn worker(shared: &Shared, rt: &Runtime) {
    let mut backoff = BACKOFF_MIN;
    while !shared.stopping() {
        let Some(addr) = shared.ble_address().filter(|_| shared.transport_allows(LinkKind::Ble) && !shared.pairing())
        else {
            shared.sleep(Duration::from_secs(1));
            continue;
        };
        if shared.owns(LinkKind::Usb) {
            shared.sleep(Duration::from_secs(1));
            continue;
        }
        let t0 = Instant::now();
        let res: Result<SessionEnd> = (|| {
            let adapter = rt.block_on(adapter())?;
            let Some((periph, rx, notifications, name)) = rt.block_on(connect(&adapter, addr))? else {
                bail!("Flipper not in range");
            };
            let write_type = if rx.properties.contains(CharPropFlags::WRITE_WITHOUT_RESPONSE) {
                WriteType::WithoutResponse
            } else {
                WriteType::WithResponse
            };
            let mut link = BleLink { rt, periph: periph.clone(), rx, write_type, notifications };
            let end = session::run(&mut link, LinkKind::Ble, &name, shared, HELLO_TIMEOUT);
            let _ = rt.block_on(periph.disconnect());
            end
        })();
        match res {
            Ok(SessionEnd::Stopped) => break,
            Ok(end) => log::info!("BLE session ended: {end:?}"),
            Err(e) => log::debug!("BLE attempt failed: {e:#}"),
        }
        if t0.elapsed() >= HEALTHY_AFTER {
            backoff = BACKOFF_MIN;
        } else {
            shared.sleep(backoff);
            backoff = (backoff * 2).min(BACKOFF_MAX);
        }
    }
}
