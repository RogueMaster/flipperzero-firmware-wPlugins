# Subscriber Counter

Shows the live subscriber count of a YouTube channel on your Flipper Zero and refreshes it automatically. Subscribers, views and videos are on three cards; switch with Left and Right.

## Requirements

- A computer running the **Flipper Internet Bridge** desktop helper: https://github.com/mete888/flipper_internet_bridge
- Or, in beta, a Wi-Fi board with **FlipperHTTP**: https://github.com/jblanked/FlipperHTTP
- A free **YouTube Data API v3** key: https://github.com/TheProductAgency/flipper-subscriber-counter/blob/main/docs/api-key.md
- Connection over **USB, Bluetooth or Wi-Fi**

## Setup

1. Start the app once. It creates the file apps_data/sub_counter/config.txt on the SD card.
2. Open that file with qFlipper and fill in **api_key** and **channel** (your @handle or the channel ID).
3. Start the desktop helper on your computer, open the app, choose USB or Bluetooth and approve the connection.

## Controls

- **Left / Right**: subscribers, views, videos
- **Up**: info sheet with last and next update
- **Down**: settings
- **OK**: refresh now
- **Back**: close, then exit

## Settings

- Sound, vibration and LED for new subscribers, each on or off
- Keep the backlight on
- Update interval from 1 minute to 1 hour
- Language: English or German
- Font style: Inter, the Flipper's built-in fonts or a mix
- Connection: desktop helper or Wi-Fi board

## Notes

- YouTube rounds public subscriber counts above 1,000 to three significant figures.
- One update uses 1 of the 10,000 free API units per day.
- Not affiliated with YouTube or Google.

Source code and full guide: https://github.com/TheProductAgency/flipper-subscriber-counter
