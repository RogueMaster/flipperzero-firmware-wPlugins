# UART Logger

Reads UART data on the GPIO header and logs it to the SD card.

## Wiring (3.3 V logic only!)
- **USART:** device TX → pin 14 (RX), device RX ← pin 13 (TX)
- **LPUART:** device TX → pin 16 (RX), device RX ← pin 15 (TX)
- **GND** → pin 8 or 18

## Settings
- **Baudrate:** 9600 … 921600 (default 115200)
- **Pins TX/RX:** 13/14 (USART) or 15/16 (LPUART)
- **Mode:** Text or Hex (16 bytes per line in file)
- **Save in:** SD root or any top-level folder
- **Folder:** log folder name (default uart_logs)
- **Start:** open the log screen

## Log screen
Recording starts automatically. The file is /ext/uart_logs/log_YYYYMMDD_HHMMSS.txt by default.
- **OK:** pause / resume recording
- **Up / Down:** scroll history
- **Back:** stop and ask whether to keep the file, then show where it was saved

The status line shows the received byte count and REC / PAUSE / SD ERR.
