# Ducobox

Control a Ducobox ventilation unit with your Flipper Zero. The app learns your own Ducobox remote, then sends Speed 1, Speed 2, Speed 3 and Auto commands over Sub-GHz.

## Install

1. Install **Ducobox** from the Apps Catalog, under **Sub-GHz**.
2. Launch it from **Apps → Sub-GHz → Ducobox**.

## First use

1. Start the app. It scans the Duco channels and waits for a remote frame.
2. Press any button on your paired Ducobox remote when the app asks. The app learns the remote and stores the template on the Flipper.
3. The remote screen appears.

## Controls

- **Left** — Speed 1
- **Up** — Speed 2
- **Right** — Speed 3
- **Down** — Auto
- **OK** — Config
- **Back** — Exit

Tap a speed key to send a temporary 15-minute command. Tap the same key again within 10 seconds to extend it to 30 or 45 minutes. Hold a speed key for 5 seconds to set it permanently. Auto is always sent as a temporary command.

**Config** lets you re-learn the remote or delete the saved template.

## Notes

- Learn the remote that is paired with your Ducobox. The app copies that remote's network identity.
- Make sure 868.3 MHz transmission is permitted in your region. The Flipper's region setting is enforced.
- This is an unofficial experiment, not a Duco product.

Build instructions, protocol details and troubleshooting are in the project README on GitHub.
