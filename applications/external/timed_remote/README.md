# Timed Remote

A Flipper Zero application that sends IR (infrared) commands after a configurable time delay.

## Features

- Browse and select IR signals from existing `.ir` files
- **Countdown Mode**: Set a timer (HH:MM:SS) and send the signal when it completes
- **Scheduled Mode**: Send the signal at a specific time of day
- **Repeat Options**: Off, Unlimited, or 1-99 repeats (countdown mode only)
- Live countdown display with repeat progress tracking

## Building

```sh
# Install requirements (first time only) and build the application
make

# Deploy to Flipper Zero connected via USB
make start

# Clean build artifacts
make clean
```

Output: `dist/timed_remote.fap`

## Usage

1. Launch "Timed Remote" from Apps menu
2. Browse to an IR file in `/ext/infrared/`
3. Select a signal from the file
4. Configure timer:
   - **Mode**: Countdown or Scheduled
   - **Time**: Hours, Minutes, Seconds
   - **Repeat**: Off, Unlimited, or specific count (countdown only)
5. Start the timer
6. Press Back to cancel at any time

## Requirements

- Flipper Zero firmware with API version 87.1+
- IR signal files in `/ext/infrared/` directory

## License

See LICENSE for license information.
