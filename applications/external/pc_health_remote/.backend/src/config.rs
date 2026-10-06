//! Persistent configuration: `%APPDATA%\pc-health-remote\config.toml`.

use anyhow::{Context, Result};
use serde::{Deserialize, Serialize};
use std::path::PathBuf;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Default, Serialize, Deserialize, clap::ValueEnum)]
#[serde(rename_all = "lowercase")]
pub enum TransportMode {
    #[default]
    Auto,
    Ble,
    Usb,
}

impl TransportMode {
    pub fn label(self) -> &'static str {
        match self {
            TransportMode::Auto => "Auto",
            TransportMode::Ble => "BLE",
            TransportMode::Usb => "USB",
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
#[serde(default)]
pub struct Config {
    /// Bluetooth address of the paired Flipper, hex string like "AA:BB:CC:DD:EE:FF".
    #[serde(skip_serializing_if = "Option::is_none")]
    pub last_device: Option<String>,
    /// auto | ble | usb
    pub transport: TransportMode,
    /// Default telemetry interval in seconds (1..=10); the Flipper's HELLO overrides it.
    pub interval_s: u8,
}

impl Default for Config {
    fn default() -> Self {
        Config { last_device: None, transport: TransportMode::Auto, interval_s: 1 }
    }
}

pub fn config_dir() -> PathBuf {
    if let Some(p) = std::env::var_os("PHR_CONFIG_DIR") {
        return PathBuf::from(p);
    }
    base_config_dir().join("pc-health-remote")
}

/// `%APPDATA%` on Windows, `$XDG_CONFIG_HOME` or `~/.config` elsewhere.
fn base_config_dir() -> PathBuf {
    let var = |k: &str| std::env::var_os(k).filter(|v| !v.is_empty()).map(PathBuf::from);
    if cfg!(windows) {
        var("APPDATA").unwrap_or_else(|| PathBuf::from("."))
    } else {
        var("XDG_CONFIG_HOME").or_else(|| var("HOME").map(|h| h.join(".config"))).unwrap_or_else(|| PathBuf::from("."))
    }
}

pub fn config_path() -> PathBuf {
    config_dir().join("config.toml")
}

pub fn log_path() -> PathBuf {
    config_dir().join("log.txt")
}

impl Config {
    pub fn parse(text: &str) -> Result<Config> {
        let mut c: Config = toml::from_str(text).context("invalid config.toml")?;
        c.interval_s = c.interval_s.clamp(1, 10);
        Ok(c)
    }

    /// Load the config; a missing or broken file yields defaults (the file is never overwritten
    /// by a failed parse).
    pub fn load() -> Config {
        match std::fs::read_to_string(config_path()) {
            Ok(text) => Config::parse(&text).unwrap_or_else(|e| {
                log::warn!("{e:#}; using defaults");
                Config::default()
            }),
            Err(_) => Config::default(),
        }
    }

    pub fn save(&self) -> Result<()> {
        let dir = config_dir();
        std::fs::create_dir_all(&dir).with_context(|| format!("creating {}", dir.display()))?;
        let body = toml::to_string_pretty(self).context("serialising config")?;
        let text = format!(
            "# PC Health Remote configuration\n\
             # transport = \"auto\" | \"ble\" | \"usb\"\n\
             # interval_s = default telemetry interval (the Flipper may request another one)\n\
             # last_device is written by `pc-health-remote pair`\n\n{body}"
        );
        let path = config_path();
        let tmp = path.with_extension("toml.tmp");
        std::fs::write(&tmp, text).with_context(|| format!("writing {}", tmp.display()))?;
        std::fs::rename(&tmp, &path).with_context(|| format!("replacing {}", path.display()))?;
        Ok(())
    }

    pub fn device_address(&self) -> Option<u64> {
        self.last_device.as_deref().and_then(parse_address)
    }
}

/// Parse "AA:BB:CC:DD:EE:FF", "AA-BB-...", or "AABBCCDDEEFF" into a 48-bit address.
pub fn parse_address(s: &str) -> Option<u64> {
    let hex: String = s.trim().chars().filter(|c| !matches!(c, ':' | '-' | ' ')).collect();
    if hex.len() != 12 || !hex.chars().all(|c| c.is_ascii_hexdigit()) {
        return None;
    }
    u64::from_str_radix(&hex, 16).ok()
}

pub fn format_address(a: u64) -> String {
    let b = a.to_be_bytes();
    format!("{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}", b[2], b[3], b[4], b[5], b[6], b[7])
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn address_round_trip() {
        let a = parse_address("aa:bb:cc:01:02:03").unwrap();
        assert_eq!(a, 0xAABB_CC01_0203);
        assert_eq!(format_address(a), "AA:BB:CC:01:02:03");
        assert_eq!(parse_address("AABBCC010203"), Some(a));
        assert_eq!(parse_address("AA:BB:CC:01:02"), None);
        assert_eq!(parse_address("ZZ:BB:CC:01:02:03"), None);
    }

    #[test]
    fn config_round_trip_and_clamp() {
        let c = Config { last_device: Some("AA:BB:CC:01:02:03".into()), transport: TransportMode::Usb, interval_s: 3 };
        let text = toml::to_string_pretty(&c).unwrap();
        assert_eq!(Config::parse(&text).unwrap(), c);
        let c = Config::parse("interval_s = 99\ntransport = \"ble\"").unwrap();
        assert_eq!(c.interval_s, 10);
        assert_eq!(c.transport, TransportMode::Ble);
        assert_eq!(Config::parse("").unwrap(), Config::default());
        assert!(Config::parse("transport = \"carrier-pigeon\"").is_err());
    }
}
