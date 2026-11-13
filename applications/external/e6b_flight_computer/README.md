# Flipper-Zero-E6B-Flight-Computer
Flipper Zero E6B flight computer app useful for aviation pilots.

![e6b](e6b.png)

**E6B Flight Computer v1.0** is a flight calculation application for the Flipper Zero, developed by **Marcopiiit** for compatible firmware such as Momentum.

Inspired by the aviation E6B flight computer, this application provides a collection of calculations for flight planning, navigation, wind correction, fuel management, altitude, speed, and unit conversions.

## Features

### Wind Calculations
- Wind triangle: calculate wind correction angle (WCA), true heading, and groundspeed.
- Wind components: calculate headwind, tailwind, and crosswind components.
- Find wind: estimate wind direction and speed from flight data.
- 1-in-60 rule: calculate track error, closing angle, and total correction.

### Navigation
- Magnetic variation and compass deviation calculations.
- Point of No Return (PNR): calculate distance and time based on endurance and outbound/return groundspeeds.
- Equal Time Point (ETP): calculate the distance and time to the point where continuing or returning takes equal time.

### Time, Speed, and Distance
- Calculate groundspeed, time, or distance by setting one input field to `0`.
- Estimate elapsed time en route (ETE) and estimated time of arrival (ETA).

### Fuel Management
- Calculate fuel flow, endurance, or fuel consumed by setting one input field to `0`.
- Convert fuel volume and density into weight.
- Calculate usable endurance and maximum range based on fuel, reserve, fuel flow, and groundspeed.

### Altitude and Airspeed
- Pressure altitude calculation using elevation and QNH.
- Density altitude estimation using pressure altitude and outside air temperature.
- Calculate estimated true airspeed (TAS), speed of sound, and Mach number from calibrated airspeed, pressure altitude, and temperature.

### Climb and Descent
- Top of descent calculations, including descent distance, descent rate, and descent time.
- Climb gradient calculations, including gradient percentage, climb angle, and climb rate.
- Turn calculations, including turn rate, time for a 360-degree turn, and turn radius.

### Unit Conversions
- Nautical miles and kilometers.
- Nautical miles and statute miles.
- Knots and km/h.
- Knots and mph.
- Feet and meters.
- Kilograms and pounds.
- Liters and US gallons.
- hPa and inHg.
- Temperature conversions between Celsius, Fahrenheit, and Kelvin.

## Installation

1. Compile the application using a compatible Flipper Zero firmware development environment.
2. Obtain the compiled `e6b.fap` file.
3. Connect your Flipper Zero to your computer.
4. Open qFlipper or another compatible file manager.
5. Copy the application to:

   `/ext/apps/Tools/e6b.fap`

6. Safely disconnect your Flipper Zero.
7. Navigate to **Applications → Tools → E6B Flight Computer**.

The exact installation directory may vary depending on your firmware and build configuration.

## How to Use

1. Launch **E6B Flight Computer**.
2. Select **Start**.
3. Choose a calculation category.
4. Select the desired calculation.
5. Enter the required values using the number input interface.
6. Select `>> CALCULATE` to display the results.
7. Press Back to return to the previous screen.

Each calculation displays its results using the units shown in the input labels and output.

## Navigation and Menu Structure

The application organizes calculations into seven categories:

| Category | Available Calculations |
|---|---|
| Wind | Wind triangle, wind components, find wind, 1-in-60 rule |
| Navigation | Magnetic variation/deviation, PNR, ETP |
| Time/Speed/Dist | Groundspeed, time, distance, ETA |
| Fuel | Fuel calculations, fuel weight, range |
| Altitude/Speed | Pressure altitude, density altitude, CAS to TAS/Mach |
| Climb/Descent | Top of descent, climb gradient, turns |
| Conversions | Distance, speed, weight, volume, pressure, temperature |

## Settings

The Settings menu provides the following options:

- **Decimals:** Select the number of decimal places used in most calculated results.
- **Haptic on result:** Enable or disable the success notification when a calculation completes.

The application uses integer inputs and floating-point calculations for results.

Settings are held in memory while the application runs; the current implementation does not persist them between launches.

## Requirements

- Flipper Zero
- Compatible firmware supporting external applications, such as Momentum
- A compatible firmware development environment to compile the source code
- `e6b.fap` application for installation

## Important Notes

- This application is intended as an aviation calculation aid and educational tool.
- Verify calculations independently before using results for real-world flight planning or aircraft operation.
- Confirm the units, input values, assumptions, and applicable aircraft performance data before relying on any result.
- Some calculations use simplified mathematical models and may not account for all operational or atmospheric factors.
- This application is not a substitute for approved flight-planning tools, official aviation documentation, or pilot training.

## Author

**Marcopiiit**

## Version

`1.0`
