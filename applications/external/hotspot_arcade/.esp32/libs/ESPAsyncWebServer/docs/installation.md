# Installation

## Compatibility

- ESP32, ESP8266, RP2040, RP2350
- Arduino Core 2.x and 3.x

## How to install

The library can be downloaded from the releases page at [https://github.com/ESP32Async/ESPAsyncWebServer/releases](https://github.com/ESP32Async/ESPAsyncWebServer/releases).

It is also deployed in these registries:

- Arduino Library Registry: [https://github.com/arduino/library-registry](https://github.com/arduino/library-registry)

- Espressif Component Registry [https://components.espressif.com/components/esp32async/espasyncwebserver](https://components.espressif.com/components/esp32async/espasyncwebserver)

- PlatformIO Registry: [https://registry.platformio.org/libraries/ESP32Async/ESPAsyncWebServer](https://registry.platformio.org/libraries/ESP32Async/ESPAsyncWebServer)
  - Use: `lib_deps=ESP32Async/ESPAsyncWebServer` to point to latest version
  - Use: `lib_deps=ESP32Async/ESPAsyncWebServer @ ^<x.y.z>` to point to latest version with the same major version
  - Use: `lib_deps=ESP32Async/ESPAsyncWebServer @ <x.y.z>` to always point to the same version (reproducible build)

## Dependencies

### ESP32 / pioarduino

```ini
[env:stable]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/stable/platform-espressif32.zip
lib_compat_mode = strict
lib_ldf_mode = chain
lib_deps =
  ESP32Async/AsyncTCP
  ESP32Async/ESPAsyncWebServer
```

### ESP8266 / pioarduino

```ini
[env:stable]
platform = espressif8266
lib_compat_mode = strict
lib_ldf_mode = chain
lib_deps =
  ESP32Async/ESPAsyncTCP
  ESP32Async/ESPAsyncWebServer
```

### LibreTiny (BK7231N/T, RTL8710B, etc.)

Version 1.9.1 or newer is required.

```ini
[env:stable]
platform = libretiny @ ^1.9.1
lib_ldf_mode = chain
lib_deps =
  ESP32Async/AsyncTCP
  ESP32Async/ESPAsyncWebServer
```

### Unofficial dependencies

**AsyncTCPSock**

AsyncTCPSock can be used instead of AsyncTCP by excluding AsyncTCP from the library dependencies and adding AsyncTCPSock instead:

```ini
lib_compat_mode = strict
lib_ldf_mode = chain
lib_deps =
  https://github.com/ESP32Async/AsyncTCPSock/archive/refs/tags/v1.0.3-dev.zip
  ESP32Async/ESPAsyncWebServer
lib_ignore =
  AsyncTCP
  ESP32Async/AsyncTCP
```

**RPAsyncTCP**

RPAsyncTCP replaces AsyncTCP to provide support for RP2040(+WiFi) and RP2350(+WiFi) boards. For example - Raspberry Pi Pico W and Raspberry Pi Pico 2W.

```ini
lib_compat_mode = strict
lib_ldf_mode = chain
platform = https://github.com/maxgerhardt/platform-raspberrypi.git
board = rpipicow
board_build.core = earlephilhower
lib_deps =
  ayushsharma82/RPAsyncTCP@^1.3.2
  ESP32Async/ESPAsyncWebServer
lib_ignore =
  lwIP_ESPHost
build_flags = ${env.build_flags}
  -Wno-missing-field-initializers
```
