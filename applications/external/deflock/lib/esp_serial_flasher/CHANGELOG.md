<a href="https://www.espressif.com">
    <img src="https://www.espressif.com/sites/all/themes/espressif/logo-black.svg" align="right" height="20" />
</a>

# CHANGELOG

> All notable changes to this project are documented in this file.
> This list is not exhaustive - only important changes, fixes, and new features in the code are reflected here.

<div align="center">
    <a href="https://keepachangelog.com/en/1.1.0/">
        <img alt="Static Badge" src="https://img.shields.io/badge/Keep%20a%20Changelog-v1.1.0-salmon?logo=keepachangelog&logoColor=black&labelColor=white&link=https%3A%2F%2Fkeepachangelog.com%2Fen%2F1.1.0%2F">
    </a>
    <a href="https://www.conventionalcommits.org/en/v1.0.0/">
        <img alt="Static Badge" src="https://img.shields.io/badge/Conventional%20Commits-v1.0.0-pink?logo=conventionalcommits&logoColor=black&labelColor=white&link=https%3A%2F%2Fwww.conventionalcommits.org%2Fen%2Fv1.0.0%2F">
    </a>
    <a href="https://semver.org/spec/v2.0.0.html">
        <img alt="Static Badge" src="https://img.shields.io/badge/Semantic%20Versioning-v2.0.0-grey?logo=semanticrelease&logoColor=black&labelColor=white&link=https%3A%2F%2Fsemver.org%2Fspec%2Fv2.0.0.html">
    </a>
</div>
<hr>

## v1.8.0 (2025-01-27)

### ✨ New Features

- **examples**: Add ESP32 fast reflash example with MD5 check *(Jaroslav Burian - e0b9b05)*
- Add a function to check flash regions against a known MD5 *(Djordje Nedic - dd480bc)*
- Add the SDIO inteface and the corresponding esp port *(Djordje Nedic - b74194b)*

### 🐛 Bug Fixes

- **esp32s3**: Ensure electric current capabilities allowing to flash devkits with UART converters *(Jaroslav Burian - 05d78a4)*
- Print MD5 debug as hex string when using stub *(Jaroslav Burian - ab0ce64)*
- Unify debug prints of all ports *(Jaroslav Burian - ccfd42c)*
- Set default flash size when detection fails *(Jaroslav Burian - adbe6ab)*
- INVALID_ARG error when ESP32P4 used as a host *(Jaroslav Burian - b42d1d8)*
- Return when SDIO connection not initialized *(Jaroslav Burian - 3ee2b25)*
- esp32c2 incorrect SPI flash config detection *(Jaroslav Burian - ed43e0a)*
- Add missing definition for Zephyr debug trace *(Jaroslav Burian - 86effb2)*
- Update USB reset sequence *(Jaroslav Burian - f64cf7c)*

### 📖 Documentation

- Update log output for Zephyr example *(Jaroslav Burian - 7abd989)*
- Fix swapped RX and TX in Zephyr example *(Jaroslav Burian - 7855e5e)*


## v1.7.0 (2024-11-25)

### ✨ New Features

- **examples**: Provide more useful error messages *(Djordje Nedic - 663aa35)*
- Add support for reading from target flash *(Djordje Nedic - 0785c94)*
- Add support for inverting reset and boot pins *(shiona - dfb8551)*
- Update zephyr example to v4.0.0 *(Jaroslav Burian - a6a0711)*

### 🐛 Bug Fixes

- Clarify and validate alignment requirements for flashing *(Djordje Nedic - 400d614)*
- ROUNDUP calculation fix *(Djordje Nedic - 6f17bfe)*
- Zephyr example *(Marek Matej - 5d380bd)*
- Handling of USB buffer overflow *(Jaroslav Burian - aa95e24)*
- Callback when USB port is disconnected *(Jaroslav Burian - c966fe6)*

### 📖 Documentation

- Remove experimental note for USB port *(Jaroslav Burian - 53fb930)*

---

## v1.6.2 (2024-10-18)

### 🐛 Bug Fixes

- Fix the ESP USB CDC ACM port read return value *(Djordje Nedic - ce71bff)*

---

## v1.6.1 (2024-10-15)

### 🐛 Bug Fixes

- Fix ESP32-H2 flash detection *(Jaroslav Burian - a8b1ac4)*

---

## v1.6.0 (2024-09-26)

### ✨ New Features

- Add logging from target to rpi pico port *(Jaroslav Burian - 15d34a1)*
- Add support for Secure Download Mode *(Djordje Nedic - 46f3f0b)*
- Add support for getting target security info *(Djordje Nedic - 96d0183)*

### 🐛 Bug Fixes

- Add delay to usb example to ensure connection *(Jaroslav Burian - c0a2069)*
- Do not enable logging if flashing error - STM *(Jaroslav Burian - cc0421f)*
- Add missing esp32c3 chip magic numbers *(Jaroslav Burian - 82c4fc7)*
- Multiple timeout fixes *(Djordje Nedic - 8f54452)*
- Only check if image fits into flash when we know flash size *(Djordje Nedic - 3bc629c)*
- Duplicate word in logging message *(Jaroslav Burian - c19f0bf)*

### 🔧 Code Refactoring

- **protocol**: Rework command and response handling code *(Djordje Nedic - 86efdf9)*
- **protocol**: Add support for receiving variably sized SLIP packets *(Djordje Nedic - e3c332a)*

---

## v1.5.0 (2024-08-13)

### ✨ New Features

- Add the Raspberry Pi Pico port *(Djordje Nedic - c72680c)*

---

## v1.4.1 (2024-07-16)

### 🐛 Bug Fixes

- Add correct MD5 calculation with stub enabled *(Jaroslav Burian - 88352d9)*
- Remove duplicate word in logging *(Jaroslav Burian - 7225889)*

### 📖 Documentation

- Update description in the READMEs *(Jaroslav Burian - 16b16a6)*

---

## v1.4.0 (2024-07-02)

### ✨ New Features

- add stub-support *(Vincent Hamp - ec1fc06)*

### 🐛 Bug Fixes

- Fix MD5 option handling *(Djordje Nedic - 09e1192)*

---

## v1.3.1 (2024-06-26)

### 🐛 Bug Fixes

- Fix ESP SPI port duplicate tracing *(Djordje Nedic - 116900d)*
- SPI interface/esp port alignment requirements fix *(Djordje Nedic - 9706ed6)*
- Upload to RAM examples monitor task priority fix *(Djordje Nedic - fd86bc3)*

---

## v1.3.0 (2024-03-22)

### ✨ New Features

- Add a convenient public API way to read the WIFI MAC *(Djordje Nedic - 8f07704)*

### 🐛 Bug Fixes

- Correctly compare image size with memory size including offset *(Dzarda7 - 2807ab5)*

---

## v1.2.0 (2024-02-28)

### ✨ New Features

- Move flash size detection functionality to the public API *(Djordje Nedic - 6ae5b95)*

### 🐛 Bug Fixes

- **docs**: Fix table in SPI load to RAM example *(Djordje Nedic - edd8a6a)*
- Fix inferring flash size from the flash ID *(Djordje Nedic - 3a1f345)*

---

## v1.1.0 (2024-02-13)

### ✨ New Features

- USB CDC ACM interface support *(Djordje Nedic - 4e26444)*
- Add the ability for ESP ports to not initialize peripherals *(Djordje Nedic - 541fe12)*

### 🐛 Bug Fixes

- **docs**: Remove notes about SPI interface being experimental *(Djordje Nedic - 3ffc189)*
- Document size_id and improve comments in places *(Djordje Nedic - 3a1e818)*

---

## v1.0.2 (2023-12-20)

### 🐛 Bug Fixes

- Fix flash size ID sanity checks *(Djordje Nedic - 01e1618)*

---

## v1.0.1 (2023-12-19)

### 🐛 Bug Fixes

- **ci**: Add more compiler warnings for the flasher in the examples *(Djordje Nedic - eb18fd0)*
- Fix md5 timeout values *(Djordje Nedic - 058156c)*

---

<div align="center">
    <small>
        <b>
            <a href="https://www.github.com/espressif/cz-plugin-espressif">Commitizen Espressif plugin</a>
        </b>
    <br>
        <sup><a href="https://www.espressif.com">Espressif Systems CO LTD. (2025)</a><sup>
    </small>
</div>
