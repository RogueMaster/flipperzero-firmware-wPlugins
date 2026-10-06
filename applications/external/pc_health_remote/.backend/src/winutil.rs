//! Small Win32 helpers: console attach for the GUI-subsystem binary, elevation check,
//! single-instance mutex, hidden child processes.

use std::ffi::c_void;
use std::process::Command;
use windows::core::HSTRING;
use windows::Win32::Foundation::{CloseHandle, GetLastError, ERROR_ALREADY_EXISTS, HANDLE};
use windows::Win32::Security::{GetTokenInformation, TokenElevation, TOKEN_ELEVATION, TOKEN_QUERY};
use windows::Win32::System::Console::{
    AllocConsole, AttachConsole, GetStdHandle, SetStdHandle, ATTACH_PARENT_PROCESS, STD_ERROR_HANDLE, STD_HANDLE,
    STD_INPUT_HANDLE, STD_OUTPUT_HANDLE,
};
use windows::Win32::System::Threading::{CreateMutexW, GetCurrentProcess, OpenProcessToken};

fn handle_is_usable(id: STD_HANDLE) -> bool {
    // SAFETY: GetStdHandle has no preconditions.
    match unsafe { GetStdHandle(id) } {
        Ok(h) => !h.is_invalid() && !h.0.is_null(),
        Err(_) => false,
    }
}

#[cfg(not(phr_check))]
fn bind_console_stream(device: &str, id: STD_HANDLE, write: bool) {
    use std::os::windows::io::AsRawHandle;
    let opened = std::fs::OpenOptions::new().read(!write).write(write).open(device);
    if let Ok(f) = opened {
        // SAFETY: the handle stays valid because the File is intentionally leaked below.
        let _ = unsafe { SetStdHandle(id, HANDLE(f.as_raw_handle())) };
        std::mem::forget(f);
    }
}

#[cfg(phr_check)]
fn bind_console_stream(_device: &str, _id: STD_HANDLE, _write: bool) {
    let _ = SetStdHandle;
}

/// The release binary uses the Windows GUI subsystem (no console window). For commands that
/// print, attach to the parent terminal (or allocate a console) and connect stdio to it, unless
/// the streams are already redirected to a file or pipe.
pub fn attach_console() {
    let (out_ok, err_ok, in_ok) =
        (handle_is_usable(STD_OUTPUT_HANDLE), handle_is_usable(STD_ERROR_HANDLE), handle_is_usable(STD_INPUT_HANDLE));
    if out_ok && err_ok {
        return;
    }
    // SAFETY: plain console API calls.
    unsafe {
        if AttachConsole(ATTACH_PARENT_PROCESS).is_err() && AllocConsole().is_err() {
            return;
        }
    }
    if !out_ok {
        bind_console_stream("CONOUT$", STD_OUTPUT_HANDLE, true);
    }
    if !err_ok {
        bind_console_stream("CONOUT$", STD_ERROR_HANDLE, true);
    }
    if !in_ok {
        bind_console_stream("CONIN$", STD_INPUT_HANDLE, false);
    }
}

/// True if this process runs with an elevated (administrator) token.
pub fn is_elevated() -> bool {
    // SAFETY: standard token query; the handle is closed before returning.
    unsafe {
        let mut token = HANDLE::default();
        if OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &mut token).is_err() {
            return false;
        }
        let mut elevation = TOKEN_ELEVATION::default();
        let mut returned = 0u32;
        let ok = GetTokenInformation(
            token,
            TokenElevation,
            Some(std::ptr::from_mut(&mut elevation).cast::<c_void>()),
            std::mem::size_of::<TOKEN_ELEVATION>() as u32,
            &mut returned,
        )
        .is_ok();
        let _ = CloseHandle(token);
        ok && elevation.TokenIsElevated != 0
    }
}

/// Holds the single-instance mutex for the lifetime of the process.
pub struct InstanceGuard(HANDLE);

impl Drop for InstanceGuard {
    fn drop(&mut self) {
        // SAFETY: the handle was returned by CreateMutexW and is closed exactly once.
        unsafe {
            let _ = CloseHandle(self.0);
        }
    }
}

/// `None` if another instance (possibly elevated) is already running in this session.
pub fn single_instance() -> Option<InstanceGuard> {
    let name = HSTRING::from("Local\\PCHealthRemote.SingleInstance");
    // SAFETY: the name is a valid NUL-terminated wide string for the call duration.
    unsafe {
        let h = CreateMutexW(None, true, &name).ok()?;
        if GetLastError() == ERROR_ALREADY_EXISTS {
            let _ = CloseHandle(h);
            return None;
        }
        Some(InstanceGuard(h))
    }
}

/// Run a helper process without flashing a console window.
#[cfg(not(phr_check))]
pub fn hide_window(cmd: &mut Command) -> &mut Command {
    use std::os::windows::process::CommandExt;
    const CREATE_NO_WINDOW: u32 = 0x0800_0000;
    cmd.creation_flags(CREATE_NO_WINDOW)
}

#[cfg(phr_check)]
pub fn hide_window(cmd: &mut Command) -> &mut Command {
    cmd
}
