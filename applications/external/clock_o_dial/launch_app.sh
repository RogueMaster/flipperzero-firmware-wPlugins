#!/bin/bash
# Launch app and handle cleanup on exit

UFBT_CMD="ufbt"

# Function to stop the app
stop_app() {
    echo "Stopping app on Flipper..."
    # "loader close" is the real Flipper CLI command - it takes no app id (only one app can run
    # at a time), unlike the "app_close <id>" this used to send, which the CLI doesn't recognize.
    echo "loader close" | $UFBT_CMD cli 2>/dev/null || true
    exit 0
}

# Trap SIGINT and SIGTERM to stop app when task is cancelled
trap stop_app SIGINT SIGTERM

# Launch the app
$UFBT_CMD launch

# If launch exits, stop the app
stop_app
