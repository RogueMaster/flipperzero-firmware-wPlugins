"""FlipDeck action system – modular, extensible action handlers."""

import importlib.util
import os
import platform
import subprocess
import sys
import yaml

# ── Built-in action handlers ──────────────────────────────


def act_shell(cfg):
    """Run a shell command."""
    cmd = cfg.get("command", "")
    wait = cfg.get("wait", False)
    if wait:
        r = subprocess.run(cmd, shell=True, capture_output=True, timeout=30)
        return r.returncode == 0
    else:
        subprocess.Popen(
            cmd, shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
        )
        return True


def act_python(cfg):
    """Run a Python script."""
    script = cfg.get("script", "")
    args = cfg.get("args", [])
    subprocess.Popen([sys.executable, script] + args)
    return True


def act_bash(cfg):
    """Run a bash/sh script."""
    script = cfg.get("script", "")
    subprocess.Popen(["bash", script])
    return True


def act_keypress(cfg):
    """Simulate a key combination."""
    keys = cfg.get("keys", "")  # e.g. "cmd+shift+4" or "ctrl+c"
    if sys.platform == "darwin":
        return _keypress_macos(keys)
    else:
        return _keypress_linux(keys)


def _keypress_macos(keys):
    parts = [k.strip().lower() for k in keys.split("+")]
    mod_map = {
        "cmd": "command down",
        "command": "command down",
        "shift": "shift down",
        "alt": "option down",
        "option": "option down",
        "ctrl": "control down",
        "control": "control down",
        "fn": "fn down",
    }
    mods = [mod_map[p] for p in parts[:-1] if p in mod_map]
    key = parts[-1] if parts else ""
    if mods:
        script = (
            f'tell app "System Events" to keystroke "{key}" using {{{", ".join(mods)}}}'
        )
    else:
        script = f'tell app "System Events" to keystroke "{key}"'
    subprocess.run(["osascript", "-e", script], timeout=5, capture_output=True)
    return True


def _keypress_linux(keys):
    try:
        subprocess.run(
            ["xdotool", "key", keys.replace("+", "plus").replace("cmd", "super")],
            timeout=5,
            capture_output=True,
        )
        return True
    except FileNotFoundError:
        print("  [!] xdotool not installed (sudo apt install xdotool)")
        return False


def act_open(cfg):
    """Open a file, folder, URL, or application."""
    target = cfg.get("target", "")
    if sys.platform == "darwin":
        app = cfg.get("app", "")
        cmd = ["open"]
        if app:
            cmd += ["-a", app]
        cmd.append(target)
        subprocess.Popen(cmd)
    else:
        subprocess.Popen(["xdg-open", target])
    return True


def act_paste(cfg):
    """Type/paste a string via HID-like simulation."""
    text = cfg.get("text", "")
    if sys.platform == "darwin":
        # Use pbcopy + cmd+v approach
        proc = subprocess.Popen(["pbcopy"], stdin=subprocess.PIPE)
        proc.communicate(text.encode())
        subprocess.run(
            [
                "osascript",
                "-e",
                'tell app "System Events" to keystroke "v" using command down',
            ],
            timeout=5,
            capture_output=True,
        )
    else:
        subprocess.run(
            ["xdotool", "type", "--clearmodifiers", text],
            timeout=10,
            capture_output=True,
        )
    return True


def act_http(cfg):
    """Make an HTTP request (curl-like)."""
    import urllib.request
    import urllib.error

    url = cfg.get("url", "")
    method = cfg.get("method", "GET").upper()
    headers = cfg.get("headers", {})
    body = cfg.get("body", None)
    try:
        data = body.encode() if body else None
        req = urllib.request.Request(url, data=data, method=method)
        for k, v in headers.items():
            req.add_header(k, v)
        urllib.request.urlopen(req, timeout=10)
        return True
    except Exception as e:
        print(f"  [!] HTTP error: {e}")
        return False


def act_osascript(cfg):
    """Run an AppleScript (macOS only)."""
    if sys.platform != "darwin":
        print("  [!] osascript only works on macOS")
        return False
    script = cfg.get("script", "")
    subprocess.run(["osascript", "-e", script], timeout=10, capture_output=True)
    return True


def act_shortcut(cfg):
    """Run a macOS Shortcuts.app shortcut."""
    if sys.platform != "darwin":
        print("  [!] Shortcuts only works on macOS")
        return False
    name = cfg.get("name", "")
    subprocess.run(["shortcuts", "run", name], timeout=30, capture_output=True)
    return True


# ── Handler registry ──────────────────────────────────────

BUILTIN_HANDLERS = {
    "shell": act_shell,
    "python": act_python,
    "bash": act_bash,
    "keypress": act_keypress,
    "open": act_open,
    "paste": act_paste,
    "http": act_http,
    "osascript": act_osascript,
    "shortcut": act_shortcut,
}

# ── Plugin loading ────────────────────────────────────────


def load_plugins(plugins_dir):
    """Load custom action plugins from a directory.

    Each plugin is a .py file that defines:
        ACTION_TYPE = "my_action"
        def execute(cfg: dict) -> bool: ...
    """
    handlers = {}
    if not os.path.isdir(plugins_dir):
        return handlers

    for fname in sorted(os.listdir(plugins_dir)):
        if not fname.endswith(".py") or fname.startswith("_"):
            continue
        path = os.path.join(plugins_dir, fname)
        try:
            spec = importlib.util.spec_from_file_location(fname[:-3], path)
            mod = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mod)
            if hasattr(mod, "ACTION_TYPE") and hasattr(mod, "execute"):
                handlers[mod.ACTION_TYPE] = mod.execute
                print(f"  Loaded plugin: {mod.ACTION_TYPE} ({fname})")
        except Exception as e:
            print(f"  [!] Failed to load plugin {fname}: {e}")

    return handlers


# ── Action executor ───────────────────────────────────────


class ActionExecutor:
    def __init__(self, actions_file, plugins_dir=None):
        self.actions = {}
        self.handlers = dict(BUILTIN_HANDLERS)
        if plugins_dir:
            self.handlers.update(load_plugins(plugins_dir))
        self.reload(actions_file)
        self._actions_file = actions_file

    def reload(self, actions_file=None):
        """(Re)load actions from YAML file."""
        path = actions_file or self._actions_file
        if not os.path.exists(path):
            self.actions = {}
            return
        with open(path) as f:
            data = yaml.safe_load(f) or {}
        self.actions = data.get("actions", {})

    def execute(self, key_name):
        """Execute action for a given key (e.g. 'f13')."""
        key_name = key_name.lower()
        if key_name not in self.actions:
            return None

        action = self.actions[key_name]
        action_type = action.get("type", "")
        name = action.get("name", key_name)
        enabled = action.get("enabled", True)

        if not enabled:
            print(f"  [{name}] disabled")
            return False

        handler = self.handlers.get(action_type)
        if not handler:
            print(f"  [!] Unknown action type: {action_type}")
            return False

        try:
            result = handler(action)
            status = "\u2713" if result else "\u2717"
            print(f"  {status} [{name}] {action_type}")
            return result
        except Exception as e:
            print(f"  \u2717 [{name}] Error: {e}")
            return False

    def get_all(self):
        """Return all configured actions."""
        return dict(self.actions)

    def set_action(self, key, config):
        """Set/update an action."""
        self.actions[key] = config
        self._save()

    def delete_action(self, key):
        """Delete an action."""
        self.actions.pop(key, None)
        self._save()

    def _save(self):
        with open(self._actions_file, "w") as f:
            yaml.dump(
                {"actions": self.actions}, f, default_flow_style=False, sort_keys=False
            )


# ── Defaults ──────────────────────────────────────────────

DEFAULT_ACTIONS = {
    "actions": {
        "f13": {
            "name": "Open Terminal",
            "type": "open",
            "target": "",
            "app": "Terminal",
            "enabled": True,
        },
        "f14": {
            "name": "Open Browser",
            "type": "open",
            "target": "https://google.com",
            "enabled": True,
        },
        "f15": {
            "name": "Screenshot",
            "type": "keypress",
            "keys": "cmd+shift+4",
            "enabled": True,
        },
        "f16": {
            "name": "Toggle Dark Mode",
            "type": "osascript",
            "script": 'tell app "System Events" to tell appearance preferences to set dark mode to not dark mode',
            "enabled": True,
        },
        "f17": {
            "name": "Paste Signature",
            "type": "paste",
            "text": "Sent from my FlipDeck",
            "enabled": True,
        },
        "f18": {
            "name": "Webhook Ping",
            "type": "http",
            "url": "https://httpbin.org/post",
            "method": "POST",
            "headers": {"Content-Type": "application/json"},
            "body": '{"source": "flipdeck", "key": "f18"}',
            "enabled": True,
        },
    }
}


def write_default_actions(path):
    with open(path, "w") as f:
        yaml.dump(DEFAULT_ACTIONS, f, default_flow_style=False, sort_keys=False)


def list_all_actions(actions_file, plugins_dir):
    """Print all configured actions."""
    executor = ActionExecutor(actions_file, plugins_dir)
    print(f"\nConfigured actions ({actions_file}):\n")
    for key, act in executor.get_all().items():
        status = "\u2705" if act.get("enabled", True) else "\u274c"
        print(
            f"  {status} {key.upper():>4}  {act.get('name', '?'):20}  [{act.get('type', '?')}]"
        )

    print(f"\nAvailable action types:")
    for t in sorted(executor.handlers.keys()):
        builtin = "(builtin)" if t in BUILTIN_HANDLERS else "(plugin)"
        print(f"  - {t} {builtin}")
    print()
