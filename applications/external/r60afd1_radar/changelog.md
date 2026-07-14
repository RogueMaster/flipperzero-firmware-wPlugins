v0.1:
- Initial release
- UART async RX via furi_hal_serial on USART1 (115200 8N1)
- Displays: presence, motion, fall alarm, distance, breathing rate, heart rate
- Body movement signal-strength bar (6 bars)
- Presence query on startup for instant correct state
- Graceful UART busy error screen
- PC-side sniffer script (sniffer.py)
- Tested on OFW 1.4.3 / API 87.1
