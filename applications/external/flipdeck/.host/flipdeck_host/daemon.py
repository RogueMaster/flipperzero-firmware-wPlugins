"""FlipDeck daemon – listens for F13-F24 keypresses and executes actions."""

import os
import signal
import sys
import threading
import time

from .actions import ActionExecutor

# ── Key listener (cross-platform) ─────────────────────────

FKEY_RANGE = range(13, 25)  # F13..F24


def _listener_pynput(executor, stop_event):
    """macOS / generic listener using pynput."""
    from pynput import keyboard

    fkey_map = {}
    for n in FKEY_RANGE:
        attr = f"f{n}"
        key_obj = getattr(keyboard.Key, attr, None)
        if key_obj:
            fkey_map[key_obj] = f"f{n}"

    def on_press(key):
        if stop_event.is_set():
            return False
        if key in fkey_map:
            name = fkey_map[key]
            print(f"  \u2190 {name.upper()}")
            executor.execute(name)

    with keyboard.Listener(on_press=on_press) as listener:
        while not stop_event.is_set():
            time.sleep(0.2)
        listener.stop()


def _listener_evdev(executor, stop_event):
    """Linux listener using evdev (no X11 required)."""
    try:
        import evdev
        from evdev import ecodes
    except ImportError:
        print("  [!] evdev not installed. Falling back to pynput.")
        print("      Install with: pip install evdev")
        return _listener_pynput(executor, stop_event)

    # Find keyboard devices
    devices = [evdev.InputDevice(path) for path in evdev.list_devices()]
    keyboards = [d for d in devices if ecodes.EV_KEY in d.capabilities()]

    if not keyboards:
        print("  [!] No keyboard devices found. Try running with sudo.")
        return

    # F13=183, F14=184, ... F24=194
    fkey_codes = {183 + i: f"f{13 + i}" for i in range(12)}

    print(f"  Monitoring {len(keyboards)} keyboard device(s)")

    import select

    while not stop_event.is_set():
        r, _, _ = select.select(keyboards, [], [], 0.5)
        for dev in r:
            try:
                for event in dev.read():
                    if event.type == ecodes.EV_KEY and event.value == 1:  # key down
                        if event.code in fkey_codes:
                            name = fkey_codes[event.code]
                            print(f"  \u2190 {name.upper()}")
                            executor.execute(name)
            except Exception:
                pass


def create_listener(executor, stop_event):
    """Create the right listener for the current platform."""
    if sys.platform == "linux" and os.geteuid() == 0:
        return lambda: _listener_evdev(executor, stop_event)
    return lambda: _listener_pynput(executor, stop_event)


# ── Daemon ────────────────────────────────────────────────


def run_daemon(actions_file, plugins_dir=None, web_port=None):
    """Run the FlipDeck daemon (key listener + optional web server)."""
    print("FlipDeck Host v3.0")
    print("=" * 50)

    executor = ActionExecutor(actions_file, plugins_dir)

    actions = executor.get_all()
    active = sum(1 for a in actions.values() if a.get("enabled", True))
    print(f"  Actions: {active} active / {len(actions)} total")
    for key, act in actions.items():
        status = "\u2705" if act.get("enabled", True) else "\u274c"
        print(f"    {status} {key.upper():>4} \u2192 {act.get('name', '?')}")

    stop_event = threading.Event()

    def on_signal(sig, frame):
        print("\n  Shutting down...")
        stop_event.set()

    signal.signal(signal.SIGINT, on_signal)
    signal.signal(signal.SIGTERM, on_signal)

    # Start web server in background thread if requested
    web_thread = None
    if web_port:
        from .web import run_web_thread

        web_thread = run_web_thread(
            executor=executor,
            actions_file=actions_file,
            plugins_dir=plugins_dir,
            port=web_port,
            stop_event=stop_event,
        )
        print(f"  Web UI: http://localhost:{web_port}")

    # Start key listener
    print(f"\n  Listening for F13-F24 keypresses...")
    if sys.platform == "darwin":
        print("  (Accessibility permission required for Terminal/Python)")
    print("  Press Ctrl+C to quit.\n")

    listener_fn = create_listener(executor, stop_event)
    try:
        listener_fn()
    except KeyboardInterrupt:
        pass
    finally:
        stop_event.set()
        if web_thread and web_thread.is_alive():
            web_thread.join(timeout=2)

    print("  Goodbye!")
