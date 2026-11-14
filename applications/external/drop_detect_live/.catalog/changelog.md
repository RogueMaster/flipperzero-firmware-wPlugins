## 0.3
- Removed unneeded files.

## 0.2
- Reset each view's animation + timer on every menu entry so the REC
  clock and Drop countdown start fresh every visit.
- Fixed a phase-transition bug that was skipping the Drop falling
  animation entirely.
- Removed per-frame UART logging that was causing visible jitter at 30 fps.
- Restart icon animations from frame 0 on view entry.

## 0.1
- Initial release. Submenu + four capture views, IMU init, CSV writer.
