# flipper_tv_remote

Turn your Flipper Zero into a fully functional TV remote. Record buttons from any IR remote and replay them from your Flipper — no original remote needed.

## What it does

flipper_tv_remote lets you save the IR signals from any TV remote and replay them through your Flipper Zero's built-in IR blaster. You can store as many remotes as you like, each with a name you choose.

The goal was a single screen that could fully replace a TV remote — all the most commonly used buttons available instantly, no menus to navigate mid-use. The d-pad, OK button, and Back button each handle two actions (short press and hold), with a double-tap on Back for Power, which makes it possible to cover the buttons you actually reach for most often in a way that feels natural to use.

The app is built around a concentric-circle UI — the same layout as a real TV remote — and supports both portrait (vertical) and landscape (horizontal) orientations so you can hold the Flipper whichever way feels most comfortable.

Key capabilities:

- **Multiple named remotes** — save as many remotes as you like, each stored as a separate **.ir** file on the SD card
- **Learn mode** — record up to 12 buttons (Power, Vol+, Vol−, Ch+, Ch−, Up, Down, Left, Right, OK, Back, Home) from any IR remote
- **Concentric-circle remote UI** — tap or hold the d-pad to send signals; short-press and hold trigger different IR commands
- **Double-tap Back for Power** — quickly tap Back twice to send the Power signal
- **Two screen orientations** — Vertical (portrait, Flipper rotated like a tall remote) or Horizontal (landscape, circle remote on the right) — preference is saved and restored on restart
- **Button Map** — in-app reference card showing every button's press and hold action
- **Compatible .ir files** — saved remotes use the standard Flipper IR format and work with the built-in Infrared app

## Installation

Pre-built releases are available on the [Releases page](https://github.com/jmanion0139/flipper_tv_remote/releases).

Download the latest **flipper_tv_remote.fap** and copy it to **apps/Infrared/** on your Flipper's microSD card. The app will appear under **Applications → Infrared → TV Remote**.

Want to build from source? See [docs/build-instructions.md](docs/build-instructions.md).

## How to use

## Main menu

| Item | What it does |
|---|---|
| **Learn Remote** | Record a new remote or overwrite an existing one |
| **Use Remote** | Choose a saved remote and control your TV |
| **Delete Remote** | Remove a saved remote from the SD card |
| **Button Map** | View the full button reference (press vs hold) |
| **Settings** | Change the screen orientation |
| **About** | Author and license info |

## Learning a remote

1. Select **Learn Remote** from the main menu.
2. Choose **New Remote** and enter a name (e.g. living_room), or choose **Update Remote** to re-record an existing one.
3. The app walks you through each button one at a time. When prompted, point your original remote at the **IR window at the top of the Flipper** and press the button.
4. After each signal is received:
   - Press **OK** to accept and move to the next button
   - Press **Right (→)** to skip a button you don't want to record
   - Press **Left (←)** to reject and try again
5. When all 12 buttons are done, the remote is saved automatically.
6. Press **Back** at any time to stop early — buttons already recorded are saved.

Keep the original remote within about 30 cm of the Flipper's IR window with a clear line of sight for the best results.

## Using your Flipper as a remote

1. Select **Use Remote** from the main menu.
2. Pick the remote you want to use from the list.
3. Use the d-pad and OK button to send IR signals. The concentric-circle UI highlights the active ring segment to show which action is being sent.

## Button reference

| Physical key | Short press (Default) | Hold (Default) |
|---|---|---|
| Up | Up | Vol Up |
| Down | Down | Vol Down |
| Left | Left | Ch Down |
| Right | Right | Ch Up |
| OK | OK | Home |
| Back | Back (IR signal) | Exit Remote |
| Back × 2 (double-tap) | Power | — |

When **Buttons** is set to **Swapped** in Settings, the short press and hold actions for the d-pad are reversed — short press sends Vol/Ch and hold sends Up/Down/Left/Right.

You can also open the **Button Map** from the main menu to see this reference any time while using the app.

## Screen orientation

Open **Settings** from the main menu to configure:

| Setting | Options | Description |
|---|---|---|
| **Orientation** | Vertical / Horizontal | **Vertical** — Flipper rotated 90°, holds like a tall remote with the d-pad on the right. **Horizontal** — Flipper held normally in landscape, circle on the right, Back/Power on the left. |
| **Buttons** | Default / Swapped | **Default** — short press sends directional signals (Up/Down/Left/Right), hold sends volume and channel. **Swapped** — short press sends volume and channel, hold sends directional signals. |

Use **Up/Down** to navigate between settings and **Left/Right** (or **OK**) to change the selected value. All settings are saved to the SD card and restored automatically on next launch.

## File storage

Remotes and settings are saved to the **infrared/** folder on the SD card:

| Path | Contents |
|---|---|
| SD:/infrared/tv_remote_\<name\>.ir | Recorded IR signals for a named remote |
| SD:/infrared/tv_remote_settings.dat | App settings (orientation preference) |

.ir files use the standard Flipper IR format and are fully compatible with the built-in Infrared app.

## License

This project is licensed under the [GNU General Public License v3.0](LICENSE).
