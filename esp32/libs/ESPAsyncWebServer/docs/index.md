# ESPAsyncWebServer

![https://avatars.githubusercontent.com/u/195753706?s=96&v=4](https://avatars.githubusercontent.com/u/195753706?s=96&v=4)

[![Latest Release](https://img.shields.io/github/release/ESP32Async/ESPAsyncWebServer.svg)](https://GitHub.com/ESP32Async/ESPAsyncWebServer/releases/)
[![PlatformIO Registry](https://badges.registry.platformio.org/packages/ESP32Async/library/ESPAsyncWebServer.svg)](https://registry.platformio.org/libraries/ESP32Async/ESPAsyncWebServer)

[![License: LGPL 3.0](https://img.shields.io/badge/License-LGPL%203.0-yellow.svg)](https://opensource.org/license/lgpl-3-0/)
[![Contributor Covenant](https://img.shields.io/badge/Contributor%20Covenant-2.1-4baaaa.svg)](code_of_conduct.md)

[![GitHub latest commit](https://badgen.net/github/last-commit/ESP32Async/ESPAsyncWebServer)](https://GitHub.com/ESP32Async/ESPAsyncWebServer/commit/)

[![Documentation](https://img.shields.io/badge/Wiki-ESPAsyncWebServer-blue?logo=github)](https://github.com/ESP32Async/ESPAsyncWebServer/wiki)

Asynchronous HTTP and WebSocket Server Library for ESP32, ESP8266, RP2040 and RP2350

Supports: WebSocket, SSE, Authentication, Arduino Json 7, MessagePack, File Upload, Static File serving, URL Rewrite, URL Redirect, etc.

## Overview

- Using asynchronous network means that you can handle more than one connection at the same time
- You are called once the request is ready and parsed
- When you send the response, you are immediately ready to handle other connections
  while the server is taking care of sending the response in the background
- Speed is OMG
- Easy to use API, HTTP Basic and Digest MD5 Authentication (default), ChunkedResponse
- Easily extendible to handle any type of content
- Supports Continue 100
- Async WebSocket plugin offering different locations without extra servers or ports
- Async EventSource (Server-Sent Events) plugin to send events to the browser
- URL Rewrite plugin for conditional and permanent url rewrites
- ServeStatic plugin that supports cache, Last-Modified, default index and more
- Simple template processing engine to handle templates

## Libraries and projects that use AsyncWebServer

- [Beelance](https://beelance.carbou.me/) - Autonomous and remotely connected weight scale for beehives 🐝
- [ElegantOTA](https://github.com/ayushsharma82/ElegantOTA) - OTA updates made slick and simple for everyone!
- [ESP Radio](https://github.com/Edzelf/Esp-radio) - Icecast radio based on ESP8266 and VS1053
- [ESP-DASH](https://github.com/ayushsharma82/ESP-DASH) - ESP-DASH is a library for ESP32 Arduino that facilitates the use of a dashboard in an asynchronous way. I have contributed most of the recently newly added fixes and features of the OSS and Pro version
- [ESP-RFID](https://github.com/omersiar/esp-rfid) - MFRC522 RFID Access Control Management project for ESP8266.
- [ESPurna](https://github.com/xoseperez/espurna) - ESPurna ("spark" in Catalan) is a custom C firmware for ESP8266 based smart switches. It was originally developed with the ITead Sonoff in mind.
- [FauxmoESP](https://github.com/vintlabs/fauxmoESP) - Belkin WeMo emulator library for ESP8266.
- [MycilaESPConnect](https://mathieu.carbou.me/MycilaESPConnect) - Simple & Easy Network Manager with Captive Portal for ESP32 supporting Ethernet
- [MycilaJSY](https://mathieu.carbou.me/MycilaJSY) - Arduino / ESP32 library for the JSY1031, JSY-MK-163, JSY-MK-193, JSY-MK-194, JSY-MK-227, JSY-MK-229, JSY-MK-333 families single-phase and three-phase AC bidirectional meters from Shenzhen Jiansiyan Technologies Co, Ltd.
- [MycilaWebSerial](https://mathieu.carbou.me/MycilaWebSerial) - WebSerial is a Serial Monitor for ESP32 that can be accessed remotely via a web browser
- [NetWizard](https://github.com/ayushsharma82/NetWizard) - No need to hard-code WiFi credentials ever again. (ESP32, RP2040+W)
- [Sattrack](https://github.com/Hopperpop/Sattrack) - Track the ISS with ESP8266
- [VZero](https://github.com/andig/vzero) - the Wireless zero-config controller for volkszaehler.org
- [WebSerial](https://github.com/ayushsharma82/WebSerial) - A remote terminal library for wireless microcontrollers to log, monitor or debug your firmware/product
- [WebSocketToSerial](https://github.com/hallard/WebSocketToSerial) - Debug serial devices through the web browser
- [YaSolR (Yet another Solar Router)](https://yasolr.carbou.me) - Heat water with your Solar Production Excess with the more powerful and precise solar diverter out there!
