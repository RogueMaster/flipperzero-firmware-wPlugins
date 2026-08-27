# Hosted Build (Arduino Emulator)

ESPAsyncWebServer can be compiled and run as a **native executable** on Linux, Windows, and macOS using the [Arduino-Emulator](https://github.com/pschatzmann/Arduino-Emulator) and [PosixAsyncTCP](https://github.com/MitchBradley/PosixAsyncTCP) libraries.

This allows you to:

- Build and run the server locally without any embedded hardware
- Iterate quickly on server logic without flashing a device
- Integrate the server into CI pipelines for compile-time and basic runtime testing

The compiled binary is a real HTTP/WebSocket server that listens on a local port and can be tested with any HTTP client.

---

## Requirements

| Tool | Notes |
|------|-------|
| CMake ≥ 3.11 | Build system |
| Ninja (recommended) or Make | Generator |
| C++17 compiler | GCC, Clang, or MSVC |
| Git | Cloning dependencies |

The following external repositories must be cloned before building:

| Repository | Purpose | Clone path |
|-----------|---------|------------|
| [pschatzmann/Arduino-Emulator](https://github.com/pschatzmann/Arduino-Emulator) | Arduino core emulation (millis, delay, String, Serial, …) | `.ci/arduino-emulator` |
| [MitchBradley/PosixAsyncTCP](https://github.com/MitchBradley/PosixAsyncTCP) | POSIX socket-based AsyncTCP replacement | `.ci/asynctcp` |
| [espressif/arduino-esp32](https://github.com/espressif/arduino-esp32) | FS/LittleFS headers | `.ci/arduino-esp32` |

---

## Cloning dependencies

```bash
git clone --depth 1 --recurse-submodules \
  https://github.com/pschatzmann/Arduino-Emulator.git \
  .ci/arduino-emulator

git clone --depth 1 \
  https://github.com/MitchBradley/PosixAsyncTCP \
  .ci/asynctcp

git clone --depth 1 \
  https://github.com/espressif/arduino-esp32.git \
  .ci/arduino-esp32
```

---

## Building

The CMake project is located at [`examples/arduino_emulator/`](https://github.com/ESP32Async/ESPAsyncWebServer/tree/master/examples/arduino_emulator).

```bash
# Configure
cmake -S examples/arduino_emulator \
      -B .ci/arduino-emulator-build/out \
      -G Ninja

# Build the host executable
cmake --build .ci/arduino-emulator-build/out \
      --target espasyncwebserver_host \
      --parallel
```

This produces the `espasyncwebserver_host` executable in the build output directory.

---

## Running

```bash
chmod +x .ci/arduino-emulator-build/out/espasyncwebserver_host
.ci/arduino-emulator-build/out/espasyncwebserver_host
```

The server starts on port **8080** by default. You can test it with:

```bash
curl http://localhost:8080/
```

Expected output:

```
ESPAsyncWebServer host app is running on port 8080
```

---

## Example application

The entry point is [`examples/arduino_emulator/main.cpp`](https://github.com/ESP32Async/ESPAsyncWebServer/tree/master/examples/arduino_emulator/main.cpp):

```cpp
#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

static AsyncWebServer server(8080);

void setup() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain",
                  "ESPAsyncWebServer host app is running on port 8080\n");
  });

  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found\n");
  });

  PosixAsyncTCPManager::getInstance().begin();
  server.begin();
}

void loop() {
  delay(1000);
}
```

The standard `setup()` / `loop()` structure is identical to embedded Arduino code. Arduino-Emulator calls these functions and provides the Arduino API.

---

## CMake project structure

[`examples/arduino_emulator/CMakeLists.txt`](https://github.com/ESP32Async/ESPAsyncWebServer/tree/master/examples/arduino_emulator/CMakeLists.txt) defines three targets:

| CMake target | Description |
|-------------|-------------|
| `arduino_emulator` | Arduino core (provided by Arduino-Emulator via `add_subdirectory`) |
| `espasyncwebserver` | All `src/*.cpp` files compiled as a static library |
| `test` | AsyncTCP, FS, and base64 support compiled as a static library |
| `espasyncwebserver_host` | Final executable linking everything together |

The `HOST` preprocessor definition is set on all targets so that platform-specific code (e.g. lwIP headers) is conditionally excluded at compile time.

---

## Preprocessor define

When building for the host, the `HOST` macro is defined:

```cmake
target_compile_definitions(espasyncwebserver PUBLIC HOST ARDUINO=10813)
```

You can use it in your own code to guard any embedded-only sections:

```cpp
#ifndef HOST
  // ESP32/ESP8266-only code
  WiFi.begin(ssid, password);
#endif
```

---

## CI integration

The [Build (Arduino Emulator)](https://github.com/ESP32Async/ESPAsyncWebServer/actions/workflows/build-arduino-emulator.yml) GitHub Actions workflow runs this build automatically on every push and pull request targeting `main` or `release/*` branches.

The workflow runs on **Ubuntu** (`ubuntu-latest`) and performs the following steps:

1. Install `cmake`, `ninja-build`, `build-essential`, and `git`
2. Clone Arduino-Emulator, PosixAsyncTCP, and arduino-esp32
3. Configure and build the `espasyncwebserver_host` target
4. Mark the resulting binary as executable

See [`.github/workflows/build-arduino-emulator.yml`](https://github.com/ESP32Async/ESPAsyncWebServer/blob/master/github/workflows/build-arduino-emulator.yml) for the full workflow definition.
