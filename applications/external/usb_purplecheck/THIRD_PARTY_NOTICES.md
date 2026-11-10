# Third-party notices

`vendor/mass_storage_usb.c`, `.h`, `mass_storage_scsi.c`, `.h` are derived from the Flipper Devices **flipperzero-good-faps** repository, `mass_storage/helpers`, GPL-3.0:

https://github.com/flipperdevices/flipperzero-good-faps/tree/dev/mass_storage/helpers

The original project's authors and contributors retain their rights. Modified by Roger F5 on 2026-11-10: bounded 1 KiB transfer buffers, command-wrapper validation, allocation initialization, read-only SCSI support, capacity response correction, fixed product identity and safe USB restoration handling. The supplied source includes the modifications. The complete GPL-3.0 text is in LICENSE.

Flipper firmware APIs and SDK are external build dependencies. No SDK, firmware image or toolchain is bundled in the distributable package.
