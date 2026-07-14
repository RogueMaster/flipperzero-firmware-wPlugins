"""FlipDeck service installer – macOS LaunchAgent + Linux systemd user service."""

import os
import subprocess
import sys

SERVICE_NAME = "com.flipdeck.host"


def install_service():
    """Install FlipDeck as a background service."""
    if sys.platform == "darwin":
        _install_macos()
    elif sys.platform == "linux":
        _install_linux()
    else:
        print(f"  [!] Service installation not supported on {sys.platform}")
        print("      Run 'flipdeck start' manually instead.")


def _install_macos():
    """Install as macOS LaunchAgent (runs at login)."""
    flipdeck_bin = _find_binary()
    plist_dir = os.path.expanduser("~/Library/LaunchAgents")
    plist_path = os.path.join(plist_dir, f"{SERVICE_NAME}.plist")
    log_dir = os.path.expanduser("~/Library/Logs/FlipDeck")

    os.makedirs(plist_dir, exist_ok=True)
    os.makedirs(log_dir, exist_ok=True)

    plist = f"""<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>{SERVICE_NAME}</string>
    <key>ProgramArguments</key>
    <array>
        <string>{flipdeck_bin}</string>
        <string>start</string>
        <string>--web</string>
    </array>
    <key>RunAtLoad</key>
    <true/>
    <key>KeepAlive</key>
    <true/>
    <key>StandardOutPath</key>
    <string>{log_dir}/flipdeck.log</string>
    <key>StandardErrorPath</key>
    <string>{log_dir}/flipdeck.err</string>
    <key>EnvironmentVariables</key>
    <dict>
        <key>PATH</key>
        <string>/usr/local/bin:/usr/bin:/bin:/opt/homebrew/bin</string>
    </dict>
</dict>
</plist>"""

    with open(plist_path, "w") as f:
        f.write(plist)

    # Load the service
    subprocess.run(["launchctl", "unload", plist_path], capture_output=True)
    subprocess.run(["launchctl", "load", plist_path], check=True)

    print(f"  \u2713 Installed macOS LaunchAgent")
    print(f"    Plist:  {plist_path}")
    print(f"    Logs:   {log_dir}/")
    print(f"    Web UI: http://localhost:7433")
    print()
    print(f"  Commands:")
    print(f"    Stop:    launchctl unload {plist_path}")
    print(f"    Start:   launchctl load {plist_path}")
    print(f"    Remove:  launchctl unload {plist_path} && rm {plist_path}")
    print()
    print("  \u26a0  Grant Accessibility permission:")
    print("     System Settings > Privacy & Security > Accessibility")
    print(f"     Add: {flipdeck_bin}")


def _install_linux():
    """Install as Linux systemd user service."""
    flipdeck_bin = _find_binary()
    unit_dir = os.path.expanduser("~/.config/systemd/user")
    unit_path = os.path.join(unit_dir, "flipdeck.service")

    os.makedirs(unit_dir, exist_ok=True)

    unit = f"""[Unit]
Description=FlipDeck Host – USB HID Macro Pad Companion
After=default.target

[Service]
Type=simple
ExecStart={flipdeck_bin} start --web
Restart=on-failure
RestartSec=5
Environment=DISPLAY=:0

[Install]
WantedBy=default.target
"""

    with open(unit_path, "w") as f:
        f.write(unit)

    subprocess.run(["systemctl", "--user", "daemon-reload"], check=True)
    subprocess.run(["systemctl", "--user", "enable", "flipdeck"], check=True)
    subprocess.run(["systemctl", "--user", "start", "flipdeck"], check=True)

    print(f"  \u2713 Installed systemd user service")
    print(f"    Unit:   {unit_path}")
    print(f"    Web UI: http://localhost:7433")
    print()
    print(f"  Commands:")
    print(f"    Status:  systemctl --user status flipdeck")
    print(f"    Stop:    systemctl --user stop flipdeck")
    print(f"    Logs:    journalctl --user -u flipdeck -f")
    print(f"    Remove:  systemctl --user disable flipdeck && rm {unit_path}")


def stop_service():
    """Stop the FlipDeck background service."""
    if sys.platform == "darwin":
        plist = os.path.expanduser(f"~/Library/LaunchAgents/{SERVICE_NAME}.plist")
        if not os.path.exists(plist):
            print("  [!] Service not installed. Nothing to stop.")
            return
        subprocess.run(["launchctl", "unload", plist], capture_output=True)
        print("  \u2713 Service stopped")
    elif sys.platform == "linux":
        subprocess.run(["systemctl", "--user", "stop", "flipdeck"], capture_output=True)
        print("  \u2713 Service stopped")
    else:
        print("  [!] Use Ctrl+C to stop the foreground process")


def service_status():
    """Show FlipDeck service status."""
    if sys.platform == "darwin":
        plist = os.path.expanduser(f"~/Library/LaunchAgents/{SERVICE_NAME}.plist")
        if not os.path.exists(plist):
            print("  Service: not installed")
            print(f"  Install: flipdeck install")
            return
        r = subprocess.run(
            ["launchctl", "list", SERVICE_NAME], capture_output=True, text=True
        )
        if r.returncode == 0:
            # Parse PID from output
            lines = r.stdout.strip().split("\n")
            pid = None
            for line in lines:
                if "PID" in line:
                    parts = line.split()
                    for i, p in enumerate(parts):
                        if p == "PID":
                            # next non-header value
                            continue
                        try:
                            pid = int(p)
                            break
                        except ValueError:
                            continue
            if pid and pid > 0:
                print(f"  Service: \u2705 running (PID {pid})")
            else:
                # Try simpler parsing
                print(f"  Service: \u2705 loaded")
            print(f"  Plist:   {plist}")
        else:
            print(f"  Service: \u274c stopped")
            print(f"  Start:   flipdeck start-service")

        log_dir = os.path.expanduser("~/Library/Logs/FlipDeck")
        log = os.path.join(log_dir, "flipdeck.log")
        err = os.path.join(log_dir, "flipdeck.err")
        print(f"  Logs:    {log}")
        if os.path.exists(err) and os.path.getsize(err) > 0:
            print(f"  Errors:  {err}")

        print(f"  Web UI:  http://localhost:7433")

    elif sys.platform == "linux":
        r = subprocess.run(
            ["systemctl", "--user", "is-active", "flipdeck"],
            capture_output=True,
            text=True,
        )
        active = r.stdout.strip() == "active"
        if active:
            print(f"  Service: \u2705 running")
        else:
            print(f"  Service: \u274c stopped")

        unit = os.path.expanduser("~/.config/systemd/user/flipdeck.service")
        installed = os.path.exists(unit)
        print(f"  Installed: {'yes' if installed else 'no'}")
        print(f"  Web UI:  http://localhost:7433")
        print(f"  Logs:    journalctl --user -u flipdeck -f")
    else:
        print("  [!] Service management not supported on this platform")


def start_service():
    """Start the FlipDeck background service (if installed)."""
    if sys.platform == "darwin":
        plist = os.path.expanduser(f"~/Library/LaunchAgents/{SERVICE_NAME}.plist")
        if not os.path.exists(plist):
            print("  [!] Service not installed. Run: flipdeck install")
            return
        subprocess.run(["launchctl", "load", plist], capture_output=True)
        print("  \u2713 Service started")
        print("  Web UI: http://localhost:7433")
    elif sys.platform == "linux":
        subprocess.run(
            ["systemctl", "--user", "start", "flipdeck"], capture_output=True
        )
        print("  \u2713 Service started")
        print("  Web UI: http://localhost:7433")


def uninstall_service():
    """Remove the FlipDeck background service."""
    if sys.platform == "darwin":
        plist = os.path.expanduser(f"~/Library/LaunchAgents/{SERVICE_NAME}.plist")
        if not os.path.exists(plist):
            print("  [!] Service not installed.")
            return
        subprocess.run(["launchctl", "unload", plist], capture_output=True)
        os.remove(plist)
        print(f"  \u2713 Removed LaunchAgent: {plist}")
        print("  (Logs in ~/Library/Logs/FlipDeck/ were kept)")
    elif sys.platform == "linux":
        subprocess.run(["systemctl", "--user", "stop", "flipdeck"], capture_output=True)
        subprocess.run(
            ["systemctl", "--user", "disable", "flipdeck"], capture_output=True
        )
        unit = os.path.expanduser("~/.config/systemd/user/flipdeck.service")
        if os.path.exists(unit):
            os.remove(unit)
        subprocess.run(["systemctl", "--user", "daemon-reload"], capture_output=True)
        print(f"  \u2713 Removed systemd service")


def show_logs():
    """Show FlipDeck service logs."""
    if sys.platform == "darwin":
        log = os.path.expanduser("~/Library/Logs/FlipDeck/flipdeck.log")
        err = os.path.expanduser("~/Library/Logs/FlipDeck/flipdeck.err")
        if os.path.exists(log):
            print(f"  === {log} (last 30 lines) ===\n")
            subprocess.run(["tail", "-30", log])
            if os.path.exists(err) and os.path.getsize(err) > 0:
                print(f"\n  === {err} (last 10 lines) ===\n")
                subprocess.run(["tail", "-10", err])
        else:
            print("  No logs yet. Is the service running?")
            print(f"  Expected: {log}")
    elif sys.platform == "linux":
        subprocess.run(
            ["journalctl", "--user", "-u", "flipdeck", "-n", "50", "--no-pager"]
        )
    else:
        print("  [!] Logs not available on this platform")


def _find_binary():
    """Find the flipdeck binary path."""
    # Check if installed as a pip package
    import shutil

    path = shutil.which("flipdeck")
    if path:
        return path
    # Fallback: use python -m
    return f"{sys.executable} -m flipdeck_host.cli"
