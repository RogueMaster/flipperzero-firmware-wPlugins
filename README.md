# TamaConnect

TamaConnect is a Flipper Zero application for communicating with
Tamagotchi Connection 2024 devices over infrared.

It allows a Flipper Zero to act as another Tamagotchi during supported
Connection interactions.

## Features

### Visit

Connect with a Tamagotchi using a configurable virtual character.

### Game

Play the Points game with a real Tamagotchi.

Options include:
- selectable GP wager
- Tamagotchi wins
- Tamagotchi loses
- random result

### Gift

Send gifts directly to a Tamagotchi.

Supported categories:
- Food (IDs 0-35)
- Snacks (IDs 36-77)
- Items (IDs 78-131)
- Special effects (IDs 132-141)

Random options are also available:
- Random Gift
- Random Food
- Random Snack
- Random Item
- Random Special

### Profile

The virtual Tamagotchi profile can be configured with:
- name
- character
- gender

Settings are saved on the Flipper Zero SD card.

## Compatibility

Developed and physically tested with a Tamagotchi Connection 2024 re-release.

The application uses the Flipper Zero infrared transceiver and does not
require external hardware.

## How to use

### Visit

On the Tamagotchi, open the Connection menu and select the appropriate
Visit connection option.

Start Visit in TamaConnect and point the Flipper Zero infrared port toward
the Tamagotchi.

### Points Game

On the Tamagotchi:

1. Open `Connect`.
2. Select `Game`.
3. Select `Points`.
4. Leave the Tamagotchi waiting for a connection.

On TamaConnect select `Game -> Points`, choose the wager and desired result,
then point the Flipper Zero at the Tamagotchi.

### Gift

On TamaConnect choose `Gift -> category -> gift`.

The Flipper will wait for the Tamagotchi.

On the Tamagotchi:

1. Open `Connect`.
2. Select `Present`.
3. Wait for `STAND BY`.
4. Press the middle button so it changes to `CONNECT`.
5. Point the Tamagotchi toward the Flipper Zero.

The selected gift should then be transferred.

## Installation

Copy `tama_connect.fap` to:

`/ext/apps/Infrared/`

on the Flipper Zero SD card.

## Building

Install uFBT and run `ufbt`.

The resulting application is created at:

`dist/tama_connect.fap`

To build, upload and launch it on a connected Flipper Zero, run:

`ufbt launch`

## Tested features

The following functionality has been tested with real hardware:

- Visit
- configurable character, name and gender
- Points game
- selectable GP wager
- controlled win/loss result
- normal gifts
- special gift effects
- random gifts
- persistent settings

Examples confirmed during development include normal inventory items and
special effects such as Cone, Snake and Jack in the Box.

## Experimental / future work

Additional Connection game modes are present in the protocol but are not
considered fully tested yet.

Planned:
- additional game modes
- Marriage support

Marriage support will be added after it can be tested with suitable adult
Tamagotchi devices.

## Credits

TamaConnect was developed through reverse engineering and real-device testing
of the Tamagotchi Connection 2024 infrared protocol.

The Tamagometer project was an important reference during development.

Tamagotchi is a trademark of Bandai. This project is unofficial and is not
affiliated with or endorsed by Bandai.

## License

MIT
