"""Dictation backend abstraction + built-in implementations.

Backends
--------
MacOSDictationBackend (macOS only)
    Triggers macOS native dictation via AppleScript (Edit menu) and detects
    active state via pmset power assertions.

CustomDictationBackend
    Runs user-supplied shell commands for start / stop / activity check.
    Configure via env vars or config.py (see config.DICTATION_* settings).

NullDictationBackend
    No-op backend used when dictation is disabled (default on non-macOS).

Factory
-------
Call ``create_backend()`` to obtain the configured backend instance.
"""

import asyncio
import logging
import subprocess
from abc import ABC, abstractmethod

log = logging.getLogger(__name__)


# ---------------------------------------------------------------------------
# Abstract interface
# ---------------------------------------------------------------------------

class DictationBackend(ABC):
    """Interface every dictation backend must implement."""

    async def discover(self) -> None:
        """One-time initialisation called at daemon startup (optional)."""

    @abstractmethod
    async def start(self) -> None:
        """Start dictation."""

    async def stop(self) -> None:
        """Stop dictation (backend-specific cleanup; ESC is sent separately)."""

    def is_active(self) -> bool:
        """Return True if dictation is still running (sync, called in executor).

        Called periodically by the daemon's sync loop.  If the backend cannot
        determine state, returning True is safe: the bridge keeps its current
        "dictating" flag and the user presses UP again to stop.
        """
        return True


# ---------------------------------------------------------------------------
# macOS native backend (AppleScript + pmset)
# ---------------------------------------------------------------------------

_MACOS_SCRIPT = """\
tell application "System Events"
    tell (first process whose frontmost is true)
        tell menu bar 1
            tell menu bar item 4
                tell menu 1
                    set allItems to every menu item
                    repeat with anItem in allItems
                        set n to name of anItem
                        if n contains "Dictation" and n does not contain "Cancel" then
                            {action}
                        end if
                    end repeat
                end tell
            end tell
        end tell
    end tell
end tell"""

_MACOS_READ_SCRIPT   = _MACOS_SCRIPT.format(action="return n")
_MACOS_TOGGLE_SCRIPT = _MACOS_SCRIPT.format(action="click anItem\nreturn")


async def _run_applescript(script: str) -> str | None:
    try:
        proc = await asyncio.create_subprocess_exec(
            "osascript", "-e", script,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )
        stdout, stderr = await proc.communicate()
        if proc.returncode != 0:
            log.error("AppleScript failed (rc=%d): %s", proc.returncode, stderr.decode().strip())
            return None
        return stdout.decode().strip() or None
    except Exception as e:
        log.error("AppleScript error: %s", e)
        return None


class MacOSDictationBackend(DictationBackend):
    """macOS native dictation triggered via the Edit menu (language-independent)."""

    async def start(self) -> None:
        await _run_applescript(_MACOS_TOGGLE_SCRIPT)

    # stop() is intentionally left as no-op: the daemon sends ESC which
    # dismisses the macOS dictation overlay.

    def is_active(self) -> bool:
        """Check for active dictation via pmset power assertions."""
        try:
            output = subprocess.check_output(
                ["pmset", "-g", "assertions"], stderr=subprocess.DEVNULL
            ).decode()
            return "Dictation" in output or "com.apple.speech.recognition" in output
        except Exception:
            return False


# ---------------------------------------------------------------------------
# Null backend (disabled / unsupported platform)
# ---------------------------------------------------------------------------

class NullDictationBackend(DictationBackend):
    """No-op backend used when dictation is disabled or unavailable."""

    async def start(self) -> None:
        log.warning(
            "Dictation is not configured on this platform. "
            "Set FLIPPER_DICTATION_BACKEND=custom and FLIPPER_DICTATION_START_CMD to enable it."
        )

    def is_active(self) -> bool:
        return False


# ---------------------------------------------------------------------------
# Custom shell-command backend
# ---------------------------------------------------------------------------

async def _run_shell(cmd: str) -> None:
    """Run *cmd* in a shell, logging any errors."""
    try:
        proc = await asyncio.create_subprocess_shell(
            cmd,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )
        _, stderr = await proc.communicate()
        if proc.returncode != 0:
            log.error("Dictation command failed (rc=%d): %s", proc.returncode, stderr.decode().strip())
    except Exception as e:
        log.error("Dictation command error: %s", e)


class CustomDictationBackend(DictationBackend):
    """Dictation backend driven by user-supplied shell commands.

    Parameters
    ----------
    start_cmd:
        Shell command to start dictation (required).
    stop_cmd:
        Shell command to stop dictation.  If empty, only ESC is sent.
    check_cmd:
        Shell command that exits 0 while dictation is active.  If empty,
        ``is_active()`` always returns True (daemon relies on manual toggle).
    """

    def __init__(
        self,
        start_cmd: str,
        stop_cmd: str = "",
        check_cmd: str = "",
    ) -> None:
        if not start_cmd:
            raise ValueError("CustomDictationBackend requires a non-empty start_cmd")
        self._start_cmd = start_cmd
        self._stop_cmd = stop_cmd
        self._check_cmd = check_cmd

    async def start(self) -> None:
        await _run_shell(self._start_cmd)

    async def stop(self) -> None:
        if self._stop_cmd:
            await _run_shell(self._stop_cmd)

    def is_active(self) -> bool:
        if not self._check_cmd:
            # No check command: assume still active so the sync loop doesn't
            # prematurely clear the dictating flag.  User presses UP to stop.
            return True
        try:
            result = subprocess.run(
                self._check_cmd,
                shell=True,
                capture_output=True,
                timeout=5,
            )
            return result.returncode == 0
        except Exception:
            return True  # Fail-safe: assume active on error


# ---------------------------------------------------------------------------
# Factory
# ---------------------------------------------------------------------------

def create_backend() -> DictationBackend:
    """Return the configured DictationBackend instance.

    Reads ``config.DICTATION_BACKEND`` ("macos" or "custom") plus the
    ``DICTATION_START/STOP/CHECK_CMD`` settings for the custom backend.
    """
    from . import config

    backend_type = config.DICTATION_BACKEND.lower()

    if backend_type == "none":
        log.info("Dictation backend: none (disabled)")
        return NullDictationBackend()

    if backend_type == "macos":
        log.info("Dictation backend: macOS native")
        return MacOSDictationBackend()

    if backend_type == "custom":
        start_cmd = config.DICTATION_START_CMD
        if not start_cmd:
            raise ValueError(
                "DICTATION_BACKEND=custom but DICTATION_START_CMD is not set. "
                "Set FLIPPER_DICTATION_START_CMD or config.DICTATION_START_CMD."
            )
        log.info(
            "Dictation backend: custom (start=%r stop=%r check=%r)",
            start_cmd, config.DICTATION_STOP_CMD, config.DICTATION_CHECK_CMD,
        )
        return CustomDictationBackend(
            start_cmd=start_cmd,
            stop_cmd=config.DICTATION_STOP_CMD,
            check_cmd=config.DICTATION_CHECK_CMD,
        )

    raise ValueError(
        f"Unknown DICTATION_BACKEND={backend_type!r}. "
        "Valid values: 'macos', 'custom', 'none'."
    )
