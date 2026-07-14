"""
FlipDeck Plugin Example: Notify

Sends a desktop notification when triggered.
Works on macOS (osascript) and Linux (notify-send).

To install: copy this file to ~/.config/flipdeck/plugins/ (Linux)
            or ~/Library/Application Support/FlipDeck/plugins/ (macOS)

Then add an action in actions.yaml:
  f18:
    name: "Alert Me"
    type: notify
    title: "FlipDeck"
    message: "Button pressed!"
    enabled: true
"""

import subprocess
import sys

# Required: tells FlipDeck which action type this plugin handles
ACTION_TYPE = "notify"


def execute(cfg):
    """Send a desktop notification.

    Args:
        cfg: dict with keys 'title' and 'message'

    Returns:
        True on success
    """
    title = cfg.get("title", "FlipDeck")
    message = cfg.get("message", "Action triggered")

    if sys.platform == "darwin":
        script = f'display notification "{message}" with title "{title}"'
        subprocess.run(["osascript", "-e", script], timeout=5, capture_output=True)
    elif sys.platform == "linux":
        subprocess.run(["notify-send", title, message], timeout=5, capture_output=True)
    else:
        print(f"  [{title}] {message}")

    return True
