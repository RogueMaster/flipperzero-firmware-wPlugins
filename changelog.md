v3.0:
Added a Bluetooth LE transport alongside the existing UART link, selectable from Settings. The Flipper's radio can only act as a peripheral, so the mesh node connects to the Flipper rather than the other way around, which requires a Meshtastic build carrying the ZeroMesh link module; UART remains the default and works with stock node firmware. Added a vector map page that renders Mapbox Vector Tiles from a PMTiles archive on the SD card, with panning, zoom, street and place labels, and markers for roster nodes reporting a GPS position. Position packets are now decoded into the roster instead of being discarded. Map archives must be built uncompressed because the Flipper firmware has no gzip; tools/build_pmtiles.py packs a z/x/y tile tree into a suitable archive.

v2.1:
Fixed sender IDs overlapping the previous message bubble in the Messages feed (per-message row height now matches the drawn name+bubble in both scroll and wrap modes). Added a periodic phone-API heartbeat so the node keeps forwarding live messages over the serial link instead of only delivering the initial config/node-info burst and then going quiet. Encrypted packets that can't be decoded are now logged instead of silently dropped. Verified build and operation against the latest Momentum firmware (API 87.1).

v2.0:
Added settings persistence to SD card (/ext/zeromesh/settings.cfg). Added multi-channel support with up to 8 channels, cycle with long-press OK on Messages page. Expanded ringtones from 7 to 19 (added Nokia, Descend, Bounce, Alert, Pulse, Siren, Beep3, Trill, Mario, LevelUp, Metric, Minimal). Added scroll speed and FPS controls. Added scroll/wrap toggle for long messages. Sender IDs now shown above message bubbles. Fixed message overlap and bubble rendering. Fixed strtok compilation error.

v1.1:
Fixed broadcast messages staying in the main Messages feed. Fixed direct messages being isolated to per-node Chat view in Roster. Added unread DM indicator (!) next to node IDs in Roster. Notification flags clear automatically on entering chat view. Added echo protection to prevent sent messages re-appearing as incoming.

v1.0:
Initial release. Live node roster with automatic discovery. Direct messaging to individual nodes. Telemetry monitoring with SNR, RSSI, battery voltage. System log page for raw serial traffic. Haptic and LED notifications on incoming traffic.
