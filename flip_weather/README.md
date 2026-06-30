# FlipWeather

FlipWeather is an innovative app for the Flipper Zero that uses WiFi to fetch GPS and weather information, making your Flipper Zero even more versatile. It leverages the FlipperHTTP flash for the WiFi Devboard, originally introduced in the WebCrawler app: https://github.com/jblanked/WebCrawler-FlipperZero/tree/main/assets/FlipperHTTP

## Requirements

- WiFi Dev Board or Raspberry Pi Pico W for Flipper Zero with FlipperHTTP Flash: https://github.com/jblanked/FlipperHTTP
- WiFi Access Point


## Features

- **GPS Information**: Provides latitude, longitude, and other relevant GPS data.
- **Weather Updates**: Displays weather details like temperature, precipitation, and more.
- **WiFi Settings Management**: Allows configuration of WiFi settings (SSID and password) for current and future features that utilize network connectivity.

## Navigation

- **Main Menu**: The central hub to access all of FlipWeather’s features:
  - **Weather**: Displays the current weather conditions for your location.
  - **GPS**: Shows your GPS coordinates and other location-based information.
  - **About**: Provides details about the app, including version information.
  - **WiFi Settings**: Lets you view and configure saved WiFi settings.

## Setup

FlipWeather automatically allocates necessary resources and initializes settings when launched. If WiFi settings have been previously configured, they are loaded automatically for easy access.

## How to Use

1. **Flash the WiFi Devboard**: Follow the instructions to flash the WiFi Devboard with FlipperHTTP: https://github.com/jblanked/FlipperHTTP/
2. **Install the App**: Download FlipWeather from the App Store.
3. **Launch FlipWeather**: Open the app on your Flipper Zero.
4. **Explore the Features**:
   - Check the **Weather** section for current weather information.
   - View **GPS** data to see your location details.
   - Use **WiFi Settings** to manage your network configurations.
   - Visit **About** for app information and version history.

## Known Issues

1. **GPS Screen Delay**: Occasionally, the GPS or Weather screen may get stuck on "Loading Data".
   - If it takes longer than 10 seconds, restart your Flipper Zero.