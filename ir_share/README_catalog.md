# Flipper Share - direct file transfer between flippers

## Overview

Flipper Share is a wireless-enabled file sharing application for Flipper Zero.

It allows to send any file over a Sub-GHz via internal transmitter directly from one Flipper Zero to another without any additional hardware, cables, smartphones, computers, internet connection and magic needed.
File size tested is 16 MB (6.5 hours), but the protocol itself can support up to 4 GB (uint32_t file size).

Features:

- Works from out of the box on any Flipper Zero, the simplest possible way to transfer files directly
- Multiple receivers supported simultaneously and works just fine (broadcast)
- Continuation of download / auto retries in case of packet loss is guaranteed at the protocol level
- Integrity check with MD5 hash after file reception
- Actual speed is around 700 Bytes/sec (42 KB/min), that allows to transfer average .fap file from one Flipper to another in less than 1 minute
- No pairing or explicit session establishment needed
- No encryption, anyone nearby can receive the file, please don't send sensitive data
- Fun torrent-like progress bar showing completely received parts of the file instead of boring usual percentage scale

# Notes

See the full [README.md](https://github.com/lomalkin/flipper-zero-apps/blob/-/flipper_share/README.md) for more details and Flipper Share protocol description.

Source code of the latest version is [here](https://github.com/lomalkin/flipper-zero-apps/blob/-/flipper_share). Please feel free to open an issues and PRs if you have any ideas or found bugs.

Follow the app news on Telegram channel [@flipper_share](https://t.me/flipper_share).


# Credits

Special thanks to [@Skorpionm](https://github.com/Skorpionm/) for building a solid foundation with the Sub-GHz packet abstraction layer API — it made this app possible, convenient, and reliable.


