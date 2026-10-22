# Third-party notices

## Flipper Internet Bridge client SDK

`vendor/internet_bridge/` is a copy of the client SDK of
[Flipper Internet Bridge](https://github.com/mete888/flipper_internet_bridge), exported with its
`scripts/vendor_bridge_sdk.py`. The only change is whitespace: the sources were reformatted with the
Flipper Zero `.clang-format` so the project passes the catalog's lint check.

- Copyright (c) 2026 mete888
- MIT License – see [`licenses/flipper-internet-bridge-MIT.txt`](licenses/flipper-internet-bridge-MIT.txt)

## FlipperHTTP

`vendor/flipper_http/` contains `flipper_http.c` and `flipper_http.h` from
[FlipperHTTP](https://github.com/jblanked/FlipperHTTP) (commit `d46118c`). Changes: the include of its
own header points to the local file, and the sources were reformatted with the Flipper Zero
`.clang-format`.

- Copyright (c) 2024 JBlanked
- MIT License – see [`licenses/FlipperHTTP-MIT.txt`](licenses/FlipperHTTP-MIT.txt)

## Inter typeface

`yt_fonts.c` contains glyphs of [Inter and Inter Display](https://rsms.me/inter/) (v4.1),
rasterized to 1-bit bitmap fonts for the Flipper Zero display.

- Copyright (c) 2016 The Inter Project Authors
- SIL Open Font License 1.1 – see [`licenses/Inter-OFL.txt`](licenses/Inter-OFL.txt)

The bitmap fonts are a modified version of Inter and are not distributed under the name "Inter"
as a standalone font.

## Trademarks

YouTube is a trademark of Google LLC. This project is not affiliated with or endorsed by Google.
