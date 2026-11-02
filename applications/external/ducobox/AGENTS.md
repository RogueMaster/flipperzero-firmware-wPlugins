# AGENTS.md

Instructions for AI agents working in this repository. Human-facing documentation lives in [`README.md`](README.md); keep explanations and procedures for people there, not here.

## What this is

Flipper Zero FAP ("Ducobox") that emulates a Ducobox RF remote. On first run it sweeps the Duco band to find the channel, learns one remote frame as a template, then synthesises Speed 1/2/3/Auto commands from that template.

Target: Flipper Zero (f7). The test device runs Momentum `mntm-012`, firmware API **87.1**. The FAP must be built for the device's API or the loader reports "app too new".

## Git and publishing

- **Do not commit or push by yourself.** Only commit, push, create remotes, or publish the repository when the user explicitly asks. Do not amend, force-push, or rewrite history unless the user asks.

## General

- **Keep it simple.** Solve the problem at hand in the simplest conventional way. Check whether this repository already solves it, then search the web, before writing new code.
- **Prefer packaged tools.** Before hand-rolling something, check nixpkgs and the Flipper SDK. Use `nix-shell -p <package> --run ...` for one-off tools instead of installing them system-wide.
- **Don't hard-wrap prose.** Keep paragraphs on one line in Markdown, docs and comments; let the editor wrap them.
- **Preserve relevant comments when refactoring.** Keep comments that still apply and update them when the context changes.
- **Avoid inline multi-step scripts.** Put reusable shell or Python sequences in real script files (`screen`, `sdk`) and call those; do not bury long snippets in configs or docs.
- **Ask before complex workarounds.** If a supported setting or API cannot do the job, confirm before overriding source files, repackaging, or taking a fragile route.
- **Never commit secrets.** Do not add tokens, passwords, private keys, credentials, or personal data to the repository. Keep them in the user's secret store or an untracked local file; if a secret appears in the working tree, stop and remove it before committing.
- **Security first.** Choose the safer option when changing configs or scripts: least privilege, validated inputs, and safe defaults.
- **Keep `AGENTS.md` agent-facing.** Put human-facing explanations and procedures in `README.md`; the store-facing summary lives in `CATALOG.md`.

## Build and run

All commands run from the repository root. Use this project's `nix-shell`; the Flipper toolchain is not available on the host.

```bash
nix-shell       # FHS build shell
make            # build dist/ducobox.fap; APPCHK must print API: 87.1
make launch     # install to /ext/apps/Sub-GHz/ducobox.fap and launch
make screen     # capture the current screen to /tmp/ducobox-screen.png
make clean      # remove build artefacts
make update     # re-fetch the pinned SDK
```

- `nix-shell --run "make launch"` works: `buildFHSEnv` would normally replace the outer shell and drop the command, so `shell.nix` recovers it from nix-shell's temporary rcfile and runs it inside the FHS. Plain `nix-shell` still gives an interactive shell.
- `shell.nix` is intentionally a `buildFHSEnv`: the Flipper SDK is generic Linux (Python 3.11 + ARM GCC) and stock NixOS `/lib64` is the stub loader. Do not switch it back to `mkShell` unless the system has a working generic ELF loader (nix-ld).
- `shell.nix` imports `<nixpkgs>` from `NIX_PATH`. Do not reintroduce the old `../server/nixos-version` path.
- SDK pin: `FW_VERSION ?= 1.4.3` in `Makefile` (API 87.1). `./sdk` installs it into the gitignored `.ufbt/current`; `.ufbt/toolchain` is a symlink to `~/.ufbt/toolchain`.
- Change `FW_VERSION` only when the device firmware/API changes. Check the device with `ufbt cli` → `info device` (`firmware.api.major/minor`).
- `make launch` and `make screen` need exclusive access to `/dev/ttyACM0`. Close qFlipper or any other serial user if the port is busy.
- `.ufbt/`, `dist/`, and `.vscode/` are generated or gitignored. Never commit them; do not hand-edit `.ufbt` SDK state.

## Source layout

| Path | Purpose |
| --- | --- |
| `application.fam` | FAP manifest: appid `ducobox`, entry `ducobox_app`, 4 KiB stack, icon `ducobox.png`, author/description, `targets=["f7"]`, sources `ducobox_app.c` and `duco_radio.c`. |
| `ducobox.png` | 10x10 1-bit FAP icon; source and license in `ICON.md`. |
| `LICENSE` | MIT license for the app. The icon attribution is separate in `ICON.md`. |
| `.clang-format` | Flipper C style used by `ufbt lint`; keep the sources formatted. |
| `CATALOG.md`, `changelog.md`, `manifest.template.yml` | Flipper Apps Catalog metadata. `manifest.template.yml` is tracked; copy it to the gitignored `manifest.yml` and fill in the public repo URL and commit SHA. |
| `screenshots/` | Apps Catalog screenshots; `ss0.png` is the current remote-screen capture. |
| `PUBLISHING.md` | Steps for publishing and updating the app in the Flipper Apps Catalog. |
| `ducobox_app.c` | All UI and state: setup sweep, template storage, config, remote screen, input, timer, RPC-free storage helpers. |
| `duco_radio.c/.h` | CC1101 preset, packet RX/TX, RSSI, retune. |
| `sdk` | Downloads/extracts the pinned firmware SDK, writes `ufbt_state.json`, symlinks the toolchain. |
| `screen` | USB RPC screenshot tool; nix-shell shebang with pyserial. |
| `shell.nix` | FHS build environment. |
| `Makefile` | `build`, `launch`, `screen`, `clean`, `update`, `sdk`, `help`. |

## Behavior contract

- Key map: Left = Speed 1, Up = Speed 2, Right = Speed 3, Down = Auto, OK = Config, Back = Exit.
- Tap a speed: temporary command for 15 minutes. Repeated taps of the same speed within 10 seconds send press counts 1 → 2 → 3 (15/30/45 minutes), capped at 3.
- Hold a speed for 5 seconds: permanent command. It fires at the 5-second mark while the key is still held (`ducobox_remote_tick`); the release does not send again (`hold_sent`).
- Auto is always auto: `permanent` is forced false and the press count is 1.
- Config contains exactly three items: **Re-learn remote**, **Delete remote** (with confirmation), **Exit**. There is no Advanced menu; do not reintroduce one without a user request.
- Template path: `/ext/apps_data/ducobox/remote.duco`, FlipperFormat with `Filetype: Ducobox RF`, `Version: 1`, `Frequency`, `Length`, `Crc`, `Data`. It is the only file the app uses.
- First run: sweep 868.244 / 868.294 / 868.326 / 868.350 / 868.394 MHz, starting at 868.294, 400 ms per step. The first decoded frame locks the channel. If that frame is already a valid template it is accepted; otherwise the screen shows "Remote found" and asks for one more press on the locked channel.
- Setup accepts only remote-to-box frames: message type `0x07`, command `0x22`, command length ≥ 6, source address ≠ 1, destination address = 1. This rejects the Ducobox's own `0x22` confirmation frames.
- The app sends one frame per action. The 4-bit counter increments 1..15 with `(counter % 15) + 1`.
- The saved template supplies the network ID, source/destination addresses, percentage and temperature byte for every synthesised frame.

## Duco RF and command encoding

- GFSK, 868.294 MHz nominal, 38.3835 kBaud, 20.63 kHz deviation, sync word `0xD391`, 4 preamble bytes, variable length, CRC on.
- CC1101 preset in `duco_radio.c`: `MDMCFG4=0xCA`, `MDMCFG3=0x83`, `MDMCFG2=0x13`, `MDMCFG1=0x22`, `DEVIATN=0x35`, `PKTCTRL0=0x05`, `PKTCTRL1=0x00`, `PKTLEN=0x20`, PATABLE `0xC0`.
- `PKTCTRL1=0x00` is deliberate: no CRC autoflush and no appended status bytes, so bad-CRC frames reach the app. The physical remote's CRC often fails the CC1101 check (`Crc: 0`); treat CRC as informational, not as a reason to reject a structurally valid template.
- `furi_hal_subghz_load_custom_preset()` stops at register `0x00`, so IOCFG2 cannot be in the preset; `furi_hal_subghz_set_frequency_and_path()` sets it.
- `duco_radio_retune()` idles, flushes the RX FIFO, retunes and re-enters RX. The flush prevents a frame received on the previous channel being attributed to the new frequency.
- `duco_radio_stop()` uses idle, not sleep. Sleeping adds wake-up and state-check pitfalls.
- Payload after the CC1101 length byte: `[0]=0x07`, `[1..4]=network ID`, `[5..6]=packed addresses`, `[7]=high nibble original destination, low nibble counter`, `[8]=RSSI`, `[9..10]=command lengths`, `[11]=0x22`, `[12]=percentage`, `[13]=mode`, `[14]=temperature`, `[15]=subscribe`, `[16]=button`.
- Mode byte = `0x08 | mode | (permanent ? 0x20 : 0)`. Mode values: `0x00` auto, `0x04` low, `0x05` middle, `0x06` high, `0x07` away.
- Button byte = `((presses & 0x03) << 3) | (permanent ? 0x04 : 0) | buttonNumber`. Button numbers: 0 auto, 1 low, 2 middle, 3 high.
- Synthesised frames copy bytes 0..10 from the template, set command lengths `0x60 0x00`, replace bytes 11..16, and use length 17. Speed 3 pulse/hold were verified byte-for-byte against physical captures.

## GUI gotchas

- `canvas_draw_str()` anchors text at the **baseline**. Use `canvas_draw_str_aligned(..., AlignTop)` for top-based layout. The remote screen uses AlignTop for the four rows, a FontSecondary header at y=0, and a separator line at y=10.
- The remote screen is a custom `View` with a locking `DucoboxRemoteModel` (`status[40]`, `status_until`). Input handles `InputTypePress`/`InputTypeRelease`; OK opens config on Press; Back returns false so the dispatcher exits.
- The 200 ms timer sends `DucoboxEventTick`. It refreshes setup and drives the 5-second hold check and status clearing. Stop the timer when leaving setup/remote.
- `view_dispatcher_stop()` is used for Config → Exit.

## Testing and diagnostics

- `make` must end with `APPCHK ... API: 87.1`. Any other API means the FAP will show "app too new" on the device.
- `make screen` writes `/tmp/ducobox-screen.png`; inspect it with the `read` tool. It talks the Flipper RPC protobuf protocol over USB (start session, StartScreenStreamRequest, ScreenFrame, u8g2 page-major framebuffer → 1-bit PNG) and uses no protobuf package.
- `loader info` over the CLI shows whether `Ducobox` is running. `storage list /ext/apps_data/ducobox` shows the template and any leftovers.
- `input send <key> <press|release>` over the CLI can simulate keys, but it sends real RF commands and can change the user's ventilation state. Do not use it without a reason.
- The USB serial CLI, `make screen`, and `make launch` cannot use the port simultaneously.
- The u8g2 frame is page-major: `data[page * 128 + x]`, bit 0 is the top pixel of the page. Do not treat it as row-major.

## Apps Catalog

- `ufbt format` and `ufbt lint` must pass before submitting. The catalog CI runs the same lint against the latest release SDK, not the local `FW_VERSION` pin.
- `CATALOG.md` and `changelog.md` must stay within the catalog's Markdown subset: no code blocks, tables, inline code, or headings deeper than level 2.
- `manifest.template.yml` is the catalog template. Copy it to the gitignored `manifest.yml`, then fill `sourcecode.location.origin` and `commit_sha` after pushing to a public GitHub repo; the catalog builds from that commit.
- The catalog builds with the latest release SDK (currently 1.5.1, API 88.2). The local `make` still targets API 87.1.
- `screenshots/ss0.png` must be 512x256 or 1024x512; the catalog validator downsizes it to 128x64.

## Known state on the test device

- FAP installed at `/ext/apps/Sub-GHz/ducobox.fap`, built for API 87.1.
- `remote.duco` is the learned template. The older `duco_*.duco` files are leftovers from the removed Advanced tools and are unused by the app.
- The app currently starts on the remote screen when `remote.duco` exists; otherwise it starts the first-run sweep.

## When to update this file

Update `AGENTS.md`, and the matching human-facing `README.md`, in the same change whenever any of these change:

- Build/toolchain/SDK pin/API target, FHS rationale, or Makefile targets.
- Key mapping, tap/hold semantics, config items, setup flow, or template validation.
- `.duco` format, template path, command encoding, CC1101 preset, or swept frequency list.
- Screen capture tooling or device paths.
- Application icon asset or its license/attribution (`ducobox.png`, `ICON.md`).
- Apps Catalog metadata or submission files (`manifest.template.yml`, `CATALOG.md`, `changelog.md`, `screenshots/`).

Keep this file concise and directive. If a statement here becomes stale, fix it rather than leaving a known-wrong instruction.
