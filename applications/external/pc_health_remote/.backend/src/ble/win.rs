//! Windows BLE via WinRT. Nothing here ever shows UI: pairing is done programmatically with
//! `DeviceInformationCustomPairing`, and reconnection is a silent GATT connection to a device
//! that is already bonded.

use super::{Found, RX_CHAR_UUID, SERIAL_SERVICE_UUID, TX_CHAR_UUID};
use crate::config::format_address;
use crate::session::{self, Link, SessionEnd};
use crate::shared::{LinkKind, Shared};
use anyhow::{bail, Context, Result};
use std::collections::HashMap;
use std::sync::mpsc::{self, Receiver, RecvTimeoutError, Sender};
use std::sync::{Arc, Mutex};
use std::thread::JoinHandle;
use std::time::{Duration, Instant};
use windows::core::{IInspectable, Interface, Ref, RuntimeType, GUID};
use windows::Devices::Bluetooth::Advertisement::{
    BluetoothLEAdvertisementReceivedEventArgs, BluetoothLEAdvertisementWatcher, BluetoothLEScanningMode,
};
use windows::Devices::Bluetooth::GenericAttributeProfile::{
    GattCharacteristic, GattCharacteristicProperties, GattClientCharacteristicConfigurationDescriptorValue,
    GattCommunicationStatus, GattDeviceService, GattSession, GattValueChangedEventArgs, GattWriteOption,
};
use windows::Devices::Bluetooth::{BluetoothCacheMode, BluetoothConnectionStatus, BluetoothLEDevice};
use windows::Devices::Enumeration::{
    DeviceInformation, DeviceInformationCustomPairing, DevicePairingKinds, DevicePairingProtectionLevel,
    DevicePairingRequestedEventArgs, DevicePairingResultStatus, DeviceUnpairingResultStatus,
};
use windows::Foundation::{IClosable, TypedEventHandler};
use windows_future::{AsyncStatus, IAsyncOperation};
use windows::Storage::Streams::{DataReader, DataWriter};
use windows::Win32::System::Com::{CoInitializeEx, COINIT_MULTITHREADED};

const HELLO_TIMEOUT: Duration = Duration::from_secs(10);
/// A session that lasted at least this long counts as healthy and resets the failure count.
const HEALTHY_AFTER: Duration = Duration::from_secs(15);
/// While the link is down, try a direct connection to the saved address this often even
/// without an advertisement sighting (a bonded device can be connectable without us seeing it
/// advertise): quickly for the first few failures, then every few seconds. A sighting or an OS
/// "connected" event triggers an attempt immediately.
const FAST_RETRY: Duration = Duration::from_millis(1500);
const FAST_RETRIES: u32 = 4;
const SLOW_RETRY: Duration = Duration::from_secs(3);
/// Never start two attempts closer together than this.
const MIN_ATTEMPT_GAP: Duration = Duration::from_millis(600);
/// How long service discovery may take when we have no sign the device is there, and when we
/// do (sighting / OS connected).
const DIRECT_DISCOVERY_LIMIT: Duration = Duration::from_millis(2500);
const SIGHTED_DISCOVERY_LIMIT: Duration = Duration::from_secs(8);

fn guid(u: u128) -> GUID {
    GUID::from_u128(u)
}

/// WinRT needs COM on every thread that calls it.
fn init_com() {
    // SAFETY: plain COM initialisation for this thread; S_FALSE (already initialised) is fine.
    let _ = unsafe { CoInitializeEx(None, COINIT_MULTITHREADED) };
}

fn close<T: Interface>(o: &T) {
    if let Ok(c) = o.cast::<IClosable>() {
        let _ = c.Close();
    }
}

fn hstring_lossy(h: &windows::core::HSTRING) -> String {
    h.to_string_lossy()
}

// ---------------------------------------------------------------------------------------------
// Scanning
// ---------------------------------------------------------------------------------------------

/// Passive/active scan for `duration`; returns every device seen with the best name and RSSI.
pub fn scan(duration: Duration) -> Result<Vec<Found>> {
    init_com();
    let seen: Arc<Mutex<HashMap<u64, Found>>> = Arc::new(Mutex::new(HashMap::new()));
    let watcher = BluetoothLEAdvertisementWatcher::new().context("creating BLE watcher")?;
    // Active scanning also requests scan responses, where the local name often lives.
    watcher.SetScanningMode(BluetoothLEScanningMode::Active)?;
    let sink = seen.clone();
    let token = watcher.Received(&TypedEventHandler::new(
        move |_w: Ref<BluetoothLEAdvertisementWatcher>, a: Ref<BluetoothLEAdvertisementReceivedEventArgs>| {
            let a = a.ok()?;
            let address = a.BluetoothAddress()?;
            let rssi = a.RawSignalStrengthInDBm()?;
            let name = a.Advertisement()?.LocalName().map(|n| hstring_lossy(&n)).unwrap_or_default();
            let mut g = crate::shared::lock(&sink);
            let e = g.entry(address).or_insert(Found { address, name: String::new(), rssi });
            if !name.is_empty() {
                e.name = name;
            }
            e.rssi = e.rssi.max(rssi);
            Ok(())
        },
    ))?;
    watcher.Start().context("starting BLE scan (is Bluetooth turned on?)")?;
    std::thread::sleep(duration);
    let _ = watcher.Stop();
    let _ = watcher.RemoveReceived(token);
    let out: Vec<Found> = crate::shared::lock(&seen).values().cloned().collect();
    Ok(out)
}

/// Why `Waiter::wait` returned.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Wake {
    /// An advertisement from the saved device was seen.
    Sighting,
    /// Windows reports the device connected (it reconnected a bonded device by itself).
    Connected,
    /// Nothing happened within the wait: time for a direct connection attempt.
    Timer,
    /// Quit, transport switched, pairing started or the address changed.
    Abort,
}

/// Keeps a passive advertisement watcher running and the bonded device "wanted" (a GATT session
/// that maintains the connection), so the OS reconnects as soon as the Flipper is back and we
/// are told immediately, instead of polling with a fresh scan every time.
struct Waiter {
    addr: u64,
    tx: Sender<Wake>,
    rx: Receiver<Wake>,
    watcher: BluetoothLEAdvertisementWatcher,
    adv_token: i64,
    dev: Option<BluetoothLEDevice>,
    status_token: i64,
    session: Option<GattSession>,
}

impl Waiter {
    fn new(addr: u64) -> Result<Waiter> {
        let (tx, rx) = mpsc::channel::<Wake>();
        let watcher = BluetoothLEAdvertisementWatcher::new()?;
        // Passive scanning is enough: the address is in every advertising packet, and it is cheaper.
        watcher.SetScanningMode(BluetoothLEScanningMode::Passive)?;
        let adv_tx = tx.clone();
        let adv_token = watcher.Received(&TypedEventHandler::new(
            move |_w: Ref<BluetoothLEAdvertisementWatcher>, a: Ref<BluetoothLEAdvertisementReceivedEventArgs>| {
                if a.ok()?.BluetoothAddress()? == addr {
                    let _ = adv_tx.send(Wake::Sighting);
                }
                Ok(())
            },
        ))?;
        watcher.Start()?;
        let mut w = Waiter { addr, tx, rx, watcher, adv_token, dev: None, status_token: 0, session: None };
        w.attach_device();
        Ok(w)
    }

    /// Open the bonded device and ask Windows to keep (re)connecting to it.
    fn attach_device(&mut self) {
        if self.dev.is_some() {
            return;
        }
        let Ok(dev) = open_device(self.addr) else { return };
        let tx = self.tx.clone();
        let token = dev.ConnectionStatusChanged(&TypedEventHandler::new(
            move |d: Ref<BluetoothLEDevice>, _a: Ref<IInspectable>| {
                if d.ok()?.ConnectionStatus()? == BluetoothConnectionStatus::Connected {
                    let _ = tx.send(Wake::Connected);
                }
                Ok(())
            },
        ));
        match token {
            Ok(t) => self.status_token = t,
            Err(e) => {
                log::debug!("cannot watch the connection status: {e}");
                close(&dev);
                return;
            }
        }
        self.session = dev
            .BluetoothDeviceId()
            .and_then(|id| GattSession::FromDeviceIdAsync(&id))
            .and_then(|op| op.join())
            .ok()
            .inspect(|s| {
                let _ = s.SetMaintainConnection(true);
            });
        self.dev = Some(dev);
    }

    fn connected(&self) -> bool {
        self.dev.as_ref().and_then(|d| d.ConnectionStatus().ok()) == Some(BluetoothConnectionStatus::Connected)
    }

    /// Block until something worth a connection attempt happens, at most `max`.
    fn wait(&mut self, shared: &Shared, max: Duration) -> Wake {
        self.attach_device();
        while self.rx.try_recv().is_ok() {}
        if self.connected() {
            return Wake::Connected;
        }
        let started = Instant::now();
        loop {
            if shared.stopping()
                || !shared.transport_allows(LinkKind::Ble)
                || shared.pairing()
                || shared.ble_address() != Some(self.addr)
                || shared.owns(LinkKind::Usb)
            {
                return Wake::Abort;
            }
            match self.rx.recv_timeout(Duration::from_millis(100)) {
                Ok(w) => return w,
                Err(RecvTimeoutError::Timeout) => {}
                Err(RecvTimeoutError::Disconnected) => return Wake::Abort,
            }
            if started.elapsed() >= max {
                return Wake::Timer;
            }
        }
    }
}

impl Drop for Waiter {
    fn drop(&mut self) {
        let _ = self.watcher.Stop();
        let _ = self.watcher.RemoveReceived(self.adv_token);
        if let Some(d) = &self.dev {
            let _ = d.RemoveConnectionStatusChanged(self.status_token);
        }
        if let Some(s) = &self.session {
            let _ = s.SetMaintainConnection(false);
            close(s);
        }
        if let Some(d) = &self.dev {
            close(d);
        }
    }
}

/// Outcome of waiting on a WinRT async operation with a time limit.
enum Awaited<T> {
    Done(windows::core::Result<T>),
    TimedOut,
}

/// Poll an async operation instead of blocking on it, so a connection attempt to a device that
/// is not there can be given up (and cancelled) after `limit` or when the app quits.
fn await_op<T: RuntimeType + 'static>(op: &IAsyncOperation<T>, limit: Duration, shared: &Shared) -> Awaited<T> {
    let started = Instant::now();
    loop {
        match op.Status() {
            Ok(AsyncStatus::Started) => {}
            _ => return Awaited::Done(op.GetResults()),
        }
        if started.elapsed() >= limit || shared.stopping() {
            let _ = op.Cancel();
            return Awaited::TimedOut;
        }
        std::thread::sleep(Duration::from_millis(15));
    }
}

// ---------------------------------------------------------------------------------------------
// Pairing (no UI)
// ---------------------------------------------------------------------------------------------

fn open_device(addr: u64) -> Result<BluetoothLEDevice> {
    BluetoothLEDevice::FromBluetoothAddressAsync(addr)?.join().with_context(|| {
        format!("device {} is not known to Windows yet (is the Flipper advertising?)", format_address(addr))
    })
}

fn is_paired(dev: &BluetoothLEDevice) -> Result<bool> {
    Ok(dev.DeviceInformation()?.Pairing()?.IsPaired()?)
}

/// Pair with `addr` using Just Works / confirm-only, accepting every request programmatically.
/// Returns `true` if the device was already paired (and `force` was not set).
pub fn pair_device(addr: u64, force: bool) -> Result<bool> {
    init_com();
    let mut dev = open_device(addr)?;
    if is_paired(&dev)? {
        if !force {
            return Ok(true);
        }
        log::info!("removing the existing pairing first");
        unpair_inner(&dev)?;
        close(&dev);
        dev = open_device(addr)?;
    }
    let pairing = dev.DeviceInformation()?.Pairing()?;
    if !pairing.CanPair()? {
        bail!("Windows reports that this device cannot be paired right now");
    }
    let custom: DeviceInformationCustomPairing = pairing.Custom()?;
    let token = custom.PairingRequested(&TypedEventHandler::new(
        move |_s: Ref<DeviceInformationCustomPairing>, args: Ref<DevicePairingRequestedEventArgs>| {
            let args = args.ok()?;
            let kind = args.PairingKind()?;
            log::info!("pairing request: {kind:?}");
            // Just Works shows up as ConfirmOnly; numeric comparison as ConfirmPinMatch.
            if kind.contains(DevicePairingKinds::ConfirmOnly) || kind.contains(DevicePairingKinds::ConfirmPinMatch) {
                args.Accept()?;
            } else {
                log::warn!("unsupported pairing kind {kind:?}; the Flipper app should use Just Works");
            }
            Ok(())
        },
    ))?;

    let kinds = DevicePairingKinds::ConfirmOnly | DevicePairingKinds::ConfirmPinMatch;
    let levels = [
        DevicePairingProtectionLevel::Encryption,
        DevicePairingProtectionLevel::None,
        DevicePairingProtectionLevel::Default,
    ];
    let mut last = String::from("not attempted");
    let mut paired = false;
    for level in levels {
        match custom.PairWithProtectionLevelAsync(kinds, level).and_then(|op| op.join()) {
            Ok(res) => {
                let st = res.Status()?;
                log::info!("pairing with protection level {level:?}: {st:?}");
                if st == DevicePairingResultStatus::Paired || st == DevicePairingResultStatus::AlreadyPaired {
                    paired = true;
                    break;
                }
                last = format!("{st:?}");
            }
            Err(e) => {
                log::warn!("pairing call failed at level {level:?}: {e}");
                last = e.to_string();
            }
        }
    }
    let _ = custom.RemovePairingRequested(token);
    // The DeviceInformation snapshot held by `dev` is not refreshed after pairing, so
    // trust the pairing result status instead of re-reading IsPaired from it.
    let confirmed = paired;
    close(&dev);
    if !confirmed {
        bail!("pairing failed: {last}. Make sure the Flipper app is open and no other device is connected to it.");
    }
    Ok(false)
}

pub fn paired_state(addr: u64) -> Option<bool> {
    init_com();
    let dev = open_device(addr).ok()?;
    let r = is_paired(&dev).ok();
    close(&dev);
    r
}

/// Paired `PCHealth*` devices known to Windows (they do not advertise while connected to us).
pub fn paired_targets() -> Result<Vec<super::PairedDevice>> {
    init_com();
    let selector = BluetoothLEDevice::GetDeviceSelectorFromPairingState(true)?;
    let infos = DeviceInformation::FindAllAsyncAqsFilter(&selector)?.join()?;
    let mut out = Vec::new();
    for i in 0..infos.Size()? {
        let info = infos.GetAt(i)?;
        let name = hstring_lossy(&info.Name()?);
        if !super::is_target_name(&name) {
            continue;
        }
        let Ok(dev) = BluetoothLEDevice::FromIdAsync(&info.Id()?).and_then(|op| op.join()) else {
            continue;
        };
        let address = dev.BluetoothAddress();
        let connected = dev.ConnectionStatus().is_ok_and(|s| s == BluetoothConnectionStatus::Connected);
        close(&dev);
        out.push(super::PairedDevice { address: address?, name, connected });
    }
    Ok(out)
}

fn unpair_inner(dev: &BluetoothLEDevice) -> Result<()> {
    let st = dev.DeviceInformation()?.Pairing()?.UnpairAsync()?.join()?.Status()?;
    log::info!("unpair: {st:?}");
    match st {
        DeviceUnpairingResultStatus::Unpaired | DeviceUnpairingResultStatus::AlreadyUnpaired => Ok(()),
        other => bail!("unpairing failed: {other:?}"),
    }
}

pub fn unpair_device(addr: u64) -> Result<()> {
    init_com();
    let dev = open_device(addr)?;
    let r = unpair_inner(&dev);
    close(&dev);
    r
}

// ---------------------------------------------------------------------------------------------
// Connection / streaming
// ---------------------------------------------------------------------------------------------

enum Event {
    Data(Vec<u8>),
    Disconnected,
}

struct BleLink {
    events: Receiver<Event>,
    rx_char: GattCharacteristic,
    write_option: GattWriteOption,
    /// Largest payload per ATT write (MTU - 3).
    max_chunk: usize,
}

impl Link for BleLink {
    fn recv(&mut self, timeout: Duration) -> Result<Vec<u8>> {
        match self.events.recv_timeout(timeout) {
            Ok(Event::Data(v)) => Ok(v),
            Ok(Event::Disconnected) => bail!("BLE link disconnected"),
            Err(RecvTimeoutError::Timeout) => Ok(Vec::new()),
            Err(RecvTimeoutError::Disconnected) => bail!("BLE event channel closed"),
        }
    }

    fn send(&mut self, frame: &[u8]) -> Result<()> {
        // A 48-byte frame normally fits one write. If the MTU stayed at the default, the
        // receiver is stream-based, so splitting preserves correctness.
        for chunk in frame.chunks(self.max_chunk.max(20)) {
            let w = DataWriter::new()?;
            w.WriteBytes(chunk)?;
            let buf = w.DetachBuffer()?;
            let res = self.rx_char.WriteValueWithResultAndOptionAsync(&buf, self.write_option)?.join()?;
            let st = res.Status()?;
            if st != GattCommunicationStatus::Success {
                bail!("GATT write failed: {st:?}");
            }
        }
        Ok(())
    }
}

fn find_service(dev: &BluetoothLEDevice, limit: Duration, shared: &Shared) -> Result<GattDeviceService> {
    let mut last = String::new();
    // Uncached first (forces a fresh discovery and connects), then the OS cache.
    for (mode, mode_limit) in
        [(BluetoothCacheMode::Uncached, limit), (BluetoothCacheMode::Cached, Duration::from_secs(2))]
    {
        let op = dev.GetGattServicesForUuidWithCacheModeAsync(guid(SERIAL_SERVICE_UUID), mode)?;
        match await_op(&op, mode_limit, shared) {
            Awaited::Done(Ok(res)) => {
                let st = res.Status()?;
                let services = res.Services()?;
                if st == GattCommunicationStatus::Success && services.Size()? > 0 {
                    return Ok(services.GetAt(0)?);
                }
                last = format!("{mode:?}: status {st:?}, {} service(s)", services.Size().unwrap_or(0));
            }
            Awaited::Done(Err(e)) => last = format!("{mode:?}: {e}"),
            // The device is not answering: give up now; the OS cache would only hand back a
            // stale service that we cannot talk to.
            Awaited::TimedOut => bail!("device not reachable (service discovery timed out after {} ms)", mode_limit.as_millis()),
        }
    }
    bail!("Flipper serial service not found ({last})")
}

fn find_char(svc: &GattDeviceService, uuid: u128, what: &str) -> Result<GattCharacteristic> {
    let mut last = String::new();
    for mode in [BluetoothCacheMode::Uncached, BluetoothCacheMode::Cached] {
        match svc.GetCharacteristicsForUuidWithCacheModeAsync(guid(uuid), mode).and_then(|op| op.join()) {
            Ok(res) => {
                let st = res.Status()?;
                let list = res.Characteristics()?;
                if st == GattCommunicationStatus::Success && list.Size()? > 0 {
                    return Ok(list.GetAt(0)?);
                }
                last = format!("{mode:?}: status {st:?}");
            }
            Err(e) => last = format!("{mode:?}: {e}"),
        }
    }
    bail!("{what} characteristic not found ({last})")
}

/// Connect to the bonded Flipper, stream until the link ends. Returns the session outcome.
fn connect_and_run(
    shared: &Shared,
    addr: u64,
    discovery_limit: Duration,
    lost_at: Option<Instant>,
) -> Result<SessionEnd> {
    let attempt_started = Instant::now();
    let dev = open_device(addr)?;
    if !is_paired(&dev)? {
        close(&dev);
        shared.set_notice("Flipper not paired - use \"Pair new Flipper...\"");
        bail!("device is not paired with Windows; run `pc-health-remote pair`");
    }
    let label = {
        let n = hstring_lossy(&dev.Name().unwrap_or_default());
        if n.is_empty() {
            format_address(addr)
        } else {
            n
        }
    };

    // Keep the link alive for as long as we use it, and let Windows negotiate the MTU.
    let session =
        match dev.BluetoothDeviceId().and_then(|id| GattSession::FromDeviceIdAsync(&id)).and_then(|op| op.join()) {
            Ok(s) => {
                let _ = s.SetMaintainConnection(true);
                Some(s)
            }
            Err(e) => {
                log::debug!("GattSession unavailable: {e}");
                None
            }
        };

    let result = (|| -> Result<SessionEnd> {
        let service = find_service(&dev, discovery_limit, shared)?;
        let rx_char = find_char(&service, RX_CHAR_UUID, "RX (write)")?;
        let tx_char = find_char(&service, TX_CHAR_UUID, "TX (notify)")?;

        let (ev_tx, ev_rx) = mpsc::channel::<Event>();

        // Incoming data (HELLO frames).
        let data_tx: Sender<Event> = ev_tx.clone();
        let value_token = tx_char.ValueChanged(&TypedEventHandler::new(
            move |_c: Ref<GattCharacteristic>, a: Ref<GattValueChangedEventArgs>| {
                let buf = a.ok()?.CharacteristicValue()?;
                let reader = DataReader::FromBuffer(&buf)?;
                let mut bytes = vec![0u8; reader.UnconsumedBufferLength()? as usize];
                reader.ReadBytes(&mut bytes)?;
                let _ = data_tx.send(Event::Data(bytes));
                Ok(())
            },
        ))?;

        // Disconnect notification.
        let status_tx = ev_tx;
        let status_token = dev.ConnectionStatusChanged(&TypedEventHandler::new(
            move |d: Ref<BluetoothLEDevice>, _a: Ref<IInspectable>| {
                if d.ok()?.ConnectionStatus()? == BluetoothConnectionStatus::Disconnected {
                    let _ = status_tx.send(Event::Disconnected);
                }
                Ok(())
            },
        ))?;

        // Subscribe. The firmware declares TX as INDICATE; accept NOTIFY too.
        let props = tx_char.CharacteristicProperties()?;
        let cccd = if props.contains(GattCharacteristicProperties::Indicate) {
            GattClientCharacteristicConfigurationDescriptorValue::Indicate
        } else {
            GattClientCharacteristicConfigurationDescriptorValue::Notify
        };
        let subscribed = (|| -> Result<()> {
            let st = tx_char.WriteClientCharacteristicConfigurationDescriptorAsync(cccd)?.join()?;
            if st != GattCommunicationStatus::Success {
                bail!("subscribing to TX ({cccd:?}) failed: {st:?}");
            }
            Ok(())
        })();

        let outcome = subscribed.and_then(|()| {
            let rx_props = rx_char.CharacteristicProperties()?;
            // Prefer acknowledged writes: at 1 frame/s the cost is negligible, and a write the
            // Flipper rejects (e.g. insufficient authentication) surfaces as an error instead
            // of being silently dropped like a write-without-response.
            let write_option = if rx_props.contains(GattCharacteristicProperties::Write) {
                GattWriteOption::WriteWithResponse
            } else {
                GattWriteOption::WriteWithoutResponse
            };
            let max_chunk =
                session.as_ref().and_then(|s| s.MaxPduSize().ok()).map_or(20, |pdu| usize::from(pdu).saturating_sub(3));
            log::info!(
                "BLE: GATT ready {} ms after the attempt started{}",
                attempt_started.elapsed().as_millis(),
                lost_at.map_or(String::new(), |l| format!(", {} ms after the previous link was lost", l.elapsed().as_millis()))
            );
            log::debug!("BLE ready: {cccd:?}, {write_option:?}, max write {max_chunk} bytes");
            let mut link = BleLink { events: ev_rx, rx_char: rx_char.clone(), write_option, max_chunk };
            session::run(&mut link, LinkKind::Ble, &label, shared, HELLO_TIMEOUT)
        });

        // Tear down: unsubscribe and detach handlers (errors are expected if the link is gone).
        let _ = tx_char.RemoveValueChanged(value_token);
        let _ = dev.RemoveConnectionStatusChanged(status_token);
        // Unsubscribing over a dead link blocks for several seconds, delaying the reconnect.
        let still_connected = dev.ConnectionStatus().is_ok_and(|s| s == BluetoothConnectionStatus::Connected);
        if outcome.is_ok() && still_connected {
            let _ = tx_char
                .WriteClientCharacteristicConfigurationDescriptorAsync(
                    GattClientCharacteristicConfigurationDescriptorValue::None,
                )
                .and_then(|op| op.join());
        }
        close(&service);
        outcome
    })();

    if let Some(s) = &session {
        let _ = s.SetMaintainConnection(false);
        close(s);
    }
    close(&dev);
    result
}

pub fn spawn(shared: Arc<Shared>) -> Option<JoinHandle<()>> {
    Some(
        std::thread::Builder::new()
            .name("ble".into())
            .spawn(move || {
                init_com();
                worker(&shared);
            })
            .expect("spawn ble thread"),
    )
}

fn log_at(level: log::Level, msg: std::fmt::Arguments) {
    log::log!(level, "{msg}");
}

fn worker(shared: &Shared) {
    log::debug!("BLE worker started");
    let mut waiter: Option<Waiter> = None;
    // Consecutive attempts that did not produce a healthy session.
    let mut fails: u32 = 0;
    // When the last healthy link ended (for the reconnect timing log).
    let mut lost_at: Option<Instant> = None;
    let mut last_attempt: Option<Instant> = None;
    while !shared.stopping() {
        let wanted = shared
            .ble_address()
            .filter(|_| shared.transport_allows(LinkKind::Ble) && !shared.pairing() && !shared.owns(LinkKind::Usb));
        let Some(addr) = wanted else {
            // Not wanted (other transport, pairing, USB owns the Flipper): stay quiet.
            waiter = None;
            shared.sleep(Duration::from_millis(500));
            continue;
        };
        if waiter.as_ref().is_none_or(|w| w.addr != addr) {
            waiter = None;
            match Waiter::new(addr) {
                Ok(w) => waiter = Some(w),
                Err(e) => {
                    log::warn!("BLE scan failed: {e:#}");
                    shared.sleep(SLOW_RETRY);
                    continue;
                }
            }
        }
        let Some(w) = waiter.as_mut() else { continue };

        let fast = fails < FAST_RETRIES;
        let gap = if fast { MIN_ATTEMPT_GAP } else { SLOW_RETRY };
        if let Some(t) = last_attempt {
            shared.sleep(gap.saturating_sub(t.elapsed()));
        }
        let waited = Instant::now();
        let wake = w.wait(shared, if fast { FAST_RETRY } else { SLOW_RETRY });
        if wake == Wake::Abort {
            continue;
        }
        let level = if fails < 3 { log::Level::Info } else { log::Level::Debug };
        let limit = if wake == Wake::Timer { DIRECT_DISCOVERY_LIMIT } else { SIGHTED_DISCOVERY_LIMIT };
        log_at(
            level,
            format_args!(
                "BLE: {wake:?} after {} ms; connecting (attempt {}{})",
                waited.elapsed().as_millis(),
                fails + 1,
                lost_at.map_or(String::new(), |l| format!(", {} ms since the link was lost", l.elapsed().as_millis()))
            ),
        );

        let t0 = Instant::now();
        last_attempt = Some(t0);
        let result = connect_and_run(shared, addr, limit, lost_at);
        let ran = t0.elapsed();
        match result {
            Ok(SessionEnd::Stopped) => break,
            Ok(end) => log::info!("BLE session ended after {} ms: {end:?}", ran.as_millis()),
            Err(e) => log_at(level, format_args!("BLE connection attempt failed after {} ms: {e:#}", ran.as_millis())),
        }
        if ran >= HEALTHY_AFTER {
            fails = 0;
            lost_at = Some(Instant::now());
        } else {
            fails = fails.saturating_add(1);
        }
    }
    log::debug!("BLE worker stopped");
}
