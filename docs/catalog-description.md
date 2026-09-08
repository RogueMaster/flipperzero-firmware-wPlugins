# Flipper USB Internet Bridge

Use a Mac's existing internet connection from Flipper Zero through an explicitly authorized USB connection. The Flipper does not join Wi-Fi, receive the Wi-Fi password, or appear as a USB network adapter.

The Flipper app sends bounded requests over USB CDC. A native macOS menu bar helper validates them, performs HTTPS requests, and streams responses back in small chunks.

## Features

- Test the USB bridge connection
- Fetch sample text and the current date and time
- Search English Wikipedia
- Check weather by location
- Read National Today entries
- View the current ISS position
- Find internet radio stations by country and play MP3 audio on the Flipper speaker
- Send a custom HTTPS GET request

## Requirements

- Flipper Zero with a microSD card
- macOS 13 or later
- The companion macOS helper from the [project repository](https://github.com/mete888/flipper_usb_to_wifi)
- A data-capable USB cable

The helper does not require administrator access. Internet access is disabled until the user chooses **Allow Once** or **Always Allow** on the Mac.

## Security

Only HTTPS is allowed. Localhost, private networks, link-local addresses, unsafe redirects, shared cookies, stored credentials, and non-HTTPS schemes are blocked. USB disconnection cancels active network work immediately.
