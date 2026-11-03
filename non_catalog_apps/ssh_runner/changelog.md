v0.3:
- The one-time move of a pre-release board key to the SD card is gone; the app no longer asks the board for it.

v0.2:
- First version in the Apps Catalog.
- SSH from the Flipper over LAN or 2.4 GHz WiFi through the SSH Runner add-on board (ESP32-S3 with a W5500).
- Quick Connect, saved hosts with their own commands, key and host-key policy, Recent, and scripts from the SD card.
- Password or key authentication: Ed25519, ECDSA and RSA keys from the SD card or generated on the board.
- Host-key pinning per host: prompt, strict or off.
- Network: DHCP or static addresses, WiFi scan and join, ping through LAN and WiFi.
- Output with its exit code and run time; very long output keeps its beginning and end.
