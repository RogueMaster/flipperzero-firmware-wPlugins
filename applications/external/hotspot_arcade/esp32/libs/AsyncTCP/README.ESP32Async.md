# AsyncTCP

[![License: LGPL 3.0](https://img.shields.io/badge/License-LGPL%203.0-yellow.svg)](https://opensource.org/license/lgpl-3-0/)
[![Continuous Integration](https://github.com/ESP32Async/AsyncTCP/actions/workflows/ci.yml/badge.svg)](https://github.com/ESP32Async/AsyncTCP/actions/workflows/ci.yml)
[![PlatformIO Registry](https://badges.registry.platformio.org/packages/ESP32Async/library/AsyncTCP.svg)](https://registry.platformio.org/libraries/ESP32Async/AsyncTCP)

Discord Server: [https://discord.gg/X7zpGdyUcY](https://discord.gg/X7zpGdyUcY)

### Async TCP Library for ESP32 Arduino

This is a fully asynchronous TCP library, aimed at enabling trouble-free, multi-connection network environment for Espressif's ESP32 MCUs.

This library is the base for [ESPAsyncWebServer](https://github.com/ESP32Async/ESPAsyncWebServer)

## AsyncClient and AsyncServer

The base classes on which everything else is built. They expose all possible scenarios, but are really raw and require more skills to use.

## Changes

- `library.properties` for Arduino IDE users
- Add `CONFIG_ASYNC_TCP_MAX_ACK_TIME`
- Add `CONFIG_ASYNC_TCP_PRIORITY`
- Add `CONFIG_ASYNC_TCP_QUEUE_SIZE`
- Add `setKeepAlive()`
- Arduino 3 / ESP-IDF 5 compatibility
- Better CI
- Better example
- Customizable macros
- Fix for "Required to lock TCPIP core functionality". Ref: https://github.com/ESP32Async/AsyncTCP/issues/27 and https://github.com/espressif/arduino-esp32/issues/10526
- Fix for "ack timeout 4" client disconnects.
- Fix from https://github.com/me-no-dev/AsyncTCP/pull/173 (partially applied)
- Fix from https://github.com/me-no-dev/AsyncTCP/pull/184
- IPv6
- LIBRETINY support
- LibreTuya
- Reduce logging of non critical messages
- Use IPADDR6_INIT() macro to set connecting IPv6 address
- xTaskCreateUniversal function

## Coordinates

```
ESP32Async/AsyncTCP @ ^3.3.2
```

## Important recommendations

Most of the crashes are caused by improper configuration of the library for the project.
Here are some recommendations to avoid them.

I personally use the following configuration in my projects:

```c++
  -D CONFIG_ASYNC_TCP_MAX_ACK_TIME=5000 // (keep default)
  -D CONFIG_ASYNC_TCP_PRIORITY=10 // (keep default)
  -D CONFIG_ASYNC_TCP_QUEUE_SIZE=64 // (keep default)
  -D CONFIG_ASYNC_TCP_RUNNING_CORE=1 // force async_tcp task to be on same core as the app (default is core 0)
  -D CONFIG_ASYNC_TCP_STACK_SIZE=4096 // reduce the stack size (default is 16K)
```
