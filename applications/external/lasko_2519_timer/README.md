# Lasko 2519 Timer for Flipper Zero

![Icon](icon.png)

An automated timer application for the Lasko 2519 Tower Fan (and compatible models). This app uses the Flipper Zero's IR blaster to cycle the fan power ON and OFF at configurable intervals.

## Features

-   **Automated Cycling**: Automatically toggles the fan power.
-   **Configurable Timers**: Set your desired "ON" duration and "OFF" duration directly on the device.
    -   Default: 20 minutes ON / 40 minutes OFF.
-   **Status Display**: Shows current state (Fan ON/OFF) and time remaining.
-   **Custom Icon**: stylized sand clock and fan icon.

## Usage

1.  **Start**: Open `Lasko 2519 Timer` from the `Infrared` apps menu.
2.  **Setup**:
    -   Ensure your fan is currently **OFF**.
    -   (Optional) Adjust the "On Time" and "Off Time" in the menu using Left/Right buttons.
3.  **Run**:
    -   Select **Start Automation** and press OK.
    -   Point the Flipper Zero at the fan.
    -   The app will turn the fan ON and begin the cycle.
4.  **Stop**: Press the **Back** button to return to the menu or exit.

## Installation

### From Flipper Apps Catalog
*(Instuctions if accepted to catalog)*
Install "Lasko 2519 Timer" via the Flipper Mobile App.

### Manual Build
1.  Clone this repository.
2.  Connect Flipper Zero via USB.
3.  Run `ufbt launch` to compile and install.

## Credits

-   **Author**: LN4CY
-   **License**: MIT
