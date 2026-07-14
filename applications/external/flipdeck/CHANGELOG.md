# Changelog

## 3.0.0

- **Multi-page support** – up to 8 configurable pages with named headers
- **Page navigation** – swipe left/right at grid edge, page dots indicator
- **Extended keys** – F13-F24 support (12 custom keys)
- **Brightness control** – BRTUP/BRTDN media keys
- **Modular action system** – 9 built-in action types
- **Plugin architecture** – drop-in `.py` plugins for custom actions
- **Background daemon** – macOS LaunchAgent + Linux systemd
- **Web UI** – action editor, config upload, Flipper status
- **Cross-platform host** – macOS + Linux support
- **pip installable** – `pip install -e .` + `flipdeck` CLI
- **Flipper App Catalog ready** – proper manifest structure

## 2.0.0

- USB HID approach (replaced serial/log)
- Native media keys without host script
- 2×3 button grid with haptic feedback

## 1.0.0

- Initial concept with USB serial/FURI_LOG
