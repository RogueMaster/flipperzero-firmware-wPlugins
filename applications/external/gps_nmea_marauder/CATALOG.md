# GPS NMEA for ESP32-C5 Marauder

Fork of gps_nmea (xMasterX / ezod) that reads GPS from an ESP32-C5 board
running ESP32 Marauder firmware instead of a bare GPS module.

## How it works
- Flipper talks to the Marauder over the GPIO UART (pins 13 TX / 14 RX) at
  115200 baud, the Marauder CLI default.
- On start the app sends "stopscan" then "nmea"; Marauder streams raw NMEA from
  its onboard GPS and the app parses it with minmea.
- On exit it sends "stopscan" so the board returns to the normal CLI.

## Controls
- **OK**: toggle backlight
- **Long Up**: cycle baud rate (default 115200)
- **Long Right**: cycle speed unit
- **Long Down**: restart the GPS stream ("stopscan" + "nmea")
- **Back**: exit
