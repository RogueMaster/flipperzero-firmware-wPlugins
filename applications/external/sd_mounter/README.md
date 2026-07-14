# Flipper Zero SD card mounter

This is a small Flipper Zero app that lets you connect the internal micro SD card reader directly to your computer.
Since the raw contents of the card are passed directly to the computer, it means that filesystems not normally supported by the Flipper can be mounted.

Due to Flipper Zero hardware limitations, SD card access speeds are very slow. Unfortunately, there's nothing that can be done about this in software.
The fastest speeds I've seen are 300kB/s read and 150kB/s write, although your results may vary.

## Warning: This app does some tomfoolery with the firmware's SD card driver. While there shouldn't be any issues with this, if you use this app, you take full responsibility for any corrupted or deleted data. Backups are always recommended.
