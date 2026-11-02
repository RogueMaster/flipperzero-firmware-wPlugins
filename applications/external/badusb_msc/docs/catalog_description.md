# BadUSB MSC

BadUSB MSC combines a BadUSB keyboard script runner and a dedicated USB mass-storage drive on Flipper Zero.

## Features

- Run supported BadUSB scripts with a selectable keyboard layout.
- Expose an isolated 8 MiB FAT16 storage volume named BADUSBMSC while using BadUSB over USB.
- Transfer files both ways using built-in computer file-management commands, without a separate PC transfer application.
- Browse, read, edit, rename and delete supported local files from the Flipper screen.
- Automatically create the virtual disk on first use.

## Important notes

For USB storage safety, eject BADUSBMSC on the computer before closing the application. Mirroring to the qFlipper-accessible folder currently supports root-level short 8.3 filenames. Only run scripts on computers you own or are authorized to use.

This is a community project based on GPL-3.0 Flipper Devices code, not an official Flipper Devices product.
