# SaFlip

Application to communicate with Saflok credentials and readers.

Currently only works on [Momentum Firmware](https://github.com/Next-Flip/Momentum-Firmware), due to missing Date/Time screen support until OFW [PR#4261](https://github.com/flipperdevices/flipperzero-firmware/pull/4261) is merged.

Allows:

* Reading cards
* Emulating cards
* Creating custom cards
* Editing card data
* Writing cards

It can read and write all of the main Basic Access data and variable keys.
It can also read log entries, both from a card, and from the lock in real-time.

Only works with Mifare Classic (MFC) cards and locks, although Mifare Ultralight C (MFULC) support is coming soon.

## Notes

* The Variable Key "Optional Function" field is untested and may not work properly.
* The "Write" option in the menu ignores the card type selected in the Edit or Create menus. It will select the card type and format the data based on the presented card.
