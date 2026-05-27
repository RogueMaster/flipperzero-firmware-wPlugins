#!/bin/bash

# SessionEnd hook: notify Flipper of disconnect, then stop the bridge
# when the last session ends (reference-counted).

SOCKET="/tmp/claude-flipper-bridge.sock"
PIDFILE="/tmp/claude-flipper-bridge.pid"
REFCOUNT_FILE="/tmp/claude-flipper-bridge.refcount"

# Read hook payload from stdin and extract the session end reason.
PAYLOAD=$(cat)
REASON=$(echo "$PAYLOAD" | python3 -c '
import json, sys
REASONS = {
    "clear": "Cleared",
    "resume": "Switched session",
    "logout": "Logged out",
    "prompt_input_exit": "User exited",
    "bypass_permissions_disabled": "Bypass perms off",
    "other": "Disconnected",
}
try:
    data = json.load(sys.stdin)
    raw = data.get("reason") or ""
    print(REASONS.get(raw, raw or "Disconnected")[:21])
except Exception:
    print("Disconnected")
' 2>/dev/null)

# Decrement session reference counter
COUNT=$(cat "$REFCOUNT_FILE" 2>/dev/null || echo 1)
COUNT=$((COUNT - 1))
if [ "$COUNT" -lt 0 ]; then COUNT=0; fi
echo "$COUNT" > "$REFCOUNT_FILE"

if [ -S "$SOCKET" ]; then
    python3 "${CLAUDE_PLUGIN_ROOT}/scripts/session-target.py" release_target "$SOCKET" >/dev/null 2>&1 || true
    echo '{"action":"claude_disconnect"}' \
        | nc -U "$SOCKET" 2>/dev/null || true
fi

# Only stop bridge when last session ends
if [ "$COUNT" -le 0 ]; then
    if [ -S "$SOCKET" ]; then
        echo "{\"action\":\"notify\",\"sound\":\"session_end\",\"vibro\":true,\"text\":\"Session End\",\"subtext\":\"$REASON\"}" \
            | nc -U "$SOCKET" 2>/dev/null || true
        # Give bridge time to deliver the message to Flipper
        sleep 0.5
    fi

    if [ -f "$PIDFILE" ]; then
        PID=$(cat "$PIDFILE")
        if kill -0 "$PID" 2>/dev/null; then
            kill "$PID"
        fi
        rm -f "$PIDFILE"
    fi
    rm -f "$REFCOUNT_FILE"
fi

exit 0
