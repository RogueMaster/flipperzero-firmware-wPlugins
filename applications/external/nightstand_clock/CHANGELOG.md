# Changelog

## 1.1

- Fix the 12-hour clock showing midnight as `00:xx AM` instead of `12:xx AM`
- Fix the brightness control so the red LED nightlight stays reachable from every starting brightness: round the inherited system brightness to the nearest step and clamp brightness to 0-100 (previously levels such as 35/45/65/70/90/95% started off-grid, so Down skipped 0 and the nightlight could never be switched on)
- Restore the notification settings before releasing the notification record when exiting
- Free the view_port and notification record (and restore the display settings) if the periodic timer fails to allocate on startup

## 1.0

- Initial release: overnight clock with screen-brightness control, red LED nightlight, and a stopwatch
