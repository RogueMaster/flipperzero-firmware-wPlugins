# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Fixed

* Message box alignment parameters `h` and `v` are now correctly evaluated.

## [1.1.0] - 2024-08-28

### Added

* Display splash screen upon application start.
* API documentation on [GitHub pages](https://ofabel.github.io/mp-flipper/).

## [1.0.0] - 2024-08-22

### Added

* First stable release on the [application catalog](https://github.com/flipperdevices/flipper-application-catalog).

### Changed

* Application ID is now `upython`

## [0.5.0-beta.1] - 2024-08-04

### Added

* Message dialog support.
* Update to the latest 0.104.0 firmware.

### Removed

* Disabled various Python builtins to shrink binary size.

## [0.4.0-beta.1] - 2024-04-14

### Added

* [Library](https://github.com/ofabel/mp-flipper/tree/lib) to include in the [firmware repository](https://github.com/ofabel/flipperzero-firmware).
* All generated files from the build prozess are now [part of the repository](https://github.com/ofabel/mp-flipper/tree/lib-release).
* Enabled split heap support for MicroPython:
  * The runtime can allocate and free heap memory.
  * Allows to start the Python process with small heap.
* Enabled scheduler support (required for interrupt handling).
* Enabled support for module `__init__` functions.
* Stabilized `flipperzero` module API:
  * Canvas support has now a proper implementation.
  * Interrupts from buttons are supported.

## [0.3.0-alpha.1] - 2024-04-04

### Added

* Floating point support
* Extend `flipperzero` module with support for:
  * Speaker, set volume and frequency
  * Canvas, very wacky implementation

## [0.2.0-alpha.1] - 2024-04-03

### Added

* Support for external imports
* Python `time` module support
* Python `random` module support
* Basic `flipperzero` module with support for:
  * Vibration
  * LED
  * Backlight
* Some test Python scripts

## [0.1.0-alpha.1] - 2024-04-01

### Added

* Basic build setup
* Minimal working example

[Unreleased]: https://github.com/ofabel/mp-flipper/compare/v1.1.0...HEAD
[1.1.0]: https://github.com/ofabel/mp-flipper/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/ofabel/mp-flipper/compare/v0.5.0-beta.1...v1.0.0
[0.5.0-beta.1]: https://github.com/ofabel/mp-flipper/compare/v0.4.0-beta.1...v0.5.0-beta.1
[0.4.0-beta.1]: https://github.com/ofabel/mp-flipper/compare/v0.3.0-alpha.1...v0.4.0-beta.1
[0.3.0-alpha.1]: https://github.com/ofabel/mp-flipper/compare/v0.2.0-alpha.1...v0.3.0-alpha.1
[0.2.0-alpha.1]: https://github.com/ofabel/mp-flipper/compare/v0.1.0-alpha.1...v0.2.0-alpha.1
[0.1.0-alpha.1]: https://github.com/ofabel/mp-flipper/releases/tag/v0.1.0-alpha.1
