![License](https://img.shields.io/github/license/ofabel/mp-flipper)
![Version](https://img.shields.io/github/v/tag/ofabel/mp-flipper)
![](https://img.shields.io/github/issues/ofabel/mp-flipper)

# MicroPython Flipper Zero

Allows you to use the power of Python natively on your Flipper Zero.
The application is available on the official [Flipper Lab](https://lab.flipper.net/apps/upython).
For details on how to programm your Flipper with Python, check out the [documentation](https://ofabel.github.io/mp-flipper/) on GitHub pages.

![MicroPython REPL](./docs/pages/assets/repl.gif)

## Disclaimer

This FAP version requires about 80 kB from SRAM to start (needed for the Python runtime and compiler).
Due to memory fragmentation it's possible, that the application crashes when you start it.
If this happens, just try again (the crash doesn't harm your device).

> [!IMPORTANT]
> This problem is already addressed to the firmware developers in [this issue](https://github.com/flipperdevices/flipperzero-firmware/issues/3927).
> Nevertheless, running the uPython application from the SD card is still a heavy task for the Flipper.

_I'm thinking about publishing a fork of the original firmware with uPython bundled as a core service._
_This would mitigate all the memory problems._
_The SD card version would still be there and maintained, but possibly with a limited set of features._

## Development

This section is only relevant, if you want to build the FAP on your own.

### Branches

This branch contains the [FAP](https://developer.flipper.net/flipperzero/doxygen/apps_on_sd_card.html) version.
The results of the preceding research phase is still available in the [poc](https://github.com/ofabel/mp-flipper/tree/poc) branch.
The [lib](https://github.com/ofabel/mp-flipper/tree/lib) branch of this repository contains just the [MicroPython](https://github.com/micropython/micropython) library.
The progress of further research on what can be achieved when moving functionality to the firmware can be found in the [fork of the original firmware](https://github.com/ofabel/flipperzero-firmware/tree/ofa/micropython).

### Requirements

* [Git](https://git-scm.com/)
* [Make](https://www.gnu.org/software/make/)
* [uFBT](https://pypi.org/project/ufbt/) available in your `PATH` (or you have to adjust the [Makefile](./Makefile))
* [Flipper Zero](https://flipperzero.one/)

### Setup

```bash
git clone --recurse-submodules git@github.com:ofabel/mp-flipper.git
```

### Build

Just open a terminal and run the Makefile targets:

```bash
make build
```

You can also build an launch the application on the attached Flipper Zero device in one command:

```bash
make launch
```
