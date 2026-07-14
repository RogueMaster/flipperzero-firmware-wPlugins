# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.8.0]

### Fixed

* [#9](https://github.com/ofabel/mp-flipper/issues/9): Update to latest SDK version.

## [1.7.0]

### Changed

* The `SPEAKER_NOTE_*` constants are replaced by an attribute delegator function to save space.

### Fixed

* [#6](https://github.com/ofabel/mp-flipper/issues/6): Update to latest SDK version.

## [1.6.0] - 2024-11-17

### Added

* Enabled extra functions for the `random` module.

## [1.5.0] - 2024-10-06

### Added

* Support for basic file system operations using the `io` module:
  * Read and write files.
  * Open in text or binary mode.
* Simple `logging` module:
  * Log levels according to the Flipper Zero API: trace, debug, info, warn, error.
  * Only the root logger is supported, so no `getLogger` function.
  * Logs directly to the log output, so no output in the REPL.
* Redirect output of `print` statements:
  * To `stdout` when a script is invoked by `py` command from the CLI.
  * To the log buffer, if a script is invoked from the UI.
* UART support for the `flipperzero` module.

### Changed

* The `py` command waits until the script terminates.

### Fixed

* [#3](https://github.com/ofabel/mp-flipper/issues/3): Proper `CR` and `LF` handling in the REPL.

## [1.4.0] - 2024-09-29

### Added

* Allow passing the path to the script to execute as a CLI argument.
* Open a REPL from the CLI interface by using the `py` command:
  * The `py` command is only available while the app is running.
  * You cannot run a Python script and use the REPL at the same time.
  * You can also start a Python script with the `py` command while the app is idle.

### Changed

* MicroPython update to version 1.23.0.

## [1.3.0] - 2024-09-08

### Added

* Simple ADC support for the `flipperzero` module:
  * Read raw value.
  * Read voltage.
* Simple PWM support for the `flipperzero` module:
  * Start a signal.
  * Stop a signal.
  * Check the status.
* Infrared support for the `flipperzero` module:
  * Receive a signal.
  * Transmit a signal.
  * Check the status.
* Reset used GPIO pins upon script termination.
* Improved GPIO related functions to prevent user errors.
* Published [Python package on PyPI](https://pypi.org/project/flipperzero/) for code completion support.

### Changed

* The GPIO init function `flipperzero.gpio_init_pin` returns a boolean value.

## [1.2.0] - 2024-09-05

### Added

* Constants for all musical notes from C0 up to B8.
* Constants for minimum and maximum speaker volumes.
* Simple GPIO support for the `flipperzero` module:
  * Initialize a pin.
  * Read from a pin.
  * Write to a pin.
  * Handle interrupts.

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

[Unreleased]: https://github.com/ofabel/mp-flipper/compare/v1.8.0...dev
[1.8.0]: https://github.com/ofabel/mp-flipper/compare/v1.7.0...v1.8.0
[1.7.0]: https://github.com/ofabel/mp-flipper/compare/v1.6.0...v1.7.0
[1.6.0]: https://github.com/ofabel/mp-flipper/compare/v1.5.0...v1.6.0
[1.5.0]: https://github.com/ofabel/mp-flipper/compare/v1.4.0...v1.5.0
[1.4.0]: https://github.com/ofabel/mp-flipper/compare/v1.3.0...v1.4.0
[1.3.0]: https://github.com/ofabel/mp-flipper/compare/v1.2.0...v1.3.0
[1.2.0]: https://github.com/ofabel/mp-flipper/compare/v1.1.0...v1.2.0
[1.1.0]: https://github.com/ofabel/mp-flipper/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/ofabel/mp-flipper/compare/v0.5.0-beta.1...v1.0.0
[0.5.0-beta.1]: https://github.com/ofabel/mp-flipper/compare/v0.4.0-beta.1...v0.5.0-beta.1
[0.4.0-beta.1]: https://github.com/ofabel/mp-flipper/compare/v0.3.0-alpha.1...v0.4.0-beta.1
[0.3.0-alpha.1]: https://github.com/ofabel/mp-flipper/compare/v0.2.0-alpha.1...v0.3.0-alpha.1
[0.2.0-alpha.1]: https://github.com/ofabel/mp-flipper/compare/v0.1.0-alpha.1...v0.2.0-alpha.1
[0.1.0-alpha.1]: https://github.com/ofabel/mp-flipper/releases/tag/v0.1.0-alpha.1
