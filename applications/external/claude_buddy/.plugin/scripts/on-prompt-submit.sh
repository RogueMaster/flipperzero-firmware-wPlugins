#!/bin/bash
set -euo pipefail

# UserPromptSubmit hook: show "Thinking..." on Flipper when user sends a prompt

SOCKET="/tmp/claude-flipper-bridge.sock"

if [ ! -S "$SOCKET" ]; then
    exit 0
fi

# Refresh the active input target from the session that just submitted a prompt.
python3 "${CLAUDE_PLUGIN_ROOT}/scripts/session-target.py" register_target "$SOCKET" >/dev/null 2>&1 || true

echo '{"action":"display","text":"Thinking...","subtext":""}' \
    | nc -U "$SOCKET" 2>/dev/null &

exit 0
