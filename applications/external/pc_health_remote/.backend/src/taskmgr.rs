//! Task Scheduler integration: auto-start at logon, highest privileges, no UAC prompt.

use anyhow::Result;

pub const TASK_NAME: &str = "PC Health Remote";

/// Escape text for an XML element body.
pub fn xml_escape(s: &str) -> String {
    s.replace('&', "&amp;").replace('<', "&lt;").replace('>', "&gt;").replace('"', "&quot;").replace('\'', "&apos;")
}

/// Task Scheduler 1.4 definition: logon trigger for `user`, highest available run level,
/// runs on battery, no time limit, restart after failures, single instance.
pub fn task_xml(user: &str, exe: &str, args: &str) -> String {
    let user = xml_escape(user);
    format!(
        r#"<?xml version="1.0" encoding="UTF-16"?>
<Task version="1.4" xmlns="http://schemas.microsoft.com/windows/2004/02/mit/task">
  <RegistrationInfo>
    <Description>Streams PC health metrics to the PC Health Remote Flipper Zero app.</Description>
  </RegistrationInfo>
  <Triggers>
    <LogonTrigger>
      <Enabled>true</Enabled>
      <UserId>{user}</UserId>
      <Delay>PT10S</Delay>
    </LogonTrigger>
  </Triggers>
  <Principals>
    <Principal id="Author">
      <UserId>{user}</UserId>
      <LogonType>InteractiveToken</LogonType>
      <RunLevel>HighestAvailable</RunLevel>
    </Principal>
  </Principals>
  <Settings>
    <MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>
    <DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>
    <StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>
    <AllowHardTerminate>true</AllowHardTerminate>
    <StartWhenAvailable>true</StartWhenAvailable>
    <RunOnlyIfNetworkAvailable>false</RunOnlyIfNetworkAvailable>
    <IdleSettings>
      <StopOnIdleEnd>false</StopOnIdleEnd>
      <RestartOnIdle>false</RestartOnIdle>
    </IdleSettings>
    <AllowStartOnDemand>true</AllowStartOnDemand>
    <Enabled>true</Enabled>
    <Hidden>false</Hidden>
    <RunOnlyIfIdle>false</RunOnlyIfIdle>
    <WakeToRun>false</WakeToRun>
    <ExecutionTimeLimit>PT0S</ExecutionTimeLimit>
    <Priority>7</Priority>
    <RestartOnFailure>
      <Interval>PT1M</Interval>
      <Count>999</Count>
    </RestartOnFailure>
  </Settings>
  <Actions Context="Author">
    <Exec>
      <Command>{}</Command>
      <Arguments>{}</Arguments>
    </Exec>
  </Actions>
</Task>
"#,
        xml_escape(exe),
        xml_escape(args)
    )
}

#[cfg(windows)]
mod imp {
    use super::*;
    use crate::winutil;
    use anyhow::{bail, Context};
    use std::process::Command;

    const NOT_ELEVATED: &str = "Administrator rights are required to create the task (it runs with highest privileges so it can read CPU sensors without a UAC prompt).\n\
         Open an elevated terminal (right-click Terminal or PowerShell > Run as administrator) and run this command once:\n    pc-health-remote install";

    fn schtasks(args: &[&str]) -> Result<(bool, String)> {
        let mut cmd = Command::new("schtasks.exe");
        winutil::hide_window(&mut cmd).args(args);
        let out = cmd.output().context("running schtasks.exe")?;
        let mut text = String::from_utf8_lossy(&out.stdout).into_owned();
        text.push_str(&String::from_utf8_lossy(&out.stderr));
        Ok((out.status.success(), text.trim().to_string()))
    }

    fn current_user() -> String {
        let name = std::env::var("USERNAME").unwrap_or_default();
        match std::env::var("USERDOMAIN") {
            Ok(d) if !d.is_empty() => format!("{d}\\{name}"),
            _ => name,
        }
    }

    pub fn install(start_now: bool) -> Result<()> {
        if !winutil::is_elevated() {
            bail!("{NOT_ELEVATED}");
        }
        let exe = std::env::current_exe().context("locating this executable")?;
        let xml = task_xml(&current_user(), &exe.to_string_lossy(), "run");

        // schtasks expects UTF-16 (with BOM) task files.
        let mut bytes: Vec<u8> = vec![0xFF, 0xFE];
        bytes.extend(xml.encode_utf16().flat_map(u16::to_le_bytes));
        let path = std::env::temp_dir().join("pc-health-remote-task.xml");
        std::fs::write(&path, bytes).with_context(|| format!("writing {}", path.display()))?;
        let result = schtasks(&["/Create", "/TN", TASK_NAME, "/XML", &path.to_string_lossy(), "/F"]);
        let _ = std::fs::remove_file(&path);
        let (ok, text) = result?;
        if !ok {
            bail!("schtasks /Create failed: {text}");
        }
        println!(
            "Task \"{TASK_NAME}\" created: starts at logon of {}, highest privileges, restarts on failure.",
            current_user()
        );
        println!("Executable: {}  (re-run `install` if you move it)", exe.display());
        if start_now {
            let (ok, text) = schtasks(&["/Run", "/TN", TASK_NAME])?;
            if ok {
                println!("Started now.");
            } else {
                println!("Could not start it now: {text}");
            }
        }
        Ok(())
    }

    pub fn uninstall() -> Result<()> {
        let (ok, text) = schtasks(&["/Delete", "/TN", TASK_NAME, "/F"])?;
        if ok {
            println!("Task \"{TASK_NAME}\" removed. A running instance keeps running until you quit it from the tray.");
            return Ok(());
        }
        let lower = text.to_lowercase();
        if lower.contains("denied")
            || !winutil::is_elevated() && !lower.contains("cannot find") && !lower.contains("does not exist")
        {
            bail!("{text}\n{NOT_ELEVATED}");
        }
        println!("Task \"{TASK_NAME}\" is not installed.");
        Ok(())
    }

    /// Output of `schtasks /Query` for the task, or `None` if it does not exist.
    pub fn query() -> Result<Option<String>> {
        let (ok, text) = schtasks(&["/Query", "/TN", TASK_NAME, "/V", "/FO", "LIST"])?;
        Ok(ok.then_some(text))
    }

    /// Number of other running instances of this executable (by image name).
    pub fn other_instances() -> usize {
        let exe = std::env::current_exe().ok().and_then(|p| p.file_name().map(|n| n.to_string_lossy().into_owned()));
        let Some(exe) = exe else { return 0 };
        let filter = format!("IMAGENAME eq {exe}");
        let Ok((_, text)) = schtasks_tasklist(&filter) else { return 0 };
        let me = std::process::id().to_string();
        text.lines().filter(|l| l.contains(&exe) && !l.contains(&format!("\"{me}\""))).count()
    }

    fn schtasks_tasklist(filter: &str) -> Result<(bool, String)> {
        let mut cmd = Command::new("tasklist.exe");
        winutil::hide_window(&mut cmd).args(["/FI", filter, "/FO", "CSV", "/NH"]);
        let out = cmd.output().context("running tasklist.exe")?;
        Ok((out.status.success(), String::from_utf8_lossy(&out.stdout).into_owned()))
    }
}

#[cfg(not(windows))]
mod imp {
    use anyhow::{bail, Result};

    const MSG: &str =
        "Task Scheduler is Windows-only. On Linux use a systemd user service that runs `pc-health-remote run`.";

    pub fn install(_start_now: bool) -> Result<()> {
        bail!(MSG)
    }

    pub fn uninstall() -> Result<()> {
        bail!(MSG)
    }

    pub fn query() -> Result<Option<String>> {
        Ok(None)
    }

    pub fn other_instances() -> usize {
        0
    }
}

pub fn install(start_now: bool) -> Result<()> {
    imp::install(start_now)
}

pub fn uninstall() -> Result<()> {
    imp::uninstall()
}

pub fn query() -> Result<Option<String>> {
    imp::query()
}

pub fn other_instances() -> usize {
    imp::other_instances()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn xml_has_required_parts_and_escapes() {
        let x = task_xml("PC\\Vlad & Co", "C:\\Program Files\\PHR\\pc-health-remote.exe", "run");
        assert!(x.contains("<LogonTrigger>"));
        assert!(x.contains("<RunLevel>HighestAvailable</RunLevel>"));
        assert!(x.contains("<UserId>PC\\Vlad &amp; Co</UserId>"));
        assert!(x.contains("<Command>C:\\Program Files\\PHR\\pc-health-remote.exe</Command>"));
        assert!(x.contains("<Arguments>run</Arguments>"));
        assert!(x.contains("<RestartOnFailure>"));
        assert!(x.contains("<DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>"));
        assert!(x.contains("<ExecutionTimeLimit>PT0S</ExecutionTimeLimit>"));
    }
}
