# NFC Stock Manager (Flipper Zero)

Inventory app using NFC tags.

## Screenshots

| Main menu | Inventory | Item details |
| :---: | :---: | :---: |
| ![Main menu](assets/menu.png) | ![Inventory](assets/items.png) | ![Item details](assets/details.png) |

## What It Does

- Scans an NFC tag.
- If it exists in the database, opens the item.
- If it does not exist, creates a new item.
- Lets you edit stock, minimum stock, and location.
- Stores data in `.bin` and exports to `.csv`.

## Quick Usage (App)

- `Scan tag`: read/create item by UID.
- `Inventory`: list, edit, or delete items.
- `Settings > Select warehouse`:
  - `Switch existing`: switch to an existing `.bin` database.
  - `New database`: create a new database.
  - `Rename current`: rename the current database.
- `Settings > Export CSV`: export the active database.

## Quick Build

Requirements: `gcc`, `make`, and a local Flipper firmware checkout.

```bash
make test
make prepare
make fap
```

If needed, change `FLIPPER_FIRMWARE_PATH` in `Makefile`.

## SD Card Data

- Folder: `/ext/apps_data/nfc_stock_manager`
- Active DB: `active_db.txt`
- Default DB: `default.bin`

## Useful Note

- Firmware builds with `-Werror`, so warnings also break `make fap`.

## Repo

- Author: Endika
- Repository: [github.com/endika/flipper-nfc-stock](https://github.com/endika/flipper-nfc-stock)
