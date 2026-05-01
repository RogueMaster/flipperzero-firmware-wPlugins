# Firmware Distribution Submission Guide

## Momentum Firmware (BLE build)

The BLE build requires custom firmware APIs (`ble_profile` lib). Submit to get included in every Momentum release.

### Steps

1. Open an issue at [Next-Flip/Momentum-Apps](https://github.com/Next-Flip/Momentum-Apps/issues/new)
2. Title: "Add Claupper (Claude Code remote control)"
3. Include:
   - Source repo: `https://github.com/Wet-wr-Labs/claupper`
   - App ID: `claude_remote_ble`
   - Category: `Bluetooth`
   - Description: One-handed Flipper Zero remote for Claude Code. 5 buttons control approve/decline/skip/enter/voice. Includes offline manual and quiz.
   - License: MIT
4. Alternatively, submit a PR adding the app as a git subtree

The Momentum team handles git subtree integration. Once merged, the app ships with every Momentum firmware update.

## Unleashed Firmware (BLE build)

### Steps

1. Open an issue at [xMasterX/all-the-plugins](https://github.com/xMasterX/all-the-plugins/issues/new)
2. Title: "Add Claupper BLE remote control"
3. Include same info as above
4. Or submit a PR to the Extra Pack

## Official Flipper App Catalog (USB build only)

See separate guide. Only the USB build (`claude_remote_usb`) qualifies since it uses stock firmware APIs.

## Other Distribution

- **awesome-flipperzero**: PR to [djsime1/awesome-flipperzero](https://github.com/djsime1/awesome-flipperzero) under Applications
- **Flipper Discord**: Share in #showcase or #apps channel
- **r/flipperzero**: Post with demo video/screenshots
- **RogueMaster**: Issue at their firmware repo
