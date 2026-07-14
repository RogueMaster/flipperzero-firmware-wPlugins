# WiFi_MaP

<div align="center">
    <p>An ESP32 and FlipperZero wifi mapping</p>
    <p>
        <img src="https://github.com/carvilsi/flipper0-wifi-map/blob/main/.github/images/wifi_map.png" alt="wifi_map" >
    </p>
</div>

---

Check the ESP32 side at: [esp32-wifi-map](https://github.com/carvilsi/esp32-wifi-map)

And do not forget to have both sides on the last version.


## Install, etc.

Clone this repo at `applications_user/` folder under `flipperzero-firmware`

`$ git clone git@github.com:carvilsi/flipper0-wifi-map.git`

### Build and flash

`$ ./fbt launch APPSRC=wifi_map`

### Logs

`$ minicom -D /dev/ttyACM0 -b 230400`

## Connection FlipperZero and ESP32

| Flipper Zero GPIO |    ESP32    |
|-------------------|-------------|
|      GND          |     GND     |
|      RX           | TXD GPIO_17 |
|      3V3          |     3V3     |

## Collected Data

WiFi_MaP, by now, saves the collected data on a kind of **CSV** formated file, like:

```
AP hash;Distance (meters);AP auth mode;Time from start (seconds)
163976e1;61.90;4;68
f8fe611;74.99;3;68
134a1d9f;74.99;3;68
f81b53a0;82.54;3;68
287de1e8;100.00;3;68
9669d9a2;100.00;3;68
427291b2;133.35;3;68
f8a4985c;1.96;3;71
2e41b0ed;8.25;4;71
2d2facaa;14.68;3;71
394d4b0b;19.57;3;71
bab9b9a6;26.10;3;71
f81b53a0;31.62;3;71
58d968cb;34.81;4;71
9fac872f;34.81;3;71
287de1e8;38.31;3;71
163976e1;42.17;4;71
eb77764e;51.09;3;71
6cb4288c;68.13;4;71
69479d2d;74.99;5;71
```
Every time that WiFi_MaP starts adds the date to the file surronded by hash signs, then starts to add the collected AP
data separated by ';'. The maximun AP that scans is 20.
The data is:

| AP name hashed | distance to AP (meters) | AP auth mode (1) | seconds since the scan started (2) | 
|----------------|-------------------------|------------------|------------------------------------|
|    9fac872f    |          1.96           |         3        |                 18                 |

### Notes 

- **(1)** -> Table of ESP32 AP WIFI Auth Mode [wifi_auth_mode_t](https://github.com/espressif/esp-idf/blob/master/components/esp_wifi/include/esp_wifi_types_generic.h#L85)

| Open | WEP | WPA_PSK | WPA2_PSK | WPA_WPA2_PSK | ENTERPRISE/WPA2 | WPA3_PSK |
|------|-----|---------|----------|--------------|-----------------|----------|
|  0   |  1  |    2    |    3     |      4       |     5/6         |     7    |


- **(2)** -> It's the time in seconds that the ESP32 module is connected to FlipperZero 

## Plot and Process Colected Data

If you want to plot and process the collected data from your warwalking session please check [flipper0-process-wifi-map-data repo](https://github.com/carvilsi/flipper0-process-wifi-map-data)

<div align="center">
    <p>
        <img src="https://github.com/carvilsi/flipper0-wifi-map/blob/main/.github/images/detail_wifi_map_aps_radius.png" alt="detail_wifi_map_aps_radius" >
    </p>
</div>

---

## TODOs

- [x] Generate one file by session, filename with date and time.
- [x] Add headers to CSV.
- [ ] Add interaction to reset time on ESP32 side
- [ ] Add navegator; basically with previous collected and processed data, get my current locations

---

**Note:** main branch works with latest version of FlipperZero firmware, **1.0.1** (in the moment of writting this).
If you want to have this app running with a firmware version < 1.* check the *legacy-previous-API-1* at the repo.

---

<3 & Hack the Planet!
