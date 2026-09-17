# IR Builder for Flipper Zero

IR Builder creates a working TV remote when the original remote is unavailable. Use a signal position found with Universal Remotes, or let IR Builder scan the candidates and pause when the TV responds.

The app includes its own TV signal library. It never replaces the firmware's stock Universal Remotes library.

## Features

- Eight primary controls: Power On, Power Off, Volume Up, Volume Down, Channel Up, Channel Down, Mute, and Unmute.
- Eight navigation controls: Up, Down, Left, Right, OK, Menu, Back, and Input.
- Fast automatic scanning with pause, adjacent stepping, replay, resume, and selection.
- Accelerated manual position entry when Left or Right is held.
- Portrait controller designed for pointing the Flipper's infrared emitter at a TV.
- Saved projects can be reopened, edited, renamed, duplicated, and deleted.
- Open an existing `.ir` remote and automatically map its common controls.
- Browse unmatched imported signals on a third **Other buttons** screen.
- Standard Flipper infrared exports under `infrared/ir_builder`.
- Optional import of up to 48 unmatched buttons from another infrared file.
- Bundled library with 1,593 signals; no separate download is required.

## Install

Install IR Builder from the Flipper Apps Catalog when available.

For manual installation, copy `ir_builder.fap` to `apps/Infrared` on the Flipper SD card and open **Apps > Infrared > IR Builder**.

## Create a remote

1. Open **New remote**, or choose **Open .ir** to build from working signals you already have.
2. Choose a control on the first page, or open **Nav** for navigation controls.
3. Enter a known one-based position, test individual candidates, or start **Auto scan**.
4. Pause as soon as the TV reacts, replay the candidate if needed, and choose **Use number**.
5. Repeat for the controls you need.
6. Open **Menu > Name & save**.

The exported remote appears under `infrared/ir_builder` in the standard Infrared app. IR Builder keeps editable `.irb` project metadata in its private app-data directory.

When opening an existing remote, IR Builder recognizes common names for Power, Volume, Channel, Mute, directions, OK, Menu, Back, and Input. Those signals populate the designed controller pages. Other signals keep their source order on the list reached through **Nav > Other**. The source file is copied into private app storage, so the project remains usable if the original file is moved.

## Signal library

The bundled `tv_builder.ir` begins with the stock 815 Universal TV records and adds 778 deduplicated navigation candidates. Candidates found in more source remotes are tried earlier.

New projects use the bundled library automatically. **Settings > Change .ir** selects another library, while **Reset builder** restores the bundled default. Existing projects continue using the library stored in their project metadata.

## Build

Install the official uFBT tool, open this repository in a terminal, and run:

```text
ufbt
```

The app targets Flipper Zero f7. The Apps Catalog builds releases against its supported firmware SDKs.

## License and data

The application source is GPL-3.0. See [NOTICE.md](NOTICE.md) for the signal database and upstream firmware credits.
