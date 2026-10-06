//! Tiny file logger with size-based rotation (log.txt -> log.old.txt at ~1 MB).

use log::{LevelFilter, Log, Metadata, Record};
use std::fs::{self, File, OpenOptions};
use std::io::Write;
use std::path::PathBuf;
use std::sync::Mutex;
use std::time::{SystemTime, UNIX_EPOCH};

const MAX_BYTES: u64 = 1_000_000;

struct Inner {
    file: Option<File>,
    written: u64,
}

pub struct FileLogger {
    path: PathBuf,
    level: LevelFilter,
    echo: bool,
    inner: Mutex<Inner>,
}

/// Install the global logger. `echo` mirrors every line to stderr (console mode).
pub fn init(echo: bool, verbose: bool) {
    let level = match std::env::var("PHR_LOG").ok().as_deref() {
        Some("error") => LevelFilter::Error,
        Some("warn") => LevelFilter::Warn,
        Some("debug") => LevelFilter::Debug,
        Some("trace") => LevelFilter::Trace,
        Some("info") => LevelFilter::Info,
        _ if verbose => LevelFilter::Debug,
        _ => LevelFilter::Info,
    };
    let path = crate::config::log_path();
    let _ = fs::create_dir_all(crate::config::config_dir());
    let logger = FileLogger { path, level, echo, inner: Mutex::new(Inner { file: None, written: 0 }) };
    if log::set_boxed_logger(Box::new(logger)).is_ok() {
        log::set_max_level(level);
    }
}

impl FileLogger {
    fn write_line(&self, line: &str) {
        let mut g = self.inner.lock().unwrap_or_else(|e| e.into_inner());
        if g.file.is_none() {
            if let Ok(meta) = fs::metadata(&self.path) {
                if meta.len() >= MAX_BYTES {
                    self.rotate();
                }
            }
            if let Ok(f) = OpenOptions::new().create(true).append(true).open(&self.path) {
                g.written = f.metadata().map(|m| m.len()).unwrap_or(0);
                g.file = Some(f);
            }
        }
        if g.written + line.len() as u64 > MAX_BYTES {
            g.file = None;
            self.rotate();
            g.written = 0;
            g.file = OpenOptions::new().create(true).append(true).open(&self.path).ok();
        }
        if let Some(f) = g.file.as_mut() {
            if f.write_all(line.as_bytes()).is_ok() {
                g.written += line.len() as u64;
            }
        }
    }

    fn rotate(&self) {
        let old = self.path.with_file_name("log.old.txt");
        let _ = fs::remove_file(&old);
        let _ = fs::rename(&self.path, &old);
    }
}

impl Log for FileLogger {
    fn enabled(&self, m: &Metadata) -> bool {
        m.level() <= self.level
    }

    fn log(&self, r: &Record) {
        if !self.enabled(r.metadata()) {
            return;
        }
        let target = r.target().strip_prefix("pc_health_remote::").unwrap_or(r.target());
        let line = format!("{} {:<5} [{}] {}\n", timestamp(), r.level(), target, r.args());
        if self.echo {
            eprint!("{line}");
        }
        self.write_line(&line);
    }

    fn flush(&self) {
        if let Some(f) = self.inner.lock().unwrap_or_else(|e| e.into_inner()).file.as_mut() {
            let _ = f.flush();
        }
    }
}

/// UTC timestamp "YYYY-MM-DD HH:MM:SS.mmmZ" without pulling in a date crate.
fn timestamp() -> String {
    let d = SystemTime::now().duration_since(UNIX_EPOCH).unwrap_or_default();
    let secs = d.as_secs() as i64;
    let (y, m, day) = civil_from_days(secs.div_euclid(86_400));
    let sod = secs.rem_euclid(86_400);
    format!(
        "{y:04}-{m:02}-{day:02} {:02}:{:02}:{:02}.{:03}Z",
        sod / 3600,
        (sod % 3600) / 60,
        sod % 60,
        d.subsec_millis()
    )
}

/// Days since 1970-01-01 -> (year, month, day). Howard Hinnant's algorithm.
fn civil_from_days(z: i64) -> (i64, u32, u32) {
    let z = z + 719_468;
    let era = z.div_euclid(146_097);
    let doe = z.rem_euclid(146_097);
    let yoe = (doe - doe / 1460 + doe / 36_524 - doe / 146_096) / 365;
    let y = yoe + era * 400;
    let doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    let mp = (5 * doy + 2) / 153;
    let d = (doy - (153 * mp + 2) / 5 + 1) as u32;
    let m = if mp < 10 { mp + 3 } else { mp - 9 } as u32;
    (if m <= 2 { y + 1 } else { y }, m, d)
}

#[cfg(test)]
mod tests {
    use super::civil_from_days;

    #[test]
    fn civil_dates() {
        assert_eq!(civil_from_days(0), (1970, 1, 1));
        assert_eq!(civil_from_days(19_723), (2024, 1, 1));
        assert_eq!(civil_from_days(20_513), (2026, 3, 1));
    }
}
