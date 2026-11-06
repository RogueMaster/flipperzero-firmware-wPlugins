//! Optional Telegram alert sender: POSTs fired alerts to the bot server (`/alert`) and links this
//! PC to a Telegram chat (`/pair`). Network work never runs on the telemetry thread.

use crate::alerts::AlertEvent;
use crate::config::TelegramConfig;
use anyhow::{bail, Context, Result};
use serde_json::{json, Value};
use std::sync::mpsc;
use std::time::Duration;

const TIMEOUT: Duration = Duration::from_secs(10);
/// Alerts queued while the server is unreachable; older ones are dropped.
const QUEUE_LIMIT: usize = 16;

fn agent() -> ureq::Agent {
    ureq::Agent::config_builder().timeout_global(Some(TIMEOUT)).build().into()
}

/// Official bot server; `server` in `[telegram]` overrides it (self-hosting or testing).
pub const DEFAULT_SERVER: &str = "https://phr-telegram.ataman-lab.workers.dev";

fn server_or_default(server: &str) -> &str {
    let s = server.trim();
    if s.is_empty() {
        DEFAULT_SERVER
    } else {
        s
    }
}

fn endpoint(server: &str, path: &str) -> String {
    format!("{}/{}", server_or_default(server).trim_end_matches('/'), path)
}

pub fn pc_name() -> String {
    sysinfo::System::host_name().filter(|n| !n.is_empty()).unwrap_or_else(|| "PC".to_string())
}

fn alert_body(ev: &AlertEvent, pc_name: &str) -> Value {
    json!({
        "rule": ev.rule.key(),
        "value": ev.value,
        "threshold": ev.threshold,
        "unit": ev.rule.unit(),
        "pc_name": pc_name,
    })
}

/// Background sender. Cloning is not needed; dropping it stops the worker.
pub struct Sender {
    tx: mpsc::SyncSender<AlertEvent>,
}

impl Sender {
    /// `None` unless `[telegram]` is enabled and fully configured.
    pub fn start(cfg: &TelegramConfig) -> Option<Sender> {
        if !cfg.enabled {
            return None;
        }
        if cfg.device_key.is_empty() {
            log::warn!("telegram alerts enabled but device_key missing; run --link-telegram <CODE>");
            return None;
        }
        let (tx, rx) = mpsc::sync_channel::<AlertEvent>(QUEUE_LIMIT);
        let url = endpoint(&cfg.server, "alert");
        let key = cfg.device_key.clone();
        let name = pc_name();
        let spawned = std::thread::Builder::new().name("telegram".into()).spawn(move || {
            let agent = agent();
            for ev in rx {
                let res = agent.post(&url).header("X-Device-Key", &key).send_json(alert_body(&ev, &name));
                if let Err(e) = res {
                    log::warn!("telegram: sending {} alert failed: {e}", ev.rule.key());
                }
            }
        });
        match spawned {
            Ok(_) => Some(Sender { tx }),
            Err(e) => {
                log::warn!("telegram: cannot start sender thread: {e}");
                None
            }
        }
    }

    /// Queue an alert; never blocks.
    pub fn send(&self, ev: AlertEvent) {
        if self.tx.try_send(ev).is_err() {
            log::warn!("telegram: queue full or sender stopped; dropping {} alert", ev.rule.key());
        }
    }
}

/// `--link-telegram <CODE>`: exchange the pairing code for a device key.
pub fn link(server: &str, code: &str) -> Result<String> {
    let code = code.trim();
    if code.is_empty() {
        bail!("empty pairing code");
    }
    let mut resp = agent()
        .post(&endpoint(server, "pair"))
        .send_json(json!({ "code": code, "pc_name": pc_name() }))
        .context("pairing request failed")?;
    let body: Value = resp.body_mut().read_json().context("unexpected reply from the server")?;
    match body.get("device_key").and_then(Value::as_str).filter(|k| !k.is_empty()) {
        Some(k) => Ok(k.to_string()),
        None => bail!("the server reply has no device_key"),
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::alerts::Rule;

    #[test]
    fn body_shape() {
        let ev = AlertEvent { rule: Rule::CpuTemp, value: 93, threshold: 90 };
        assert_eq!(
            alert_body(&ev, "DESKTOP"),
            json!({"rule": "cpu_temp", "value": 93, "threshold": 90, "unit": "C", "pc_name": "DESKTOP"})
        );
    }

    #[test]
    fn endpoint_joins_cleanly() {
        assert_eq!(endpoint("https://w.workers.dev/", "alert"), "https://w.workers.dev/alert");
        assert_eq!(endpoint(" https://w.workers.dev ", "pair"), "https://w.workers.dev/pair");
    }

    #[test]
    fn disabled_or_incomplete_has_no_sender() {
        let mut c = TelegramConfig::default();
        assert!(Sender::start(&c).is_none());
        c.enabled = true;
        assert!(Sender::start(&c).is_none());
    }

    #[test]
    fn link_needs_code() {
        assert!(link("", " ").is_err());
        assert!(link("https://x.invalid", " ").is_err());
    }

    #[test]
    fn empty_server_uses_default() {
        assert_eq!(endpoint("", "pair"), format!("{DEFAULT_SERVER}/pair"));
        assert_eq!(endpoint("  ", "alert"), format!("{DEFAULT_SERVER}/alert"));
        assert_eq!(endpoint("http://localhost:8787/", "alert"), "http://localhost:8787/alert");
    }
}
