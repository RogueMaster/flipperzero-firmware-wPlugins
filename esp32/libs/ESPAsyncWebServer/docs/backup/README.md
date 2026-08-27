![https://avatars.githubusercontent.com/u/195753706?s=96&v=4](https://avatars.githubusercontent.com/u/195753706?s=96&v=4)

# ESPAsyncWebServer

[![Latest Release](https://img.shields.io/github/release/ESP32Async/ESPAsyncWebServer.svg)](https://GitHub.com/ESP32Async/ESPAsyncWebServer/releases/)
[![PlatformIO Registry](https://badges.registry.platformio.org/packages/ESP32Async/library/ESPAsyncWebServer.svg)](https://registry.platformio.org/libraries/ESP32Async/ESPAsyncWebServer)

[![License: LGPL 3.0](https://img.shields.io/badge/License-LGPL%203.0-yellow.svg)](https://opensource.org/license/lgpl-3-0/)
[![Contributor Covenant](https://img.shields.io/badge/Contributor%20Covenant-2.1-4baaaa.svg)](code_of_conduct.md)

[![GitHub latest commit](https://badgen.net/github/last-commit/ESP32Async/ESPAsyncWebServer)](https://GitHub.com/ESP32Async/ESPAsyncWebServer/commit/)

[![ESP32Async Discord Server](https://img.shields.io/badge/Discord-ESP32Async-blue?logo=discord)](https://discord.gg/X7zpGdyUcY)

[![Documentation](https://img.shields.io/badge/Wiki-ESPAsyncWebServer-blue?logo=github)](https://github.com/ESP32Async/ESPAsyncWebServer/wiki)

## Asynchronous HTTP and WebSocket Server Library for ESP32, ESP8266, RP2040 and RP2350

Supports: WebSocket, SSE, Authentication, Arduino Json 7, File Upload, Static File serving, URL Rewrite, URL Redirect, etc.

- [Documentation](#documentation)
- [How to install](#how-to-install)
- [Dependencies](#dependencies)
  - [ESP32 / pioarduino](#esp32--pioarduino)
  - [ESP8266 / pioarduino](#esp8266--pioarduino)
  - [Unofficial dependencies](#unofficial-dependencies)

## Documentation

The complete [project documentation](https://github.com/ESP32Async/ESPAsyncWebServer/wiki) is available in the Wiki section.

## How to install

The library can be downloaded from the releases page at [https://github.com/ESP32Async/ESPAsyncWebServer/releases](https://github.com/ESP32Async/ESPAsyncWebServer/releases).

It is also deployed in these registries:

- Arduino Library Registry: [https://github.com/arduino/library-registry](https://github.com/arduino/library-registry)

- ESP Component Registry [https://components.espressif.com/components/esp32async/espasyncwebserver](https://components.espressif.com/components/esp32async/espasyncwebserver)

- PlatformIO Registry: [https://registry.platformio.org/libraries/esp32async/ESPAsyncWebServer](https://registry.platformio.org/libraries/esp32async/ESPAsyncWebServer)

  - Use: `lib_deps=ESP32Async/ESPAsyncWebServer` to point to latest version
  - Use: `lib_deps=ESP32Async/ESPAsyncWebServer @ ^<x.y.z>` to point to latest version with the same major version
  - Use: `lib_deps=ESP32Async/ESPAsyncWebServer @ <x.y.z>` to always point to the same version (reproductible build)

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

## Important recommendations for build options

Most of the crashes are caused by improper use or configuration of the AsyncTCP library used for the project.
Here are some recommendations to avoid them and build-time flags you can change.

`CONFIG_ASYNC_TCP_MAX_ACK_TIME` - defines a timeout for TCP connection to be considered alive when waiting for data.
In some bad network conditions you might consider increasing it.

`CONFIG_ASYNC_TCP_QUEUE_SIZE` - defines the length of the queue for events related to connections handling.
Both the server and AsyncTCP library were optimized to control the queue automatically. Do NOT try blindly increasing the queue size, it does not help you in a way you might think it is. If you receive debug messages about queue throttling, try to optimize your server callbacks code to execute as fast as possible.
Read #165 thread, it might give you some hints.

`CONFIG_ASYNC_TCP_RUNNING_CORE` - CPU core thread affinity that runs the queue events handling and executes server callbacks. Default is ANY core, so it means that for dualcore SoCs both cores could handle server activities. If your server's code is too heavy and unoptimized or you see that sometimes
server might affect other network activities, you might consider to bind it to the same core that runs Arduino code (1) to minimize affect on radio part. Otherwise you can leave the default to let RTOS decide where to run the thread based on priority

`CONFIG_ASYNC_TCP_STACK_SIZE` - stack size for the thread that runs sever events and callbacks. Default is 16k that is a way too much waste for well-defined short async code or simple static file handling. You might want to cosider reducing it to 4-8k to same RAM usage. If you do not know what this is or not sure about your callback code demands - leave it as default, should be enough even for very hungry callbacks in most cases.

> [!NOTE]
> This relates to ESP32 only, ESP8266 uses different ESPAsyncTCP lib that does not has this build options

I personally use the following configuration in my projects:

```c++
  -D CONFIG_ASYNC_TCP_MAX_ACK_TIME=5000   // (keep default)
  -D CONFIG_ASYNC_TCP_PRIORITY=10         // (keep default)
  -D CONFIG_ASYNC_TCP_QUEUE_SIZE=64       // (keep default)
  -D CONFIG_ASYNC_TCP_RUNNING_CORE=1      // force async_tcp task to be on same core as Arduino app (default is any core)
  -D CONFIG_ASYNC_TCP_STACK_SIZE=4096     // reduce the stack size (default is 16K)
```

If you need to serve chunk requests with a really low buffer (which should be avoided), you can set `-D ASYNCWEBSERVER_USE_CHUNK_INFLIGHT=0` to disable the in-flight control.
