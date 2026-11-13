# Flipper Zero IR Identifier

![iridentifier](irid.png)

Flipper Zero simple infrared signal identifier, read the IR signal and search any match in the Flipper and User databases.
**IR Identifier** is a Flipper Zero app for identifying the likely device type and brand of an infrared remote control. It listens for an IR transmission, compares the captured signal against built-in rules and available `.ir` databases, then displays possible matches ranked by confidence.

> **Note:** Identification is best-effort. A result is a likely match, not a guarantee that the remote or device has been identified correctly.

## Features

- **IR signal scanning** — point an infrared remote at the Flipper Zero and press a button.
- **Built-in identification rules** — recognizes selected protocol/address combinations for common devices.
- **Universal Flipper IR database search** — searches the built-in IR assets in `/ext/infrared/assets`.
- **Optional user database search** — searches `.ir` files under `/ext/infrared`, including subfolders.
- **RAW signal matching** — compares long RAW captures against available RAW entries and uses approximate header timings to estimate some air-conditioner brands.
- **Multiple results** — stores and ranks up to 24 database candidates so you can browse alternatives.
- **Match confidence indicator** — shows High, Medium, or Low according to the match score.
- **Scan resume** — if you interrupt a database scan with Back, you can resume it from the interrupted point.
- **Search logs** — save successful searches, browse previous logs, and delete individual or all logs.
- **Settings** — enable or disable the user database and choose whether logs are saved automatically, never, or after confirmation.

## Identification sources

The app uses these sources:

1. **Built-in table:** known protocol and address combinations.
2. **Flipper universal IR assets:** files in `/ext/infrared/assets`; the file/path is used to infer the device type.
3. **User `.ir` files:** files under `/ext/infrared`; brand and device type may be inferred from folder and file names.
4. **RAW timing heuristic:** approximate header timings can suggest a brand for some long signals, especially air conditioners.

Results are ranked using the match score, whether the brand is recognized, brand ranking, whether the device type is known, and the source of the result.

### Confidence levels

- **High:** the signal matches both the protocol/address and command, or reaches the app's highest match score.
- **Medium:** a partial match, such as a protocol/address match or a similar RAW signal.
- **Low:** a heuristic estimate or an uncertain identification.

These labels describe the app's matching logic; they do not guarantee compatibility.

## Installation

1. Build the app for your Flipper Zero using a compatible Flipper Zero firmware development environment.
2. Install the resulting `.fap` file on the device.
3. Open **Apps** and find **IR Identifier** in the **Infrared** category.

The app's application metadata uses the app ID `ir_identifier` and the category `Infrared`. Build instructions depend on the firmware SDK and project layout used for your build.

## How to use

1. Open **IR Identifier**.
2. Select **Scan** and press OK.
3. Point your remote at the Flipper Zero's infrared receiver.
4. Press a button on the remote and wait for the scan to finish.
5. Browse the results to compare possible brands and device types.
6. If needed, enable **User DB** in **Settings** and scan again.

For best results, use a clear transmission from the original remote and make sure the remote is aimed toward the Flipper Zero.

## Controls

### Main menu

| Button | Action |
| --- | --- |
| Up / Down | Select a menu item |
| OK | Open the selected item |
| Back | Exit the app |

Menu items: **Scan**, **Log**, **Settings**, and **About**.

### Scanning and results

| Button | Action |
| --- | --- |
| Back while waiting | Return to the main menu |
| Back while scanning | Stop the scan and show the results found so far |
| Left / Up | Previous result |
| Right / Down | Next result |
| OK on the results screen | Start a new scan |
| Back on results | Resume an interrupted scan, or leave the results and begin another scan |

When the log mode is set to **Ask**, the app asks whether to save a successful search when leaving the results screen.

### Settings

- **User DB:** turn the search of user `.ir` files on or off.
- **Log:** choose `Yes`, `No`, or `Ask`.
  - `Yes` — automatically save successful searches.
  - `No` — never save logs.
  - `Ask` — ask before leaving the results screen.
- **Clear all logs:** delete all saved logs after confirmation.

Use Up / Down to select a setting. Use Left / Right or OK to change the selected value. Follow the on-screen confirmation prompts for deletion.

### Log browser

| Button | Action |
| --- | --- |
| Up / Down | Select a log or scroll through its contents |
| OK | Open a log; in the viewer, advance by a page |
| Left | Request deletion of the selected log; in the viewer, move back a page |
| Right | Advance by a page in the viewer |
| Back | Return to the previous screen |

The log list displays up to the 40 newest log files.

## File locations

| Purpose | Path |
| --- | --- |
| Flipper universal IR assets | `/ext/infrared/assets` |
| User IR database | `/ext/infrared` |
| App logs | `/ext/apps_data/ir_identifier/` |

Logs are plain-text files named like:

`log_YYYYMMDD_HHMMSS.txt`

Each log records the timestamp, signal details, number of results, match confidence, and the full path of each matching `.ir` file where available. The app keeps up to 40 logs in its list.

## Limitations

- Not every remote or IR protocol can be identified.
- A brand inferred from a folder or filename may be wrong or incomplete.
- RAW matching compares signal timing patterns and does not fully decode every protocol.
- The air-conditioner brand detector uses approximate timing fingerprints and should be treated as a suggestion.
- A matching database entry does not necessarily mean every button or function on the remote will work with the target device.
- Search results depend on the IR assets and user `.ir` files installed on the SD card.

## Credits

Created by **Marcopiiit**.

Version shown in the app: **1.0**.

