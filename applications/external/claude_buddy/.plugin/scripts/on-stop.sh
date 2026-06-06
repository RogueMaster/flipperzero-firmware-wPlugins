#!/bin/bash
set -euo pipefail

# Stop hook: notify Flipper when Claude finishes a turn, with tool usage summary

SOCKET="/tmp/claude-flipper-bridge.sock"
STATS="/tmp/claude-flipper-turn-stats.json"

if [ ! -S "$SOCKET" ]; then
    rm -f "$STATS"
    exit 0
fi

# Skip "Turn complete" if a direct Flipper notify was sent this turn
SKIP_FLAG="/tmp/claude-flipper-skip-stop.flag"
if [ -f "$SKIP_FLAG" ]; then
    rm -f "$SKIP_FLAG" "$STATS"
    exit 0
fi

# Read hook payload from stdin
PAYLOAD=$(cat)

# Check if interrupted
INTERRUPTED=$(echo "$PAYLOAD" | python3 -c "
import json, sys
try:
    data = json.load(sys.stdin)
    print('true' if data.get('interrupted', False) else 'false')
except Exception:
    print('false')
" 2>/dev/null)

SUBTEXT=""
if [ -f "$STATS" ]; then
    # Build compact summary from tool stats: "3 Edit 2 Bash"
    SUBTEXT=$(python3 -c "
import json, sys
try:
    stats = json.load(open('$STATS'))
    parts = sorted(stats.items(), key=lambda x: -x[1])
    summary = ' '.join(f'{v} {k}' for k, v in parts)
    print(summary[:21] if summary else '')
except Exception:
    print('')
" 2>/dev/null)
    rm -f "$STATS"
fi

if [ "$INTERRUPTED" = "true" ]; then
    echo "{\"action\":\"notify\",\"sound\":\"interrupt\",\"vibro\":true,\"text\":\"Interrupted\",\"subtext\":\"$SUBTEXT\"}" \
        | nc -U "$SOCKET" 2>/dev/null &
else
    echo "{\"action\":\"notify\",\"sound\":\"success\",\"vibro\":true,\"text\":\"Turn complete\",\"subtext\":\"$SUBTEXT\"}" \
        | nc -U "$SOCKET" 2>/dev/null &
fi

exit 0
