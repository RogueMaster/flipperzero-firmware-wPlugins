//! PC Health Remote - PC backend. Streams system metrics to the Flipper Zero app over BLE/USB.

#![cfg_attr(all(windows, not(debug_assertions)), windows_subsystem = "windows")]
// Parts of the code (tray, Task Scheduler, WMI sensors) are only used on Windows.
#![cfg_attr(not(windows), allow(dead_code))]

mod app;
mod ble;
mod config;
#[cfg_attr(not(windows), allow(dead_code))]
mod icon;
mod logging;
mod metrics;
mod protocol;
mod session;
mod shared;
mod taskmgr;
#[cfg(windows)]
mod tray;
mod usb;
#[cfg(windows)]
mod winutil;

use anyhow::{Context, Result};
use clap::{Args, Parser, Subcommand};
use config::{format_address, parse_address, Config, TransportMode};
use std::time::Duration;

#[derive(Parser)]
#[command(
    name = "pc-health-remote",
    version,
    about = "Streams PC health metrics to the PC Health Remote Flipper Zero app"
)]
struct Cli {
    #[command(flatten)]
    global: Global,
    #[command(subcommand)]
    command: Option<Cmd>,
}

#[derive(Args)]
struct Global {
    /// Attach or allocate a console and echo the log to it (the Windows release build has no console)
    #[arg(long, global = true)]
    console: bool,
    /// Debug-level logging
    #[arg(long, short, global = true)]
    verbose: bool,
    /// Send synthetic, changing values instead of real metrics (to test alerts on the Flipper)
    #[arg(long, global = true)]
    simulate: bool,
    /// Transport for this run (not saved): auto, ble or usb
    #[arg(long, value_enum, global = true)]
    transport: Option<TransportMode>,
    /// Default telemetry interval in seconds for this run (the Flipper's HELLO overrides it)
    #[arg(long, global = true, value_parser = clap::value_parser!(u8).range(1..=10))]
    interval: Option<u8>,
    /// Run without a tray icon (headless; Ctrl-C to stop)
    #[arg(long, global = true)]
    no_tray: bool,
}

#[derive(Subcommand)]
enum Cmd {
    /// Run the tray app and stream telemetry (default)
    Run,
    /// Scan for a "PCHealth*" Flipper and pair with it without any Windows UI
    Pair {
        /// Bluetooth address to pair with instead of the strongest "PCHealth*" device
        #[arg(long)]
        address: Option<String>,
        /// Remove an existing pairing first (use after resetting the Flipper app's keys)
        #[arg(long)]
        force: bool,
        /// Seconds to scan
        #[arg(long, default_value_t = 10)]
        scan_secs: u64,
    },
    /// Remove the Windows pairing and forget the saved device
    Unpair {
        /// Address to unpair (default: the saved device)
        #[arg(long)]
        address: Option<String>,
    },
    /// Print one JSON snapshot of all metrics and where each came from
    Metrics,
    /// Create the "PC Health Remote" scheduled task (run once, elevated)
    Install {
        /// Do not start the task right away
        #[arg(long)]
        no_start: bool,
    },
    /// Remove the scheduled task
    Uninstall,
    /// Show configuration, pairing, USB ports, scheduled task and running state
    Status,
}

fn main() {
    let cli = Cli::parse();
    let is_run = matches!(cli.command, None | Some(Cmd::Run));
    let echo = cli.global.console || !is_run;

    #[cfg(windows)]
    if echo {
        winutil::attach_console();
    }
    logging::init(echo, cli.global.verbose);
    std::panic::set_hook(Box::new(|info| log::error!("panic: {info}")));

    if let Err(e) = dispatch(cli) {
        log::error!("{e:#}");
        if !echo {
            eprintln!("error: {e:#}");
        }
        std::process::exit(1);
    }
}

fn dispatch(cli: Cli) -> Result<()> {
    match cli.command.unwrap_or(Cmd::Run) {
        Cmd::Run => app::run(app::RunOpts {
            simulate: cli.global.simulate,
            no_tray: cli.global.no_tray,
            transport: cli.global.transport,
            interval_s: cli.global.interval,
        }),
        Cmd::Pair { address, force, scan_secs } => cmd_pair(address, force, scan_secs),
        Cmd::Unpair { address } => cmd_unpair(address),
        Cmd::Metrics => cmd_metrics(),
        Cmd::Install { no_start } => taskmgr::install(!no_start),
        Cmd::Uninstall => taskmgr::uninstall(),
        Cmd::Status => cmd_status(),
    }
}

fn parse_addr_arg(a: &str) -> Result<u64> {
    parse_address(a).with_context(|| format!("\"{a}\" is not a Bluetooth address (expected AA:BB:CC:DD:EE:FF)"))
}

fn cmd_pair(address: Option<String>, force: bool, scan_secs: u64) -> Result<()> {
    let address = address.as_deref().map(parse_addr_arg).transpose()?;
    println!(
        "Scanning for \"{}*\" devices for {scan_secs} s (open the PC Health Remote app on the Flipper)...",
        ble::NAME_PREFIX
    );
    let outcome = ble::pair(address, force, Duration::from_secs(scan_secs.clamp(2, 60)))?;
    app::remember_device(None, outcome.address)?;
    let name = if outcome.name.is_empty() { "Flipper".to_string() } else { outcome.name.clone() };
    if outcome.already_paired {
        println!(
            "{name} ({}) is already paired; saved as last_device. Use --force to re-pair.",
            format_address(outcome.address)
        );
    } else {
        println!(
            "Paired with {name} ({}); saved as last_device in {}",
            format_address(outcome.address),
            config::config_path().display()
        );
    }
    println!("Start the tray app with `pc-health-remote run` (or `install` to start it at logon).");
    Ok(())
}

fn cmd_unpair(address: Option<String>) -> Result<()> {
    let mut cfg = Config::load();
    let addr = match address.as_deref() {
        Some(a) => parse_addr_arg(a)?,
        None => cfg.device_address().context("no saved device; pass --address")?,
    };
    ble::unpair(addr)?;
    if cfg.device_address() == Some(addr) {
        cfg.last_device = None;
        cfg.save()?;
    }
    println!("Unpaired {} and cleared it from the config.", format_address(addr));
    Ok(())
}

fn cmd_metrics() -> Result<()> {
    let mut collector = metrics::Collector::new();
    // sysinfo needs two samples a short while apart for CPU percentages.
    std::thread::sleep(Duration::from_millis(700));
    let snap = collector.collect();
    let t = snap.to_telemetry();
    let frame = t.encode(0);
    let name = |b: &[u8]| String::from_utf8_lossy(&b[..b.iter().position(|&c| c == 0).unwrap_or(b.len())]).into_owned();
    let hex: String = frame.iter().map(|b| format!("{b:02x}")).collect();
    let out = serde_json::json!({
        "snapshot": snap,
        "wire": {
            "flags": format!("0x{:02x}", t.flags()),
            "cpu_temp_valid": t.cpu_temp_valid,
            "gpu_present": t.gpu_present,
            "gpu_temp_valid": t.gpu_temp_valid,
            "battery_present": t.battery_present,
            "fan_valid": t.fan_valid,
            "ram_total_dgb": t.ram_total_dgb,
            "vram_total_dgb": t.vram_total_dgb,
            "top_cpu_name": name(&t.top_cpu_name),
            "top_ram_name": name(&t.top_ram_name),
            "frame_hex": hex,
        },
        "summary": snap.summary(),
    });
    println!("{}", serde_json::to_string_pretty(&out)?);
    Ok(())
}

fn cmd_status() -> Result<()> {
    let cfg = Config::load();
    println!("PC Health Remote {}", env!("CARGO_PKG_VERSION"));
    println!("Config:    {}", config::config_path().display());
    println!(
        "           transport = {}, interval_s = {}, last_device = {}",
        cfg.transport.label(),
        cfg.interval_s,
        cfg.last_device.as_deref().unwrap_or("(none - run `pair`)")
    );
    println!("Log:       {}", config::log_path().display());
    #[cfg(windows)]
    println!("Elevated:  {}", if winutil::is_elevated() { "yes" } else { "no" });
    match cfg.device_address() {
        Some(a) => println!(
            "BLE:       {} paired with the OS: {}",
            format_address(a),
            match ble::paired_state(a) {
                Some(true) => "yes",
                Some(false) => "no",
                None => "unknown",
            }
        ),
        None => println!("BLE:       no device saved"),
    }
    let ports = usb::flipper_ports();
    println!(
        "USB:       {}",
        if ports.is_empty() { "no Flipper serial port found".to_string() } else { ports.join(", ") }
    );
    match taskmgr::query() {
        Ok(Some(text)) => {
            println!("Task:      installed");
            let keys = ["TaskName", "Status", "Last Run Time", "Last Result", "Next Run Time", "Run As User"];
            let picked: Vec<&str> =
                text.lines().filter(|l| keys.iter().any(|k| l.trim_start().starts_with(k))).collect();
            for l in if picked.is_empty() { text.lines().collect() } else { picked } {
                println!("           {}", l.trim());
            }
        }
        Ok(None) => println!("Task:      not installed (run `pc-health-remote install` from an elevated terminal)"),
        Err(e) => println!("Task:      cannot query ({e:#})"),
    }
    let others = taskmgr::other_instances();
    println!("Running:   {}", if others > 0 { format!("yes ({others} instance(s))") } else { "no".to_string() });
    if cfg.transport == TransportMode::Ble && cfg.last_device.is_none() {
        println!("Note:      transport is BLE but no device is paired yet");
    }
    Ok(())
}
