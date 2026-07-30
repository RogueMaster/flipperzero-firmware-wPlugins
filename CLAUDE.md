# CLAUDE.md

Guidance for working in this repo. Read the README for what the app does; this file is
about what breaks and why.

## Two halves

| half | language | built with | runs on |
| --- | --- | --- | --- |
| `wol_*.c`, `wol_*.h` | C | ufbt | Flipper Zero (STM32WB55) |
| `esp32/src/main.cpp` | C++/Arduino | PlatformIO | ESP32-S2 dev board |

They talk over USART at 115200 with a tab separated line protocol defined in the header
comment of `esp32/src/main.cpp`. `wol_esp.c` is the client side. Changing one side means
changing both, and bumping `WOL_FW_VERSION` if the wire format moves.

## Build

```
python -m ufbt                    # dist/wol_flipper.fap
python -m platformio run -d esp32 # $TEMP/wol-flipper-esp/build/wifi_devboard/*.bin
```

Neither tool is on PATH; call them through `python -m`. Always rebuild the fap after
touching anything, the SDK compiles with `-Werror` and catches a lot.

After changing the ESP firmware: rebuild with PlatformIO, copy `bootloader.bin`,
`partitions.bin` and `firmware.bin` into `fw/`, then rebuild the fap so the new images
get packed into it.

There is no hardware here. Nothing in this repo has ever run on a real Flipper or a real
dev board. Do not claim otherwise; a clean build is the only verification available.

## Traps that already cost time

**ufbt globs sources recursively.** `sources=["wol_*.c"]` in `application.fam` is load
bearing. The default `["*.c*"]` drags `esp32/src/main.cpp` into the FAP build.

**PlatformIO's `.pio` directory breaks ufbt app discovery** in a way that makes it report
"Found nothing to build" with a bogus warning about a missing manifest. That is why
`platformio.ini` sets `build_dir` outside the repo. Do not point it back inside.

**A FAP is loaded into RAM and the Flipper has 256 KB total.** Keep `.text` + `.rodata`
small. Two consequences already baked in:

* `wol_loader_glue.c` replaces the vendored stub table with an ESP32-S2 only one. The
  upstream table references all eleven chip stubs, roughly 90 KB of blobs. It also stubs
  out `esp_loader_get_spi_ops` / `esp_loader_get_sdio_ops` so `protocol_spi.c` and
  `protocol_sdio.c` do not have to be compiled, and defines an empty
  `esp_stub_esp32p4rev1`, which the library references unconditionally.
* the ESP firmware images are file assets, not `.rodata`. `fap_file_assets="fw"` packs
  them into a `.fapassets` section that carries no ELF `ALLOC` flag, so the loader
  streams it to `/ext/apps_assets/wol_flipper/` instead of mapping it. Verify with
  `arm-none-eabi-readelf -S dist/wol_flipper.fap` after touching the manifest: if
  `.fapassets` ever shows an `A` flag, the app will not fit in RAM.

**The dev board can be put into its bootloader from the Flipper.** Header pin 7 (PC3) is
DTR and pin 6 (PB2) is RTS on the official board, and the library calls
`ops->enter_bootloader` once per `esp_loader_connect()`. An earlier version of this app
shipped no-op hooks and a scene telling the user to press BOOT+RESET by hand, which was
simply wrong. Third party boards do leave those pins unconnected, so the manual sequence
stays documented as the fallback.

**No `strlcpy` in the SDK libc.** Use `wol_strcpy()` from `wol_config.c`.

**`popup_set_text()` stores the pointer, it does not copy.** Anything passed to it must
outlive the popup: string literals, statics, or `app->flasher_status`. The same caution
is not needed for submenu labels, but this code uses static buffers there anyway.

**esp-serial-flasher is pinned at v2.0.0** in `lib/`, with `.git`, tests, examples and
the zephyr port removed. v2 replaced the old weak symbol port API with an
`esp_loader_port_ops_t` vtable plus a `esp_loader_t` handle; anything found online about
`loader_port_write()` refers to v1 and does not apply. If the library is ever updated,
re-check `wol_loader_glue.c` against the new stub table and protocol getters.

## Threading

Serial work never happens on the GUI thread. `wol_scene_send.c` and
`wol_scene_flasher.c` each spawn a `FuriThread`, and the scene's `on_exit` sets
`app->worker_cancel` then joins it. Anything blocking added to those workers must poll
that flag, or backing out of a scene will hang the UI for the length of a timeout.

Workers never touch view models. They post progress with
`view_dispatcher_send_custom_event()`, packing everything into the event word:

* `WOL_EVENT_SEND(step)` for the wake flow
* `WOL_EVENT_FLASH(stage, percent)` and `WOL_EVENT_FLASH_DONE(result)` for the flasher

The one exception is `app->flasher_info`, written by the worker before it posts the done
event and read by the GUI thread after receiving it. The queue provides the ordering.

Scenes whose menus feed on custom events must range check `event.event` before treating
it as a menu index, otherwise a late progress event from a worker that outlived its scene
lands in the wrong handler. See the guards at the top of `wol_scene_start.c`.

## Licensing

This project is MIT. Do not paste code from `0xchocolate/flipperzero-esp-flasher`, it is
GPL-3.0 and would relicense the whole app. The vendored `lib/esp-serial-flasher` is
Apache 2.0; keep its `LICENSE`, never edit its sources in place, and if the set of
removed or replaced files changes, update the modification list in the README, which
Apache 2.0 section 4(b) requires.

## Style

Match the surrounding code: 4 spaces, `wol_` prefix on everything global, one scene per
file, comments only where the reason is not obvious from the code. No em dashes in files
that a human reads.
