# Changelog

## Version 1.1.0 - 2026-04-24

**Added**
- Word-wrap for long chat messages using screen math (128x64 px)
- Up to 6 line slots visible at once (FontKeyboard 8px height)
- Optional "Send to Flipper Zero" checkbox - monitor TikTok chat without the device
- Text-to-Speech (TTS) with voice selection and speed control
- Keepalive packets every 15 seconds to prevent Flipper timeout
- Deduplication of repeated messages within 3 second window
- 3-second backlog discard on TikTok connect
- Letter T icon (10x10 px)

**Changed**
- Removed all developer comments from source files
- Config file excluded from git via .gitignore

## Version 1.0.0 - 2026-04-01

**Added**
- Initial release
- Real-time FlipTok Live chat display on Flipper Zero via BLE
- Chat, gift, follow event handling
- LED and sound notifications per event type
- Scrollable message history (8 messages circular buffer)
- Splash screen with FlipTok logo
- Python GUI server with auto-dependency installation
- EulerStream API key support
- Auto-reconnect on BLE disconnect
- Transliteration of non-ASCII characters (PL, DE, FR, etc.)
