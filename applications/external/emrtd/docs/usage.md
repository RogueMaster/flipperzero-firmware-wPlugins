# Using the reader

## What you need

One of two keys, both of them printed on the document you are holding. That is
the whole access control model: a document you cannot read is a document you
cannot open.

**The three values of the machine readable zone**, which every eMRTD carries -
at the bottom of a passport's data page, on the back of an identity card:

| | Format | Example |
| --- | --- | --- |
| Document number | up to 20 characters, letters and digits | `L898902C` |
| Date of birth | day, month, year | `06.08.1969` |
| Date of expiry | day, month, year | `01.07.2030` |

The reader hashes those three into the password that opens the chip, and for a
passport they are the only way in.

**Or the card access number**, six digits, which identity cards print and
passports do not - often next to the photograph. Where a document has one it is
the better choice: it is shorter to type, there is no check digit arithmetic to
get wrong, and it is what opened the Czech identity card in the README's table.
A CAN works with PACE alone, never with BAC, so the three values are still
worth storing as a fall back.

The antenna is in the **back** of the Flipper, so that is the side the document
goes against. A card lies flat on it, face to face. A passport is opened at the
data page, laid flat, with the Flipper face up over the middle of the page; if
nothing answers, close the book and try the back cover instead - the chip is in
one place or the other, and a few centimetres decide it.

Take the document out of any case that has metal or another card in it, and
keep both still while the progress bar moves.

## The screens

The flow follows the scenes listed in `scenes/emrtd_scene_config.h`.

```
Start ─┬─ Read ──── Read ─┬─ ReadSuccess ── Result ─┬─ Holder
       │                  │                         ├─ Document
       │                  └─ ReadError              ├─ Security
       ├─ Document ─┬─ DocNumberInput                ├─ Files ── File detail
       │            ├─ DateInput (birth, expiry)     └─ Photo
       │            ├─ CanInput
       │            └─ ForgetConfirm
       ├─ Options ── DataGroups
       ├─ Saved ──── SavedDetail
       ├─ Donate
       └─ About
```

### Document

Where the credentials are entered. The document number is a text field: it is
upper cased, and the filler character `<` does not have to be typed - a number
shorter than nine characters is padded for you when the key is derived.

The two dates use a field of their own rather than a keyboard. The MRZ stores
a date as `YYMMDD`, which is not the order anyone reads one in, and typing six
digits in the wrong order fails in a way that looks exactly like a chip that
will not open. So the view keeps three separate fields:

- **left** and **right** move between day, month and year,
- **up** and **down** change the value under the cursor,
- a date that does not exist cannot be entered at all.

A date of birth is read as a date in the past and an expiry as one in the
future, which is the only way to tell 1930 from 2030. That is a display aid;
what is stored is still the six digits the MRZ carries.

The CAN field is for documents that print one, which in practice means
identity cards and residence permits. A stored CAN takes the place of the three
MRZ values whenever PACE runs, and the Document screen labels it that way.
Entering one does not discard the MRZ values: BAC has no other key, so the
reader keeps them for the fall back.

**Forget** clears the stored credentials. It asks first, and then deletes the
settings file described below.

### Options

- **Access method** - automatic, PACE only, or BAC only. Automatic reads
  `EF.CardAccess`, runs whichever driver the chip announces, and falls back to
  the other if that one is refused. The fixed settings exist for diagnosis: if
  a document behaves oddly, pinning the method says which half of the problem
  you have.
- **Data groups** - which groups to attempt. The default is everything except
  DG3 and DG4, which are protected by EAC and will not open for any reader
  without a state issued terminal certificate.
- **Export to SD** - whether a read writes anything at all. Default **on**.
  It is the master switch: with it off the result stays on screen and nothing
  reaches storage, the trace included.
- **APDU trace** - write `trace.txt` next to the export: every command and
  every answer, with the Secure Messaging taken off them so that the file is
  readable rather than a wall of ciphertext, and a note against each saying
  which file, which offset and which status word. Default **off**, because it
  is a diagnostic tool and it records the document's own answers in the clear.
  It writes into the export directory, so it needs **Export to SD** on; with
  exporting off the row reads `Needs export` and no trace is written.
  [trace.md](trace.md) describes the file line by line; read
  [security.md](security.md) before sending one to anybody.
- **Remember on SD** - keep the document number, the dates and the CAN between
  runs, in the settings file. Default **on**: the card is the only place they
  survive the app being closed, and typing three values before every read is
  what makes a reader unusable. They are the key to the document, so the way
  back is in two places and both are deliberate - turn this row off to stop
  the writing, and **Document -> Forget stored data** to remove what is
  already on the card. [security.md](security.md) says what that means.
  A change to the settings file format clears the file, so an upgrade may ask
  you to type them once more.

### Read

The read screen shows the stage - selecting the application, reading
`EF.CardAccess`, authenticating, reading a file, verifying, exporting - the
file in flight, a progress bar over the whole read, and the access method as
soon as a driver has succeeded.

It says the method as soon as it is known on purpose. PACE is several seconds
of elliptic curve arithmetic and DG2 is tens of kilobytes over a 106 kbit
link, so a read is slow enough that a screen which stops saying anything
looks like a crash. It also means that a failure halfway through still tells
you how far it got and how the chip was opened.

**Back** stops the read. The poller is stopped from the scene rather than
from inside its own callback, so it ends cleanly; a document may stay powered
for a moment afterwards.

### The result

- **Holder** - the name, nationality, sex and date of birth from DG1, plus
  what DG11 adds when the document carries it.
- **Document** - type, issuing state, number, date of expiry, and whether the
  MRZ check digits are consistent.
- **Security** - how the chip was opened, which cipher and curve were used,
  which security protocols the chip announced, and the result of comparing
  every data group with the hash EF.SOD lists for it.
- **Keys** - the Secure Messaging keys this read derived: the cipher, `KSenc`,
  `KSmac`, and the send sequence counter the session started from. They come
  out of what you typed and this chip's answer to it, so they are your data
  and the reader shows them rather than using them silently. They are held
  only while the app is open and they are written nowhere - not to the report,
  not to the trace. The row appears only when a session was actually opened.
- **Files** - one line per elementary file: whether it was announced, read,
  skipped or failed, its size, and whether its hash matched. The detail screen
  shows the first bytes of the file and, for the groups that are decoded,
  what they contain.
- **Photo** - what DG2 carries. The Flipper cannot decode a JPEG or a JPEG
  2000 image on its screen, so what this screen reports is the format and the
  size, and the image itself is in the export as `face.jpg` or `face.jp2`.

A read that failed still produces a result: `ReadError` names the error, and
the same screens show whatever was read before it happened.

### Saved reads

The reads already on the card. It opens the Flipper's file browser at
`/ext/apps_data/emrtd`, filtered to the text files, so what you pick is the
`report.txt` of an earlier read and what you get is that report on screen.

Deleting an export is not done from here: use the Flipper's own file manager,
or take the SD card out. That is deliberate - a screen that can erase an
identity with one press is a screen that will do it by accident.

### Donate

Where to buy me a coffee: the address, and a QR code that carries it, for a
phone camera to open without anybody typing it. The backlight stays on while
the screen is open, so it does not switch off while the camera is still
focusing.

## Where things land

```
/ext/apps_data/emrtd/
    emrtd.settings                       the remembered credentials and options
    L898902C_20260920_2114/              one directory per read
        report.txt                       what was read, how, and what verified
        trace.txt                        the APDU log, when it was asked for
        EF_COM.bin  EF_SOD.bin  EF_DG1.bin  ...
        mrz.txt                          the decoded machine readable zone
        face.jpg                         the image lifted out of DG2
```

The directory is named after the document number and the time of the read, so
two reads of the same document do not collide.

The `.bin` files are exactly the bytes the chip returned, including the outer
BER-TLV envelope of each file. That is what makes them worth keeping: they can
be verified against EF.SOD again later, by anything that can hash.

If the SD card is missing or full the read still runs; the export is reported
as failed and the result stays on screen.
