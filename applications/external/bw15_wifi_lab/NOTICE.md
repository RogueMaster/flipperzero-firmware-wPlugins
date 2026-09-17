# NOTICE — Flipper application attributions

5Ghost WiFi Lab (by PINGEQUA) builds upon the following open-source projects.
Original copyright and license notices are retained as required.

## Flipper FAP (MIT)

Source for this Flipper application is published in this repository.

- **gorebrau/delfyRTL** — the Flipper application (UART control of the BW16) is
  based on delfyRTL's flipper-app, which itself derives from jamisonderek's MIT
  UART example. delfyRTL's firmware subtree is GPLv3 and is NOT used by the FAP —
  the FAP talks to the module only over UART.
  https://github.com/gorebrau/delfyRTL

- **jamisonderek/flipper-zero-tutorials** — MIT — UART helper / ring-buffer pattern
  https://github.com/jamisonderek/flipper-zero-tutorials
  uart_helper.c/.h and ring_buffer.c/.h originate from the UART example.

The FAP contains no companion-module firmware and does not link GPLv3 firmware.
