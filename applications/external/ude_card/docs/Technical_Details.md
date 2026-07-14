# Technical Details

> [!NOTE]
> The reverse engineering of devices which are legally in my possession is permitted according to § 3 Abs. 1 Satz 2 lit. b GeschGehG (German Trade Secrets Act)

> [!IMPORTANT]
> For legal reasons, no keys for reading the card are included in this application.
> The functionality of this application is limited to parsing the contents of the card.
> No modifications are possible or will be made possible in the future.
> **[See legal information (in German)](Legal.md)**

Intercards are MIFARE Classic 1K cards with a 4 Byte UID.
They are divided into 16 sectors (0 to 15) with each sector containing four blocks (0 to 3).

The details below pertain to cards issued by the University of Duisburg-Essen.
I do not have details on other Intercards, but feel free to open an issue or submit a pull request if you have one and are ready to collaborate.

## Card types

I know of four types of cards:

- Student cards
- Employee cards
- Cash cards
- Library cards

While cash cards are fully supported by this application, member number parsing fails (Parsing error 016) for now.
Implementation for proper detection of cash cards will be implemented in the future.

Library cards have not been tested yet.

## Identification

The app utilizes Block 1 and 2 of Sector 0 to determine the authenticity of the card, as they (or so I assume) contain the necessary identification data for the University of Duisburg-Essen:

```c
    {0x55, 0x06, 0x38, 0x2a, 0x38, 0x2a, 0x38, 0x2a, 0x38, 0x2a, 0x38, 0x2a, 0x38, 0x2a, 0x38, 0x2a},
    {0x38, 0x2a, 0x38, 0x2a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
```

If these bytes are present, this card is recognized as a UDECard.

## KS-Nr. / UID

The KS-Nr. is a decimal representation of the UID.
It is the decimal value of the UID bytes if they were written in reverse order (see `uid_to_ksnr()` in `udecard.c` for the implementation).

## Member type

The member type is stored at Sector 1, in the first and third byte of Block 2.
For older cards, the second occurrence was also found to be in the fourth byte of Block 2.
For students it is set to `0x02`, and for employees it is set to `0x03`.
Given that the university also issues library cards which cannot be used for payment, it is possible that there is an additional member type.

## Balance

> [!CAUTION]
> **DO NOT even attempt to change the balance or transaction count manually using other tools!**
> See [below](#can-i-perform-modifications-to-my-card-with-this-application-eg-changing-my-balance) why.

The balance is saved as three bytes (`byte[0]`,`byte[1]`,`byte[2]`), with the bytes located in Sector 3 Block 0 at positions 2,3 and 4 and at Sector 3 Block 1 at positions 7,8,9.
The balance is stored as euro cents and can be calculated by `byte[0] << 8 + byte[1]`.
The third byte `byte[2]` is a check byte, which is simply the XOR of `byte[0]` and `byte[1]`.

## Transaction Count

The transaction count is calculated in the same way as the [Balance](#balance) and can be found in Sector 2 Block 2, at bytes 13,14 and 15 and as well as in Sector 3 Block 0 at bytes 10, 11 and 12.

## Student / Employee number

The student/employee number is saved four times.
It is likely that this is due to the use of different applications, such as the Library.
This clarifies the reason for the different values for KeyA for each sector in which it is saved.

The member number locations are as follows:

- Sector 5 Block 0
- Sector 6 Block 0
- Sector 8 Block 0
- Sector 9 Block 0

Here is an overview regarding the member number's presence for different card types:

| Sector | **Student** | **Employee** | **Cash** | **Library** |
|----------|-------------|--------------|----------|-------------|
| 5 | ✔️ | ✔️ |  ❌ | ❔ |
| 6 |  ✔️ | ✔️ |  ✔️ | ❔ |
| 8 |  ✔️ | ✔️ |  ❌ | ❔ |
| 9 |  ✔️ | ✔️ |  ❌ | ❔ |

(✔️: Present; ❌: set to ASCII zeros ;❔: Not analysed yet.)

The member number has a length of 12 Bytes.
It is saved in reversed order, is '0'-padded and ASCII encoded.

This application examines all four occurrences and assumes that the parsing of this number was successful if all four instances are identical.
If this is not the case, the first occurrence is used and the member number parsing is flagged as faulty.

## Can I perform modifications to my card with this application (e.g. changing my balance)?

Short answer: **No you cannot and it will not be supported.**
This would not only be illegal, it would also be unethical.

If you lack a strong moral compass: Changes to the balance are traceable, since every card has a KS-Nummer (which is a decimal representation of the UID, see `uid_to_ksnr()` in `udecard.c`).
**It is tracked at every checkout.**
**Even corrections to the balance made by the student service in response to a mistake made at the checkout are tracked and stored in a database including a reason why the change had to be made!**
**Consequently, inconsistencies with the balance will ring some alarm bells.**
**I am in no way responsible what you do.**
**You WILL get in trouble.**
The external central tracking of all transactions has to be the primary reason, why the university has not yet discontinued the use of this card type.
