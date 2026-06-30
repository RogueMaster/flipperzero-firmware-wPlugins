FlipWiFi is the companion app for the popular FlipperHTTP firmware. It allows you to scan and save Wi-Fi networks for use across all FlipperHTTP apps, set up captive portals, and send deauthentication attacks.

## Requirements
- Wi-Fi Developer Board, BW16, Raspberry Pi, or ESP32 device flashed with FlipperHTTP v2.0 or higher: https://github.com/jblanked/FlipperHTTP
- 2.4 GHz or 5 GHz Wi-Fi access point

## Connect Online
- Discord: https://discord.gg/5aN9qwkEc6
- YouTube: https://www.youtube.com/@jblanked
- Instagram: https://www.instagram.com/jblanked
- Other: https://www.jblanked.com/social/

## Features

- **Scan**: Discover nearby Wi-Fi networks and add them to your list.
- **Deauthentication**: Send deauthentication frames to disconnect clients from a selected network for testing or troubleshooting.
- **Captive Portal**: Create a captive portal with custom HTML and auto-input routing.
- **Saved Access Points**: View your saved networks, manually add new ones, or configure the Wi-Fi network to be used across all FlipperHTTP apps.

## Setup

FlipWiFi automatically allocates the necessary resources and initializes settings upon launch. If WiFi settings have been previously configured, they are loaded automatically for easy access. You can also edit the list of WiFi settings by downloading and modifying the "wifi_list.txt" file located in the "/SD/apps_data/flip_wifi/data" directory. To use the app:

1. **Flash the Wi-Fi Developer Board**: Follow the instructions to flash the Wi-Fi Dev Board with FlipperHTTP: https://github.com/jblanked/FlipperHTTP  
2. **Install the App**: Download FlipWiFi from the Flipper Lab.  
3. **Launch FlipWiFi**: Open the app on your Flipper Zero.  
4. Connect, review, and save Wi-Fi networks.

## Disclaimer
Please use this firmware responsibly. JBlanked and JBlanked, LLC disclaim all liability for any misuse of this firmware. You are solely responsible for your actions.