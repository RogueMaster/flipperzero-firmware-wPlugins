"""Main daemon: routes messages between Flipper serial and Claude Code IPC."""

import asyncio
import json
import logging
import os
from collections import OrderedDict
from pathlib import Path
from . import config, protocol
from .claude_ipc import ClaudeIPC
from .serial_conn import SerialConnection
from .transport import Transport
from .input import InputBackend, create_backend as create_input_backend
from .voice import DictationBackend, create_backend as create_dictation_backend

log = logging.getLogger(__name__)


class Daemon:
    def __init__(self, transport: Transport, dictation: DictationBackend | None = None, input_backend: InputBackend | None = None):
        self.serial = SerialConnection(transport)
        self.ipc = ClaudeIPC()
        self._dictation: DictationBackend = dictation or create_dictation_backend()
        self._input: InputBackend = input_backend or create_input_backend()
        self._dictating = False
        self._claude_connected = False
        self._perm_future: asyncio.Future | None = None
        self._menu_sent = False
        self._cmd_map: dict[str, str] = {}  # truncated -> full command
        self._space_repeat_task: asyncio.Task | None = None
        self._session_targets: OrderedDict[str, dict[str, str]] = OrderedDict()

        self.serial.on_message(self._handle_flipper_msg)
        self.serial.on_connect(self._send_initial_state)
        self.ipc.on_action(self._handle_ipc_action)

    async def _handle_flipper_msg(self, msg: dict):
        msg_type = msg.get("t", "")
        data = msg.get("d", {})
        log.debug("Flipper RX: type=%s data=%s", msg_type, data)

        if msg_type == "hello":
            bt_name = data.get("bt", "")
            log.info("Flipper connected: fw=%s bt=%s", data.get("fw", "?"), bt_name or "?")
            if bt_name and config.BT_NAME_CACHE:
                try:
                    with open(config.BT_NAME_CACHE, "w") as f:
                        f.write(bt_name)
                    log.info("Cached BT name: %s", bt_name)
                except Exception as e:
                    log.warning("Failed to cache BT name: %s", e)
            if bt_name:
                _save_bt_name_to_plugin_config(bt_name)
            # Flipper app just (re)started — cancel any pending permission request
            if self._perm_future and not self._perm_future.done():
                log.info("Cancelling stale permission request")
                self._perm_future.set_result(None)
            await self.serial.send(
                protocol.notify_msg("ready", vibro=True, text="Claude Code", subtext="Connected")
            )
            # Restore Claude state if already connected
            if self._claude_connected:
                await self.serial.send(protocol.state_msg(True))
            # Send command menu
            commands = self._load_commands()
            if commands:
                menu_bytes = protocol.menu_msg(commands)
                log.debug("Sending menu (%d commands, %d bytes)", len(commands), len(menu_bytes))
                await self.serial.send(menu_bytes)
            else:
                log.warning("No commands loaded — menu not sent")
            self._menu_sent = True

        elif msg_type == "cmd":
            text = data.get("text", "")
            if text:
                text = self._cmd_map.get(text, text)
                await self._input.send_chars(text + " ")

        elif msg_type == "yes":
            await self._send_to_claude("yes")

        elif msg_type == "enter":
            await self._send_keystroke("return")

        elif msg_type == "pgup":
            await self._send_keystroke("page_up")

        elif msg_type == "pgdown":
            await self._send_keystroke("page_down")

        elif msg_type == "ctrl_o":
            await self._input.send_modified_keystroke(31, "control down")

        elif msg_type == "ctrl_e":
            await self._input.send_modified_keystroke(14, "control down")

        elif msg_type == "shift_tab":
            await self._input.send_modified_keystroke(48, "shift down")

        elif msg_type == "down":
            await self._send_keystroke("down")

        elif msg_type == "esc":
            await self._send_keystroke("escape")
            if self._dictating:
                self._dictating = False
                await self._dictation.stop()
                # ESC already played sound+vibro locally; just reset LED
                await self.serial.send(
                    protocol.notify_msg("voice_stop_quiet", vibro=False, text="")
                )

        elif msg_type == "voice":
            log.info("Voice UP received (dictating=%s)", self._dictating)
            try:
                if self._dictating:
                    self._dictating = False
                    await self._dictation.stop()
                    await self._send_keystroke("escape")
                    # Button already played sound+vibro locally; just reset LED
                    await self.serial.send(
                        protocol.notify_msg("voice_stop_quiet", vibro=False, text="")
                    )
                    log.info("Dictation stopped")
                else:
                    await self._dictation.start()
                    self._dictating = True
                    # Button already played sound+vibro locally; just start LED blink
                    await self.serial.send(
                        protocol.notify_msg("voice_start_led", vibro=False, text="")
                    )
                    log.info("Dictation started")
            except Exception as e:
                log.error("Voice handler error: %s", e)
                await self.serial.send(protocol.status_msg("", ""))

        elif msg_type == "backspace":
            await self._send_keystroke("backspace")

        elif msg_type == "space_down":
            await self._start_space_repeat()

        elif msg_type == "space_up":
            await self._stop_space_repeat()

        elif msg_type == "interrupt":
            log.info("Flipper interrupt request — sending Ctrl+C")
            await self._send_ctrl_c()

        elif msg_type == "perm_resp":
            allowed = data.get("allow", False)
            always = data.get("always", False)
            esc = data.get("esc", False)
            log.info("Flipper perm_resp: allow=%s always=%s esc=%s future=%s",
                     allowed, always, esc, self._perm_future is not None)
            if self._perm_future and not self._perm_future.done():
                self._perm_future.set_result({"allow": allowed, "always": always})
            if esc:
                await self._send_keystroke("escape")

        elif msg_type == "pong":
            if not self._menu_sent:
                log.info("First pong — sending initial state")
                self._menu_sent = True
                commands = self._load_commands()
                if commands:
                    menu_bytes = protocol.menu_msg(commands)
                    log.debug("Sending menu (%d commands, %d bytes)", len(commands), len(menu_bytes))
                    await self.serial.send(menu_bytes)
                if self._claude_connected:
                    await self.serial.send(protocol.state_msg(True))

    async def _handle_ipc_action(self, request: dict) -> dict:
        action = request.get("action", "")

        if action == "notify":
            if self._perm_future and not self._perm_future.done():
                log.info("Dismissing pending permission (deferring to Claude)")
                self._perm_future.set_result({"ask": True})
            sound = request.get("sound", "alert")
            vibro = request.get("vibro", True)
            text = request.get("text", "")
            subtext = request.get("subtext", "")
            await self.serial.send(protocol.notify_msg(sound, vibro, text, subtext))
            return {"status": "ok"}

        elif action == "display":
            text = request.get("text", "")[:21]
            sub = request.get("subtext", "")[:21]
            await self.serial.send(protocol.status_msg(text, sub))
            return {"status": "ok"}

        elif action == "claude_connect":
            # Update project dir if provided (may differ from bridge startup)
            project_dir = request.get("project_dir", "")
            if project_dir:
                config.PROJECT_DIR = project_dir
                log.info("Updated PROJECT_DIR to %s", project_dir)
            self._claude_connected = True
            await self.serial.send(protocol.state_msg(True))
            # Refresh commands for the (possibly new) project
            commands = self._load_commands()
            if commands and self.serial.connected:
                await self.serial.send(protocol.menu_msg(commands))
            return {"status": "ok"}

        elif action == "claude_disconnect":
            await self._stop_space_repeat()
            self._claude_connected = False
            await self.serial.send(protocol.state_msg(False))
            return {"status": "ok"}

        elif action == "register_target":
            session_key = self._register_target(request)
            if not session_key:
                return {"status": "invalid_target"}
            return {"status": "ok", "session_key": session_key}

        elif action == "release_target":
            released = self._release_target(str(request.get("session_key", "")).strip())
            return {"status": "ok", "released": released}

        elif action == "permission_request":
            tool = request.get("tool", "Tool")[:21]
            detail = request.get("detail", "")[:21]
            log.info("Permission request: %s %s", tool, detail)

            if self._perm_future and not self._perm_future.done():
                log.info("Permission busy, rejecting")
                return {"status": "busy", "allowed": False}

            if not self.serial.connected:
                log.info("No Flipper, falling back")
                return {"status": "no_flipper", "allowed": None}

            self._perm_future = asyncio.get_running_loop().create_future()
            await self.serial.send(protocol.perm_msg(tool, detail))

            if not self.serial.connected:
                log.info("Send failed, no Flipper")
                self._perm_future = None
                return {"status": "no_flipper"}

            try:
                log.info("Waiting for Flipper response (60s timeout)")
                result = await asyncio.wait_for(self._perm_future, timeout=60.0)
                if result is None:
                    log.info("Permission cancelled (Flipper reset)")
                    return {"status": "no_flipper"}
                if result.get("ask"):
                    log.info("Permission dismissed — deferring to Claude")
                    return {"status": "ask"}
                log.info("Permission result: %s", result)
                return {
                    "status": "ok",
                    "allowed": result["allow"],
                    "always": result["always"],
                }
            except asyncio.TimeoutError:
                log.info("Permission timed out")
                return {"status": "timeout"}
            finally:
                self._perm_future = None

        return {"status": "unknown_action"}

    # Built-in slash commands
    BUILTIN_COMMANDS = [
        "/add-dir", "/agents", "/autofix-pr", "/batch", "/branch", "/btw", "/buddy",
        "/chrome", "/claude-api", "/clear", "/color", "/compact", "/config", "/context",
        "/copy", "/debug", "/desktop", "/diff", "/doctor", "/effort", "/exit", "/export",
        "/extra-usage", "/fast", "/feedback", "/help", "/hooks", "/ide", "/init",
        "/insights", "/install-github-app", "/install-slack-app", "/keybindings",
        "/login", "/logout", "/loop", "/mcp", "/memory", "/mobile", "/model",
        "/permissions", "/plan", "/plugin", "/powerup", "/release-notes",
        "/reload-plugins", "/remote-control", "/remote-env", "/rename", "/resume",
        "/review", "/rewind", "/sandbox", "/schedule", "/security-review", "/skills",
        "/stats", "/status", "/statusline", "/stickers", "/tasks", "/team-onboarding",
        "/teleport", "/terminal-setup", "/theme", "/ultraplan", "/update-config",
        "/usage", "/voice",
    ]

    def _load_commands(self) -> list[str]:
        """Load built-in commands + all non-internal slash commands.

        Sources (checked in both ``~/.claude/`` and ``<project>/.claude/``):
        1. ``commands/*.md``  — filename (without .md) becomes ``/<name>``
        2. ``skills/<name>/SKILL.md`` — ``name`` frontmatter becomes ``/<name>``
        3. Enabled plugins (``enabledPlugins`` in settings.json) — for each
           enabled plugin resolved from ``~/.claude/plugins/marketplaces/``
           (falls back to ``cache/`` for externally-sourced plugins):
           - ``commands/*.md``  → ``/<plugin>:<filename>``
           - ``skills/<name>/SKILL.md`` → ``/<skill-name>`` (from frontmatter)
        """
        commands: set[str] = set(self.BUILTIN_COMMANDS)

        home_claude = Path.home() / ".claude"
        project_claude = Path(config.PROJECT_DIR) / ".claude"
        roots = [home_claude, project_claude]
        log.debug("Command discovery: home=%s project=%s", home_claude, project_claude)

        # 1. commands/*.md
        for root in roots:
            commands_dir = root / "commands"
            if commands_dir.is_dir():
                for md in commands_dir.glob("*.md"):
                    cmd = "/" + md.stem
                    log.debug("  command file: %s -> %s", md, cmd)
                    commands.add(cmd)

        # 2. skills/<name>/SKILL.md
        for root in roots:
            skills_dir = root / "skills"
            if skills_dir.is_dir():
                for skill_md in skills_dir.glob("*/SKILL.md"):
                    name = self._parse_skill_name(skill_md)
                    if name:
                        log.debug("  skill: %s -> %s", skill_md, name)
                        commands.add(name)
                    else:
                        log.warning("  skill: %s — no name in frontmatter, skipped", skill_md)

        # 3. Enabled plugins
        enabled = self._get_enabled_plugins(roots)
        log.debug("Enabled plugins: %s", enabled)
        plugins_base = home_claude / "plugins"
        for plugin_key, marketplace in enabled.items():
            plugin_dir = self._resolve_plugin_in_marketplace(
                plugins_base / "marketplaces" / marketplace, plugin_key
            ) or self._resolve_plugin_in_cache(
                plugins_base / "cache" / marketplace / plugin_key
            )
            if not plugin_dir:
                log.warning("  plugin %s@%s — not found in marketplace or cache", plugin_key, marketplace)
                continue
            plugin_name = self._read_plugin_name(plugin_dir) or plugin_key
            log.debug("  plugin %s@%s resolved to %s (name=%s)", plugin_key, marketplace, plugin_dir, plugin_name)
            # Plugin commands/*.md
            pcmd_dir = plugin_dir / "commands"
            if pcmd_dir.is_dir():
                for md in pcmd_dir.glob("*.md"):
                    cmd = "/" + plugin_name + ":" + md.stem
                    log.debug("    plugin command: %s -> %s", md, cmd)
                    commands.add(cmd)
            # Plugin skills/<name>/SKILL.md
            pskills_dir = plugin_dir / "skills"
            if pskills_dir.is_dir():
                for skill_md in pskills_dir.glob("*/SKILL.md"):
                    name = self._parse_skill_name(skill_md)
                    if name:
                        log.debug("    plugin skill: %s -> %s", skill_md, name)
                        commands.add(name)
                    else:
                        log.warning("    plugin skill: %s — no name in frontmatter, skipped", skill_md)

        # Legacy: flipper-commands.txt
        for path in config.CUSTOM_COMMANDS_FILES:
            if not os.path.isfile(path):
                continue
            try:
                with open(path) as f:
                    for line in f:
                        line = line.strip()
                        if line and not line.startswith("#"):
                            commands.add(line)
            except Exception as e:
                log.error("Error reading %s: %s", path, e)

        # Build truncated->full mapping; Flipper menu items are 26 chars max
        MENU_ITEM_MAX = 26
        self._cmd_map = {}
        for cmd in commands:
            truncated = cmd[:MENU_ITEM_MAX]
            self._cmd_map[truncated] = cmd

        result = sorted(self._cmd_map.keys())
        custom_count = len(result) - len(self.BUILTIN_COMMANDS)
        log.info("Loaded %d commands (%d built-in + %d custom)",
                 len(result), len(self.BUILTIN_COMMANDS), custom_count)
        return result

    @staticmethod
    def _parse_skill_name(skill_md: Path) -> str | None:
        """Extract ``name`` from SKILL.md YAML frontmatter, return as ``/name``."""
        try:
            text = skill_md.read_text(encoding="utf-8")
        except Exception:
            return None
        if not text.startswith("---"):
            return None
        end = text.find("---", 3)
        if end == -1:
            return None
        for line in text[3:end].splitlines():
            line = line.strip()
            if line.startswith("name:"):
                value = line[5:].strip().strip("'\"")
                if value:
                    return "/" + value
        return None

    @staticmethod
    def _get_enabled_plugins(roots: list[Path]) -> dict[str, str]:
        """Return ``{pluginName: marketplaceName}`` from settings.json in each root."""
        enabled: dict[str, str] = {}
        for root in roots:
            settings_file = root / "settings.json"
            if not settings_file.is_file():
                continue
            try:
                data = json.loads(settings_file.read_text(encoding="utf-8"))
            except Exception:
                continue
            for key, val in (data.get("enabledPlugins") or {}).items():
                if not val:
                    continue
                # key format: "pluginName@marketplace"
                parts = key.split("@", 1)
                if len(parts) == 2:
                    plugin_name, marketplace = parts
                    enabled.setdefault(plugin_name, marketplace)
        return enabled

    @staticmethod
    def _resolve_plugin_in_marketplace(mkt_dir: Path, plugin_name: str) -> Path | None:
        """Find a plugin directory inside a marketplace clone.

        Checks, in order:
        - ``<mkt>/plugins/<plugin>/``  (bundled plugin)
        - ``<mkt>/external_plugins/<plugin>/``  (external plugin stub)
        - Any dir under ``<mkt>/`` containing ``.claude-plugin/plugin.json``
          whose ``name`` field matches *plugin_name* (e.g. single-plugin repos
          where the plugin root is a subdirectory).
        """
        if not mkt_dir.is_dir():
            return None
        for subdir in ("plugins", "external_plugins"):
            candidate = mkt_dir / subdir / plugin_name
            if candidate.is_dir():
                return candidate
        # Search for .claude-plugin/plugin.json with matching name
        for pj in mkt_dir.glob("**/.claude-plugin/plugin.json"):
            plugin_dir = pj.parent.parent
            # Skip the marketplace-level .claude-plugin (same dir as mkt_dir)
            if plugin_dir == mkt_dir:
                continue
            try:
                data = json.loads(pj.read_text(encoding="utf-8"))
                if data.get("name") == plugin_name:
                    return plugin_dir
            except Exception:
                continue
        return None

    @staticmethod
    def _resolve_plugin_in_cache(base: Path) -> Path | None:
        """Fallback: find the latest versioned dir under ``cache/<mkt>/<plugin>/``."""
        if not base.is_dir():
            return None
        versions = sorted(
            (d for d in base.iterdir() if d.is_dir() and not d.name.startswith(".")),
            reverse=True,
        )
        return versions[0] if versions else None

    @staticmethod
    def _read_plugin_name(plugin_dir: Path) -> str | None:
        """Read ``name`` from ``.claude-plugin/plugin.json``."""
        pj = plugin_dir / ".claude-plugin" / "plugin.json"
        if not pj.is_file():
            return None
        try:
            data = json.loads(pj.read_text(encoding="utf-8"))
            return data.get("name") or None
        except Exception:
            return None

    async def _send_initial_state(self):
        """Wait for Flipper app to take over CDC, then send a ping.
        The actual state (menu, claude status) is sent on first pong or hello."""
        self._menu_sent = False
        await asyncio.sleep(2.0)
        if self.serial.connected:
            await self.serial.send_ping()

    async def _dictation_sync_loop(self):
        """Poll dictation backend state and correct bridge/Flipper state on mismatch."""
        while True:
            await asyncio.sleep(config.DICTATION_POLL_INTERVAL)
            if not self._dictating:
                continue
            try:
                active = await asyncio.get_running_loop().run_in_executor(
                    None, self._dictation.is_active
                )
            except Exception as e:
                log.error("Dictation poll error: %s", e)
                continue
            if not active:
                log.info("Dictation ended outside bridge — syncing state")
                self._dictating = False
                await self.serial.send(
                    protocol.notify_msg("voice_stop_quiet", vibro=False, text="Dictation stopped")
                )

    async def _send_ctrl_c(self):
        await self._input.send_ctrl_c()

    async def _send_keystroke(self, key: str):
        await self._input.send_keystroke(key)

    async def _send_to_claude(self, text: str):
        await self._input.send_text(text)

    async def _space_repeat_loop(self):
        first_send = True
        try:
            while True:
                await self._input.send_chars(" ", focus=first_send)
                first_send = False
                await asyncio.sleep(config.SPACE_REPEAT_INTERVAL)
        except asyncio.CancelledError:
            raise

    async def _start_space_repeat(self):
        if self._space_repeat_task is not None:
            return
        self._space_repeat_task = asyncio.create_task(self._space_repeat_loop())

    async def _stop_space_repeat(self):
        if self._space_repeat_task is None:
            return
        task = self._space_repeat_task
        self._space_repeat_task = None
        task.cancel()
        try:
            await task
        except asyncio.CancelledError:
            pass

    def _normalize_target(self, request: dict) -> dict[str, str] | None:
        target = {
            "session_key": str(request.get("session_key", "")).strip(),
            "app_name": str(request.get("app_name", "")).strip(),
            "term_program": str(request.get("term_program", "")).strip(),
            "term_session_id": str(request.get("term_session_id", "")).strip(),
            "iterm_session_id": str(request.get("iterm_session_id", "")).strip(),
            "tty": str(request.get("tty", "")).strip(),
        }
        if not target["session_key"]:
            return None
        if not any(
            (
                target["app_name"],
                target["term_program"],
                target["term_session_id"],
                target["iterm_session_id"],
                target["tty"],
            )
        ):
            return None
        return target

    def _apply_active_target(self) -> None:
        active = next(reversed(self._session_targets.values()), None)
        self._input.set_target(active)

    def _register_target(self, request: dict) -> str | None:
        target = self._normalize_target(request)
        if not target:
            log.info("Ignoring invalid input target registration: %s", request)
            return None

        session_key = target["session_key"]
        self._session_targets.pop(session_key, None)
        self._session_targets[session_key] = target
        self._apply_active_target()
        log.info(
            "Registered input target session=%s app=%s tty=%s",
            session_key,
            target["app_name"] or "?",
            target["tty"] or "?",
        )
        return session_key

    def _release_target(self, session_key: str) -> bool:
        if not session_key:
            return False
        released = self._session_targets.pop(session_key, None) is not None
        self._apply_active_target()
        if released:
            log.info("Released input target session=%s", session_key)
        return released

    async def run(self):
        await self._dictation.discover()
        await self.ipc.start()

        if await self.serial.connect():
            self.serial._read_task = asyncio.create_task(self.serial.read_loop())
            self.serial._ping_task = asyncio.create_task(self.serial.ping_loop())
            await self._send_initial_state()

        asyncio.create_task(self.serial.reconnect_loop())
        asyncio.create_task(self._dictation_sync_loop())

        log.info("=" * 60)
        log.info("Bridge daemon started")
        log.info("=" * 60)
        try:
            await asyncio.Event().wait()
        except asyncio.CancelledError:
            pass
        finally:
            await self._stop_space_repeat()
            # Await the transport close so the Flipper sees a clean BLE
            # disconnect before this process exits. Without this, the next
            # bridge process (spawned when the user starts a new Claude
            # session) can't reconnect until the Flipper's link-supervision
            # timeout fires.
            try:
                await asyncio.wait_for(self.serial.aclose(), timeout=3.0)
            except asyncio.TimeoutError:
                log.warning("Serial aclose timed out; forcing close")
                self.serial.close()
            except Exception as e:
                log.warning("Serial aclose error: %s", e)
            await self.ipc.stop()
            log.info("=" * 60)
            log.info("Bridge daemon stopped")
            log.info("=" * 60)


def _save_bt_name_to_plugin_config(bt_name: str) -> None:
    """Persist bt_name into ~/.claude/settings.json as the bluetoothName userConfig.

    Only called when connected via USB so the Flipper's own reported name is
    authoritative. On the next BLE-only session the bridge reads this value
    and scans for the correct device without any manual configuration.
    """
    settings_path = Path.home() / ".claude" / "settings.json"
    try:
        settings = json.loads(settings_path.read_text()) if settings_path.exists() else {}
        plugin_cfg = settings.setdefault("pluginConfigs", {}).setdefault("flipper-claude-buddy", {})
        options = plugin_cfg.setdefault("options", {})
        if options.get("bluetoothName") == bt_name:
            return  # already up to date, skip the write
        options["bluetoothName"] = bt_name
        settings_path.write_text(json.dumps(settings, indent=2))
        log.info("Saved bluetoothName=%r to plugin config", bt_name)
    except Exception as e:
        log.warning("Failed to save bluetoothName to plugin config: %s", e)
