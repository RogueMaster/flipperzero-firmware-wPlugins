//! The `run` command: wires the collector, transports and the tray (or a headless loop).

use crate::ble;
use crate::config::{format_address, Config, TransportMode};
use crate::metrics::{simulate, Collector};
use crate::shared::Shared;
use crate::usb;
use anyhow::Result;
use std::sync::atomic::Ordering;
use std::sync::Arc;
use std::thread::JoinHandle;
use std::time::{Duration, Instant};

#[derive(Debug, Clone, Default)]
pub struct RunOpts {
    pub simulate: bool,
    pub no_tray: bool,
    pub transport: Option<TransportMode>,
    pub interval_s: Option<u8>,
}

/// Collect metrics and publish them for the transports.
fn collector_thread(shared: Arc<Shared>) {
    let mut collector = (!shared.simulate).then(Collector::new);
    let t0 = Instant::now();
    // CPU load needs two samples; give sysinfo its minimum interval before the first one.
    shared.sleep(Duration::from_millis(300));
    while !shared.stopping() {
        let started = Instant::now();
        let snap = match collector.as_mut() {
            Some(c) => c.collect(),
            None => simulate::snapshot(t0.elapsed().as_secs_f64()),
        };
        shared.publish(snap.to_telemetry(), snap.summary());
        // Idle (no Flipper) runs slowly: it only feeds the tooltip.
        let period = if shared.link().is_some() { shared.send_period() } else { Duration::from_secs(2) };
        shared.sleep(period.saturating_sub(started.elapsed()));
    }
}

fn spawn_collector(shared: Arc<Shared>) -> JoinHandle<()> {
    std::thread::Builder::new()
        .name("metrics".into())
        .spawn(move || collector_thread(shared))
        .expect("spawn metrics thread")
}

/// Persist the transport choice made from the tray menu.
pub fn set_transport(shared: &Shared, mode: TransportMode) {
    shared.set_transport(mode);
    let mut cfg = Config::load();
    cfg.transport = mode;
    if let Err(e) = cfg.save() {
        log::warn!("cannot save config: {e:#}");
    }
    log::info!("transport set to {}", mode.label());
}

/// Save a newly paired device to the config file and tell the running transports.
pub fn remember_device(shared: Option<&Shared>, address: u64) -> Result<()> {
    let mut cfg = Config::load();
    cfg.last_device = Some(format_address(address));
    cfg.save()?;
    if let Some(s) = shared {
        s.set_ble_address(Some(address));
    }
    Ok(())
}

/// "Pair new Flipper..." from the tray: scan, pair programmatically, remember the device.
pub fn pair_in_background(shared: Arc<Shared>) {
    if !shared.begin_pairing() {
        return;
    }
    shared.set_notice("");
    let spawned = std::thread::Builder::new().name("pair".into()).spawn({
        let shared = shared.clone();
        move || {
            let outcome = ble::pair(None, true, Duration::from_secs(10))
                .and_then(|o| remember_device(Some(&shared), o.address).map(|()| o));
            match outcome {
                Ok(o) => {
                    log::info!("paired with {} ({})", o.name, format_address(o.address));
                    shared.set_notice(&format!(
                        "Paired with {}",
                        if o.name.is_empty() { format_address(o.address) } else { o.name }
                    ));
                }
                Err(e) => {
                    log::error!("pairing failed: {e:#}");
                    shared.set_notice("Pairing failed - see log");
                }
            }
            shared.end_pairing();
        }
    });
    if let Err(e) = spawned {
        log::error!("cannot start pairing thread: {e}");
        shared.end_pairing();
    }
}

fn join_all(handles: Vec<JoinHandle<()>>, timeout: Duration) {
    let deadline = Instant::now() + timeout;
    for h in handles {
        while !h.is_finished() && Instant::now() < deadline {
            std::thread::sleep(Duration::from_millis(25));
        }
        if h.is_finished() {
            let _ = h.join();
        }
    }
}

pub fn run(opts: RunOpts) -> Result<()> {
    #[cfg(windows)]
    let _instance = match crate::winutil::single_instance() {
        Some(g) => g,
        None => {
            log::info!("another instance is already running; exiting");
            eprintln!("PC Health Remote is already running.");
            return Ok(());
        }
    };

    let mut cfg = Config::load();
    if let Some(t) = opts.transport {
        cfg.transport = t;
    }
    if let Some(i) = opts.interval_s {
        cfg.interval_s = i.clamp(1, 10);
    }
    let shared = Arc::new(Shared::new(&cfg, opts.simulate));
    log::info!(
        "starting v{} (transport {}, interval {} s{}{})",
        env!("CARGO_PKG_VERSION"),
        cfg.transport.label(),
        cfg.interval_s,
        if opts.simulate { ", SIMULATED data" } else { "" },
        cfg.last_device.as_deref().map_or(String::new(), |d| format!(", device {d}"))
    );

    {
        let s = shared.clone();
        let _ = ctrlc::set_handler(move || s.stop.store(true, Ordering::Relaxed));
    }

    let mut handles = vec![spawn_collector(shared.clone()), usb::spawn(shared.clone())];
    handles.extend(ble::spawn(shared.clone()));

    #[cfg(windows)]
    if !opts.no_tray {
        let s = shared.clone();
        crate::tray::run(
            shared,
            Box::new(move || {
                s.stop.store(true, Ordering::Relaxed);
                join_all(handles, Duration::from_secs(3));
                log::info!("stopped");
            }),
        );
    }

    // Headless: Linux, or `--no-tray` on Windows. Runs until Ctrl-C.
    let mut last = String::new();
    while !shared.stopping() {
        let line = shared.status_line();
        if line != last {
            log::info!("status: {line}");
            last = line;
        }
        shared.sleep(Duration::from_millis(500));
    }
    join_all(handles, Duration::from_secs(3));
    log::info!("stopped");
    Ok(())
}
