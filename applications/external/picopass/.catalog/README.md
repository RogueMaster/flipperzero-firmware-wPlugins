# Picopass

This application allows you to read, write, create, save, and emulate legacy HID iClass cards and fobs (based on the picopass chipset).  Also supports saving the credential to the Flipper Zero LFRFID data format, changing the keys on the card, performing dictionary attack, and performing the 'online' part of the loclass attack.

# Create (manual credential)

Use _Create_ from the main menu when you want to build a credential without first reading a card.

1. Select _Create_ and choose the Wiegand format, facility code, and card number (numeric keypad).
2. Pick an action: _Emulate_ to start field emulation, _Write_ to program a presented card, or _Save_ to store the file for later.
3. Defaults target iClass Legacy using standard keys and 3DES; CSN is pre-filled with a typical HID value. Values persist if you back out and return.

# Loclass (HID Only)

The loclass attack emulates specific CSN and collects responses from the reader which can be used to calculate the elite or (some) custom key configured for that reader.  This key is then used to read data on the cards used with that reader.

## Online part

1. Run _loclass_ from the picopass main menu
2. Present the flipper to the reader.  Holding flipper directly to reader may not work, vary distance by a few inches.
3. Collect responses until the progress bar is full.

NOTE: If the screen says “Got std key” AND stays on 0/18, then loclass isn't needed.

## Offline part

1. Download the loclass log (_sdcard/apps_data/picopass/.loclass.log_) from your Flipper Zero.
2. Use [loclass.ericbetts.dev](https://loclass.ericbetts.dev/) or a tool of your choice to calculate the key
3. Copy the key to _iclass_elite_dict_user.txt_ and place in _sdcard/apps_data/picopass/assets/_
4. Run _Read_ from the picopass main menu
5. Present card to the back of the Flipper Zero.

## Failure

There are some situations when the offline loclass may not find a key, such as:
 * non-iClass picopass (Circuit Laundry, etc)
 * iClass SE
 * Readers configured with Standard-2 keyset
 * Custom keyed readers using Standard KDF
 * Custom keyed readers using SE KDF

# NR-MAC Attack

Due to the nature of how secure mode picopass works, it is possible to emulate some public fields from a card and capture the reader's response, which can be used to authenticate.  Two of the pieces involved in this are the 'NR' and 'MAC'.  This allows you to get a dump of the card, except for the key, even if you don't know the key.  For picopass in non-HID systems this can allow you to see what the data looks like.  For iClass SE the data (SIO) is encrypted, but a friend with a HID SAM can decrypt it.

*These instructions are intended to be performed all at the same time.  If you use the card with the reader between Card Part 1 and Card Part 2, then Card Part 2 will fail.*

## First: Card Part 1

1. Place card against Flipper Zero
2. Run _Read_ from the picopass main menu
3. Get a "Read Failed" message
4. Select the "Menu" option
5. Select "Save Partial"  (regardless if this card has been saved previously)
6. Name file something you'll remember
7. *Immediately* proceed to Reader Part

## Second: Reader Part

1. Select _Saved_ from the picopass main menu
2. Select the file name you saved in last step of Card Part 1
3. Select _Emulate_
4. Expose Flipper Zero to reader (It may work better a few inches from the reader, as opposed to physically touching)
5. Flipper will buzz and screen will say "NR-MAC Saved!" (may be very brief)
6. *Immediately* proceed to Card Part 2

## Third: Card Part 2

1. Place card against Flipper Zero
2. Run _Read_ from the picopass main menu
3. Card will authenticate and read
4. Suggested to do both "Save" and "Save as Seader"


# Elite Keygen Attack

Background: https://youtu.be/MKSXSKQHz6o?si=DEKkW60x858pUI0a&t=600

The keys used for early Elite systems used the VB6 (yes, as in Visual Basic) RNG to generate the keys.  This attack uses the known VB6 RNG to generate the keys.  This attack is only useful for early Elite systems, as later systems are keyed in some other manor.  Since this can generate an insanely large number of values (and eventually loop), by default it is limited to the first 2000 keys.  Please provide feedback if you would like this increased.  Also, the leaked iCopyX dictionary included 700ish of these, so the first 700 are redundant to the System Elite Dictionary attack run during "Read".  This attack is not useful for iClass SE systems.

# Incrementing CN during emulation

**beta**

While emulating certain formats (such as H10301), you will see options for "-1", "0", "+1".  These options will decrement, set to zero, or increment the Card Number (CN) field of the emulated card.  The FC, where applicable, will not be impacted.
