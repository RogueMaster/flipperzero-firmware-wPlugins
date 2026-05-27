"""Input backend abstraction — sends keystrokes to a registered runner target.

Backends
--------
AppleScriptInputBackend (macOS only)
    Uses osascript / System Events to inject keystrokes.

XdotoolInputBackend (Linux X11 only)
    Uses xdotool to inject keystrokes into the focused or targeted window.

NullInputBackend
    No-op fallback used when no platform backend is available.

Factory
-------
Call ``create_backend()`` to obtain the configured backend instance.
"""

import asyncio
import logging
import sys
from abc import ABC, abstractmethod
from dataclasses import dataclass

log = logging.getLogger(__name__)


# ---------------------------------------------------------------------------
# Abstract interface
# ---------------------------------------------------------------------------

class InputBackend(ABC):
    """Interface every input backend must implement."""

    def set_target(self, target: dict[str, str] | None) -> None:
        """Optionally bind future input to a specific runner session."""

    @abstractmethod
    async def send_ctrl_c(self) -> None:
        """Send Ctrl+C to the current runner target."""

    @abstractmethod
    async def send_keystroke(self, key: str) -> None:
        """Send a named key ('return', 'escape', 'down', 'tab', 'backspace')."""

    @abstractmethod
    async def send_text(self, text: str) -> None:
        """Type *text* and press Return in the focused application."""

    @abstractmethod
    async def send_chars(self, text: str, *, focus: bool = True) -> None:
        """Type text without pressing Return."""

    @abstractmethod
    async def send_modified_keystroke(self, key_code: int, modifiers: str) -> None:
        """Send a key code with modifier(s) (e.g. 'control down')."""

# ---------------------------------------------------------------------------
# macOS AppleScript backend
# ---------------------------------------------------------------------------

def _key_code(key: str) -> int:
    codes = {
        "return":    36,
        "escape":    53,
        "down":     125,
        "space":     49,
        "tab":       48,
        "backspace": 51,
        "page_up":  116,
        "page_down": 121,
    }
    return codes.get(key, 36)


def _escape_applescript(text: str) -> str:
    return text.replace("\\", "\\\\").replace('"', '\\"')


def _clean_target_value(value: object) -> str:
    if value is None:
        return ""
    return str(value).strip()


@dataclass(slots=True)
class InputTarget:
    session_key: str = ""
    app_name: str = ""
    term_program: str = ""
    term_session_id: str = ""
    iterm_session_id: str = ""
    tty: str = ""
    window_id: str = ""  # X11 window ID (Linux only, set by VTE terminals)

    @classmethod
    def from_payload(cls, payload: dict[str, str] | None) -> "InputTarget | None":
        if not payload:
            return None
        target = cls(
            session_key=_clean_target_value(payload.get("session_key")),
            app_name=_clean_target_value(payload.get("app_name")),
            term_program=_clean_target_value(payload.get("term_program")),
            term_session_id=_clean_target_value(payload.get("term_session_id")),
            iterm_session_id=_clean_target_value(payload.get("iterm_session_id")),
            tty=_clean_target_value(payload.get("tty")),
            window_id=_clean_target_value(payload.get("window_id")),
        )
        if not any(
            (
                target.app_name,
                target.term_program,
                target.term_session_id,
                target.iterm_session_id,
                target.tty,
                target.window_id,
            )
        ):
            return None
        return target

    def describe(self) -> str:
        parts = []
        if self.app_name:
            parts.append(f"app={self.app_name}")
        if self.tty:
            parts.append(f"tty={self.tty}")
        if self.window_id:
            parts.append(f"window_id={self.window_id}")
        if self.term_session_id:
            parts.append(f"term_session_id={self.term_session_id}")
        if self.iterm_session_id:
            parts.append(f"iterm_session_id={self.iterm_session_id}")
        if not parts:
            return "default"
        return ", ".join(parts)


def _generic_focus_script(app_name: str) -> str:
    escaped_app = _escape_applescript(app_name)
    return (
        "set __flipperTargetFocused to false\n"
        "try\n"
        f'    tell application "{escaped_app}" to activate\n'
        "    set __flipperTargetFocused to true\n"
        "end try\n"
        "if __flipperTargetFocused then delay 0.05\n"
    )


def _terminal_focus_script(target: InputTarget) -> str:
    if not target.tty:
        return _generic_focus_script("Terminal")

    escaped_tty = _escape_applescript(target.tty)
    return (
        "set __flipperTargetFocused to false\n"
        "try\n"
        '    tell application "Terminal"\n'
        "        repeat with w in windows\n"
        "            repeat with t in tabs of w\n"
        f'                if tty of t is "{escaped_tty}" then\n'
        "                    set index of w to 1\n"
        "                    set selected of t to true\n"
        "                    activate\n"
        "                    set __flipperTargetFocused to true\n"
        "                    exit repeat\n"
        "                end if\n"
        "            end repeat\n"
        "            if __flipperTargetFocused then exit repeat\n"
        "        end repeat\n"
        "    end tell\n"
        "end try\n"
        "if not __flipperTargetFocused then\n"
        + "\n".join(f"    {line}" for line in _generic_focus_script("Terminal").splitlines())
        + "\nend if\n"
    )


def _focus_script(target: InputTarget | None) -> str:
    if not target:
        return ""
    if target.app_name == "Terminal":
        return _terminal_focus_script(target)
    if target.app_name:
        return _generic_focus_script(target.app_name)
    return ""


def _build_key_code_script(
    key_code: int,
    modifiers: str = "",
    target: InputTarget | None = None,
) -> str:
    lines = []
    focus = _focus_script(target)
    if focus:
        lines.append(focus.rstrip())
    lines.extend(
        [
            'tell application "System Events"',
            (
                f"    key code {key_code} using {{{modifiers}}}"
                if modifiers
                else f"    key code {key_code}"
            ),
            "end tell",
        ]
    )
    return "\n".join(lines)


def _build_send_text_script(text: str, target: InputTarget | None = None) -> str:
    lines = []
    focus = _focus_script(target)
    if focus:
        lines.append(focus.rstrip())
    lines.extend(
        [
            'tell application "System Events"',
            f'    keystroke "{_escape_applescript(text)}"',
            "    key code 36",
            "end tell",
        ]
    )
    return "\n".join(lines)


def _build_send_chars_script(
    text: str,
    target: InputTarget | None = None,
    *,
    focus: bool = True,
) -> str:
    lines = []
    focus_script = _focus_script(target) if focus else ""
    if focus_script:
        lines.append(focus_script.rstrip())
    lines.extend(
        [
            'tell application "System Events"',
            f'    keystroke "{_escape_applescript(text)}"',
            "end tell",
        ]
    )
    return "\n".join(lines)


async def _run_applescript(script: str, context: str) -> None:
    try:
        proc = await asyncio.create_subprocess_exec(
            "osascript", "-e", script,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )
        stdout, stderr = await proc.communicate()
        if proc.returncode != 0:
            message = stderr.decode().strip() or stdout.decode().strip() or f"rc={proc.returncode}"
            log.warning("%s failed: %s", context, message)
    except Exception as e:
        log.error("%s error: %s", context, e)


class AppleScriptInputBackend(InputBackend):
    """macOS input backend using osascript / System Events."""

    def __init__(self) -> None:
        self._target: InputTarget | None = None

    def set_target(self, target: dict[str, str] | None) -> None:
        self._target = InputTarget.from_payload(target)
        log.info(
            "Input target set: %s",
            self._target.describe() if self._target else "frontmost application",
        )

    async def send_ctrl_c(self) -> None:
        await _run_applescript(
            _build_key_code_script(8, modifiers="control down", target=self._target),
            "Ctrl+C",
        )

    async def send_keystroke(self, key: str) -> None:
        await _run_applescript(
            _build_key_code_script(_key_code(key), target=self._target),
            f"keystroke({key})",
        )

    async def send_text(self, text: str) -> None:
        await _run_applescript(
            _build_send_text_script(text, target=self._target),
            "send_text",
        )

    async def send_chars(self, text: str, *, focus: bool = True) -> None:
        await _run_applescript(
            _build_send_chars_script(text, target=self._target, focus=focus),
            "send_chars",
        )

    async def send_modified_keystroke(self, key_code: int, modifiers: str) -> None:
        await _run_applescript(
            _build_key_code_script(key_code, modifiers=modifiers, target=self._target),
            f"keystroke(code={key_code}, mod={modifiers})",
        )


# ---------------------------------------------------------------------------
# Null backend (fallback)
# ---------------------------------------------------------------------------

class NullInputBackend(InputBackend):
    """No-op fallback when no platform input backend is available."""

    _warned: bool = False

    def _warn(self) -> None:
        if not NullInputBackend._warned:
            NullInputBackend._warned = True
            log.warning(
                "No input backend available on this platform — "
                "Flipper button-to-keystroke forwarding is disabled. "
                "On Linux install xdotool (apt install xdotool) to enable it."
            )

    async def send_ctrl_c(self) -> None:
        self._warn()

    async def send_keystroke(self, key: str) -> None:
        self._warn()

    async def send_text(self, text: str) -> None:
        self._warn()

    async def send_chars(self, text: str, *, focus: bool = True) -> None:
        self._warn()

    async def send_modified_keystroke(self, key_code: int, modifiers: str) -> None:
        self._warn()


# ---------------------------------------------------------------------------
# Linux xdotool backend
# ---------------------------------------------------------------------------

# Mapping from abstract key names to X11 keysyms used by xdotool
_XDOTOOL_KEY_NAMES: dict[str, str] = {
    "return":    "Return",
    "escape":    "Escape",
    "down":      "Down",
    "up":        "Up",
    "left":      "Left",
    "right":     "Right",
    "space":     "space",
    "tab":       "Tab",
    "backspace": "BackSpace",
    "page_up":   "Prior",
    "page_down": "Next",
}

# macOS keycode → X11 keysym (only codes actually used by the bridge)
_MACOS_KEYCODE_TO_XSYM: dict[int, str] = {
    8:   "c",        # Ctrl+C
    36:  "Return",
    48:  "Tab",
    49:  "space",
    51:  "BackSpace",
    53:  "Escape",
    116: "Prior",
    121: "Next",
    125: "Down",
}

# macOS modifier phrase → xdotool modifier prefix
_MACOS_MOD_TO_XDOTOOL: dict[str, str] = {
    "control down": "ctrl",
    "shift down":   "shift",
    "option down":  "alt",
    "command down": "super",
}


async def _run_xdotool(args: list[str], context: str) -> None:
    try:
        proc = await asyncio.create_subprocess_exec(
            "xdotool", *args,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )
        stdout, stderr = await proc.communicate()
        if proc.returncode != 0:
            message = stderr.decode().strip() or stdout.decode().strip() or f"rc={proc.returncode}"
            log.warning("%s failed: %s", context, message)
    except Exception as e:
        log.error("%s error: %s", context, e)


class XdotoolInputBackend(InputBackend):
    """Linux X11 input backend using xdotool."""

    def __init__(self) -> None:
        self._target: InputTarget | None = None

    def set_target(self, target: dict[str, str] | None) -> None:
        self._target = InputTarget.from_payload(target)
        log.info(
            "Input target set: %s",
            self._target.describe() if self._target else "active window",
        )

    def _window_args(self) -> list[str]:
        """Return --window <id> args if a window ID is known, else empty list."""
        if self._target and self._target.window_id:
            return ["--window", self._target.window_id]
        return []

    async def _focus(self) -> None:
        if self._target and self._target.window_id:
            await _run_xdotool(
                ["windowfocus", "--sync", self._target.window_id],
                "focus",
            )

    async def send_ctrl_c(self) -> None:
        await self._focus()
        await _run_xdotool([*self._window_args(), "key", "--clearmodifiers", "ctrl+c"], "Ctrl+C")

    async def send_keystroke(self, key: str) -> None:
        xsym = _XDOTOOL_KEY_NAMES.get(key, key)
        await self._focus()
        await _run_xdotool([*self._window_args(), "key", "--clearmodifiers", xsym], f"keystroke({key})")

    async def send_text(self, text: str) -> None:
        await self._focus()
        await _run_xdotool(
            [*self._window_args(), "type", "--clearmodifiers", "--", text],
            "type",
        )
        await _run_xdotool([*self._window_args(), "key", "Return"], "Return")

    async def send_chars(self, text: str, *, focus: bool = True) -> None:
        if focus:
            await self._focus()
        await _run_xdotool(
            [*self._window_args(), "type", "--clearmodifiers", "--", text],
            "type_chars",
        )

    async def send_modified_keystroke(self, key_code: int, modifiers: str) -> None:
        xsym = _MACOS_KEYCODE_TO_XSYM.get(key_code, str(key_code))
        xmod = _MACOS_MOD_TO_XDOTOOL.get(modifiers.lower().strip())
        combo = f"{xmod}+{xsym}" if xmod else xsym
        await self._focus()
        await _run_xdotool(
            [*self._window_args(), "key", "--clearmodifiers", combo],
            f"keystroke(code={key_code}, mod={modifiers})",
        )


# ---------------------------------------------------------------------------
# Factory
# ---------------------------------------------------------------------------

def create_backend() -> InputBackend:
    if sys.platform == "darwin":
        return AppleScriptInputBackend()
    if sys.platform == "linux":
        import shutil
        if shutil.which("xdotool"):
            log.info("Input backend: xdotool (Linux X11)")
            return XdotoolInputBackend()
        log.warning(
            "xdotool not found — keystroke forwarding disabled. "
            "Install it with: sudo apt install xdotool"
        )
        return NullInputBackend()
    log.warning("No input backend for platform %r — keystroke forwarding disabled.", sys.platform)
    return NullInputBackend()
