# FDX-B Maker
Construct fully ISO-compliant FDX-B animal microchip data right on your Flipper Zero! The data gets stored as plain RFID files on the SD card, which you can then write to T5577 or EM4305 cards/chips as normal.

# Fields
## Country Code
0 to 999. A 3-digit number corresponding to a country (specified by [ISO 3166](https://en.wikipedia.org/wiki/List_of_ISO_3166_country_codes)) that handles the registration data for the animal. Codes 900-998 are reserved for specific manufacturers, and code 999 is reserved for test chips.

Note: You can also enter values from 1000 to 1023, but these do not comply with the standard and may not get interpreted correctly in all places.

## National Identification Code
0 to 274877906943. A 12-digit number assigned by the authority of the country or manufacturer specified in the Country Code field.

## Animal Bit
Yes/No. Whether or not the FDX-B transponder is being used for animal identification.

## Retagging Counter
0 to 7. Initially set to 0, if an animal needs to be assigned a new transponder the counter should be incremented by 1 each time.

## User Information Field
0 to 31. Can be anything, defined by the country or manufacturer specified in the Country Code field.

## Visual Start Digit
0 to 7. If an animal has a number printed on a visible tag, it will probably be shorter than 12 digits. In that case, you can use this field to indicate what or where the first digit of the visible number is within the National Identification Code.

## RUDI Bit
Yes/No. Stands for "Reference to User Data Inside", and indicates whether or not the transponder is an FDX-ADV chip emulating FDX-B. Yours is not.

## Data Block
3 bytes. You can put whatever you want here, or choose to disable it entirely. You can also encode a temperature from 74ºF to 125ºF in this section by setting the Data Block setting to "Temp".

Note: If you set the Data Block setting to "Off" but you still have data stored in the actual data block, you will be asked whether you'd like to clear the data  entirely or just stop advertising that it's present.

# Special Thanks To
- **Sergei Gavrilov** from the Flipper Team for creating FDX-B support to begin with
- **anon3825968** on the DangerousThings forums for somehow figuring out how temperature data is encoded on Bio-Thermo chips
- **My wonderful friends** for getting me interested in this sort of thing