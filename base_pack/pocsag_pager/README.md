# Flipper POCSAG Pager (RX & TX)

## Credits
- Protocol RX implementation by @Shmuma + 512/2400 baud support by @htotoo
- TX encoder, UI, presets, and maintenance by @xMasterX
- Keyboard module adapted from [ESP32 WiFi Marauder Companion](https://github.com/0xchocolate/flipperzero-wifi-marauder)
- Icons by @Svaarich
- Originally based on [Weather Station](https://github.com/flipperdevices/flipperzero-firmware/tree/dev/applications/plugins/weather_station) from OFW

## Features
- **Receive & Transmit POCSAG** (CCIR Rec. 584) at **512, 1200, and 2400 baud** on CC1101-supported frequencies (internal and external CC1101)
- **Message types (TX & RX):** Alphanumeric (7-bit ASCII), Numeric (BCD), Tone Only (Alert 1), and Alert 2
- **Polarity (TX):** Normal (standard CCIR 584) and Inverted
- **2-FSK presets built-in:**
  - RX: `FM95` (2-FSK ±9.5 kHz deviation)
  - TX: 2-FSK ±4.76 kHz deviation via async GDO0 DMA modulation
- **Regional TX check:** respects Flipper regional TX restrictions on Official Firmware (OFW) and frequency limits on Unleashed
- **Live TX progress bar** with Cancel and Resend
- **Persistent settings, presets & RX cache on microSD (`/ext/apps_data/pocsag_pager/`):**
  - Last used frequency and TX fields: `/ext/apps_data/pocsag_pager/settings.txt`
  - Named RIC & message presets: `/ext/apps_data/pocsag_pager/presets/<name>.txt`
  - Offloaded RX long messages cache: `/ext/apps_data/pocsag_pager/rx_cache/`

Default frequency is set to DAPNET (`439.9875 MHz` / `439987500 Hz`).

## Adding Custom RX Frequencies & Modulations
To add custom frequencies or presets to the selector list, create `/ext/apps_data/pocsag_pager/subghz_settings.txt` (legacy `/ext/pocsag/settings.txt` is also supported as fallback) and copy the contents of [setting_user.example](https://github.com/flipperdevices/flipperzero-firmware/blob/dev/applications/main/subghz/resources/subghz/assets/setting_user.example) into it.

- Lines starting with `#` are comments and ignored — remove `#` to enable a line (e.g. `Frequency: 433000000`).
- Custom frequencies appear at the end of the frequency list (`Default list > Custom list`).
- In **Send message**, press **OK** on `Freq (OK:edit)` to enter any custom frequency directly in MHz (e.g. `439.9875`) or Hz (`439987500`).

All contributions are welcome!
