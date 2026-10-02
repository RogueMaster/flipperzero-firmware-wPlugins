# NW Crawl

A tiny roguelike for the [Flipper Zero](https://flipperzero.one), set on the alphabet streets of NW Portland.

Start on Burnside and head north one street per floor (Couch, Davis, Everett, Flanders ... Thurman) until you reach the Witch's Castle in Forest Park.

## The game

- **Floors:** 19 streets plus the castle. Each is a randomly generated maze of rooms, corridors and side alleys with fog of war. The up-arrow tile leads to the next street.
- **Locals:** rats, crows, raccoons, rogue e-scooters and coyotes. Tougher ones show up the further north you go.
- **Pickups:**
  - Coffee is carried with you and heals 5 HP when you drink it.
  - A pink-box donut raises your max HP.
  - A hazy IPA raises your attack.
  - A used book reveals the floor's map.
- **Joe's Cellar:** a safe room on Pettygrove. Walk into the bartender for one stiff pour (full HP and +1 attack).
- **The Witch:** she hexes you from range, summons crows and vanishes when hit.
- **Sound:** square-wave effects that follow the Flipper's volume setting.

## Controls

| Button | Action |
| --- | --- |
| D-pad | Move; walk into something to attack it |
| OK | Drink a coffee, or wait a turn at full health |
| Hold Back | Quit |

## Build and install

You need [ufbt](https://github.com/flipperdevices/flipperzero-ufbt), the Flipper app build tool:

```bash
python3 -m pip install --upgrade ufbt
```

With the Flipper connected over USB (and qFlipper closed), build, install and start the game:

```bash
ufbt launch
```

It installs to `Apps → Games → NW Crawl`. To build without a device, run `ufbt` and copy `dist/nw_crawl.fap` to `apps/Games` on the SD card.

## License

[MIT](LICENSE)
