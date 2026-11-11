# A/C Master for Flipper Zero

**Current release: v0.3.0 Beta.** Protocol compatibility varies by model; the application does not guarantee support for every air conditioner.

A multi-profile infrared controller for compatible air conditioners. A/C Master can send direct state commands for supported protocols, guide protocol discovery in a room, or learn and replay basic signals from an existing remote.

## Features

- Control power, temperature, mode and fan speed when a profile has a supported decoded protocol.
- Save multiple unit profiles and give each one a useful name.
- Identify a unit by manufacturer and try the bundled protocol variants with a simple Yes/No confirmation.
- Learn and replay Normal and Off signals for units without direct state control.
- Store profiles on the Flipper Zero microSD card.

## Supported protocol families

The bundled protocol library includes Mitsubishi Electric, Mitsubishi Heavy Industries (MHI), Gree, Fujitsu and LG variants.

- **Mitsubishi Electric:** the 144-bit pathway is the confirmed baseline. The 112-bit and 136-bit pathways are included but need further hardware verification.
- **MHI:** 88-bit and 152-bit pathways are included. The MHI control path has been confirmed on a local unit.
- **Gree (experimental):** YAW1F, YBOFB and YX1FSF variants are included. Compatibility with older Gree air conditioners remains unresolved; some tested older units did not respond.
- **Fujitsu:** 0x30, compact 0xFC and 0x31 variants are included. A local hardware test produced a working response; broader model compatibility is not verified. Fujitsu swing commands are deliberately excluded from probes.
- **LG (experimental):** the wizard tries LG 28-bit followed by LG2 28-bit, with confirmation after each probe. LG operation has been confirmed on one physical unit. Confirmed profiles use direct power, temperature (16-30 C), Heat/Cool and Auto/Low/Medium/High fan controls through the unchanged main screen. Broader compatibility and model-dependent fan encodings require further testing.

Protocol compatibility can vary between models. A successful probe should be confirmed by the user before saving a profile. If direct state control is unavailable, learned profiles can replay the saved Normal and Off signals; temperature, mode and fan controls are not available for those profiles.

## Known limitations

- Some older Gree units do not respond to the included protocols. Gree support remains experimental.
- Not all manufacturer models use the same IR variant; success with one unit does not guarantee compatibility with another.
- Learned signals support basic ON/OFF replay only; temperature, mode and fan control require a supported decoded protocol.
- The Flipper does not receive operating-state confirmation from the A/C; on-screen settings represent requested settings.

## Controls

On the control screen:

- Up / Down: adjust temperature.
- Left / Right: adjust fan speed.
- Short OK: toggle power.
- Long OK: change mode when supported.
- Double-click OK: open or leave Select Unit.

On Select Unit:

- Left / Right: browse saved units.
- Short OK: use the selected unit.
- Down: start adding a unit.
- Long OK: open Rename / Delete.

## Add a unit

Choose **Identify Unit** to select a manufacturer and try its known protocol variants. A/C Master sends a test state and asks whether the unit responded. Confirm a successful result to save the profile. If identification does not work, choose **Learn Remote** to capture Normal and Off signals from an existing remote.

## Build and run

Install uFBT, then run from the project directory:

```sh
ufbt
```

The FAP is created in `dist/gbhobbytech_ac_multi_remote.fap`. To build, upload and launch over USB, run:

```sh
ufbt launch
```

To install it persistently, copy the FAP to **SD Card/apps/Infrared** with qFlipper.

## Screenshots

Captured from the running app with qFlipper:

![A/C Master main control screen](screenshots/Main%20Screen.png)
![Unit selection screen](screenshots/Select%20Unit%20Screen.png)
![Manage unit screen](screenshots/Manage%20Unit.png)
![Add unit wizard](screenshots/Unit%20Add%20Wizard.png)
![Setup wizard](screenshots/Setup%20Wizard.png)

## Profile storage

Profiles are stored under `apps_data/gbhobbytech_ac_multi_remote` on the Flipper Zero microSD card.

## Protocol references

Protocol reference sources and licence considerations are documented in [docs/PROTOCOL_REFERENCES.md](docs/PROTOCOL_REFERENCES.md).

## License

A/C Master is licensed under the MIT License. See [LICENSE](LICENSE).
