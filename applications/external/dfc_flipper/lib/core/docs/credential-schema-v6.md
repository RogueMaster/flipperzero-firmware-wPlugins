# DFC credential schema, version 6

This is the complete field layout for the canonical `.dfc` text and `.dfcb`
binary encodings. The [format specification](credential-format-v6.md) defines
version compatibility, authentication rules, and feature constraints. The
[examples](../examples/credentials/README.md) contain matching text and binary
files. A credential stores configured card state, including keys. Treat real
credentials as secrets.

## Common rules

In the tables below, `I` is a minimally encoded, signed, two's-complement DER
INTEGER; `U` is an `I` constrained to non-negative values; `B` is a one-octet
DER BOOLEAN body (`00` or `FF`); `X(n)` is exactly `n` octets; and `X` is a
variable-length octet string. `?` means optional. A constructed field contains
the listed children in order. `*` means zero or more, and `+` means one or
more. Tags `80`–`8F` are context-specific primitive tags; `A0`–`AF` are
context-specific constructed tags. These are implicit tags, so there is no
inner universal INTEGER or OCTET STRING tag. Repeated keys, files, applications,
and key sets use universal `30` SEQUENCE; individual record bytes use `04`
OCTET STRING. The outer credential tag is application `60`. Lengths use
canonical definite DER form. Unknown tags, duplicate fields, changed order,
and trailing bytes are invalid.

Text is UTF-8 limited to ASCII, with one `Name: Value` per line and a final
newline. Hex bytes are uppercase pairs separated by single spaces; booleans
are `0` or `1`; decimal integers have no leading zeroes. `NN` and `FF` in
field names are consecutive, zero-based, uppercase hexadecimal indexes, not
file numbers. The tables show **binary tag order**; text field order differs
where stated below. Absent optional fields are omitted. `Filetype: DFC Credential`
and `Version: 6` start every
v6 text file. Counts precede their indexed members. Binary lists derive their
counts from the number of children.

Canonical text order is: header; card fields; PICC key settings, commands,
optional preference, key count and keys, optional ID/activation settings,
optional EV2 feature groups, and PICC files; application count; then each
application's identity, key settings, commands, optional preference, ordinary
keys or key sets, files, capability data, delegated state, and optional SM
disable. Inside a file, its common fields precede its type-specific contents;
SDM follows data contents. The example files show the exact line spelling and
placement. A reader rejects recognized fields placed outside this order.

## Root and card

| Tag | Binary field | Text field | Type |
| --- | --- | --- | --- |
| `60` | Credential | whole file | constructed |
| `80` | Version | `Version` | `U = 6` |
| `A1` | Card | card fields below | constructed |
| `A2` | PICC | PICC fields below | constructed |
| `A3` | Applications | `Application Count`, then `Application NN` | `30 Application` * |

`Filetype` is text-only. Card fields in `A1`:

| Tag | Text field | Type |
| --- | --- | --- |
| `80` | `Card Generation` | `U`: EV1=1, EV2=2, EV3=3 |
| `81` | `Card Storage` | `U`, byte capacity |
| `82` | `UID` | `X(4/7/10)` |
| `83` | `UID Provenance` | `U`: Real=0, Random=1, Unknown=2 |
| `84`? | `Card Static Signature` | `X(56)` |
| `85`? | `Card Hardware Version` | `X(7)`, added in v5 |
| `86`? | `Card Software Version` | `X(7)`, added in v5 |

## PICC

| Tag | Text field | Type |
| --- | --- | --- |
| `80` | `PICC Key Settings 1` | `X(1)` |
| `81` | `PICC Key Settings 2` | `X(1)` |
| `82` | `PICC Authentication Commands` | `U`, six-bit mask |
| `83`? | `PICC Random ID` | `B`, emitted only when true |
| `84`? | `PICC Format Disabled` | `B`, emitted only when true |
| `85`? | `PICC ATS` | `X` |
| `86`? | `PICC SAK` | `X(1)` |
| `87`? | `PICC ATQA` | `X(2)` |
| `88`? | `PICC SM Disable` | `X(1)` |
| `A9` | `PICC Key Count`, then `PICC Key KK` | `30 Key` * |
| `AA` | `PICC File Count`, then `PICC File FF` | `30 File` * |
| `8B`? | `PICC EV2 Card Capabilities` | `X(6)` |
| `AC`? | `PICC Proximity ...` | Proximity container |
| `AD`? | `PICC Virtual Card ...` | Virtual card container |
| `AE`? | `PICC DAM ...` | DAM container |
| `8F`? | `PICC Preferred Authentication Command` | `U`, one mask bit, added in v6 |

Authentication mask bits are `D40=01`, `ISO=02`, `AES=04`, `EV2First=08`,
`EV2NonFirst=10`, `ISO7816=20`. Text lists enabled names in that order,
separated by comma and space, or `None` for zero. A preferred command is one
enabled bit. `ISO` means native `0x1A`; `ISO7816` means the standard mutual
authentication APDUs. The application uses the same mask rules.

Nested PICC containers:

| Container | Ordered children, tag: text suffix (type) |
| --- | --- |
| Proximity `AC` | `80`: `Key` `X(16)`, `81`: `Option` `X(1)`, `82`: `Published Response Time` `U`, `83`?: `Bitrate` `X(1)` |
| Virtual card `AD` | `80`: `Installation ID` `X`, `81`: `Information` `X(1)`, `82`: `Capabilities` `X(2)`, `83`: `UID` `X(4/7/10)`, `84`: `Select MAC Key` `X(16)`, `85`: `Select Encryption Key` `X(16)`, `86`: `Authentication Mandatory` `B`, `87`: `Proximity Mandatory` `B` |
| DAM `AE` | `80`: `Authentication Key` `X(16)`, `81`: `MAC Key` `X(16)`, `82`: `Encryption Key` `X(16)` |

Prefix every nested text suffix above with `PICC Proximity`, `PICC Virtual
Card`, or `PICC DAM`, respectively. Proximity requires EV3. EV2 capabilities,
virtual card, and DAM require EV2 or EV3.

## Applications and keys

Each application is a `30` SEQUENCE inside `A3`. Its text prefix is
`Application NN`.

| Tag | Text suffix | Type |
| --- | --- | --- |
| `80` | `AID` | `X(3)` |
| `81`? | `ISO File ID` | `X(2)`, network byte order |
| `82`? | `DF Name` | `X`, up to 16 octets |
| `83` | `Key Settings 1` | `X(1)` |
| `84` | `Key Settings 2` | `X(1)` |
| `85` | `Authentication Commands` | `U`, six-bit mask |
| `A6`? | `Key Count`, then `Key KK` | `30 Key` *, absent with key sets |
| `A7` | `File Count`, then `File FF` | `30 File` * |
| `A8`? | `Key Set ...` | Key-set container, replaces `A6` |
| `89`? | `Capability Data` | `X(10)` |
| `AA`? | `Delegated ...` | Delegated container |
| `8B`? | `SM Disable` | `X(1)`, added in v6 |
| `8C`? | `Preferred Authentication Command` | `U`, one mask bit, added in v6 |

Each ordinary key is a `30` SEQUENCE inside `A9` or `A6`. Each key-set key
uses the same layout inside its set's `A4`:

| Tag | Text suffix | Type |
| --- | --- | --- |
| `80` | `Key KK` slot index | `U` |
| `81` | `Key KK` value | `X(16/24)`; DES is stored in 16 octets |
| `82` | `Key KK Version` | `X(1)` |

The key slot index is binary-only; text `KK` is its value. Key length follows
the key type in `Key Settings 2` or the key-set type.

Key-set container `A8` has `80` Key Count (`U`), `81` Maximum Key Size (`U`),
`82` Settings (`X(1)`), and `A3` Sets (`30` Set +). Text uses `Key Set Key
Count`, `Key Set Maximum Key Size`, `Key Set Settings`, and `Key Set Count`.
Each set's text prefix is `Application NN Key Set SS` and its `30` body has
`80` Number (`U`, text index `SS`), `81` Version (`X(1)`), `82` Type (`U`,
text `D40`/`ISO`/`AES`), `83` Initialized (`B`), and `A4` Keys (`30` Key +).
Text also gives each set a `Key Count`. Set zero is initialized and active.

Delegated container `AA` has `80` Slot Number (`U`), `81` Slot Version
(`X(1)`), `82` Quota Limit (`U`), and `83` Free Blocks (`U`). Its text prefix
is `Application NN Delegated`.

## Files

Each file is a `30` SEQUENCE inside its owner's file list. Text prefix is
`PICC File FF` or `Application NN File FF`.

| Tag | Text suffix | Type |
| --- | --- | --- |
| `80` | `Number` | `U`, file number 0–31 |
| `81` | `Type` | `U`: Standard Data=0, Backup Data=1, Value=2, Linear Record=3, Cyclic Record=4, Transaction MAC=5 |
| `82` | `Communication Settings` | `X(1)` |
| `83` | `Access Rights` | `X(2)`, network byte order |
| `84`? | `ISO File ID` | `X(2)`, network byte order |
| `A5`? | data-file fields | required for types 0 and 1 |
| `A6`? | value-file fields | required for type 2 |
| `A7`? | record-file fields | required for types 3 and 4 |
| `A8`? | transaction-MAC fields | required for type 5 |
| `A9`? | `SDM ...` | SDM container, standard data file on EV3 |

Exactly one of `A5`–`A8` appears. Type-specific children, in tag order:

| Container | Ordered children, tag: text suffix (type) |
| --- | --- |
| Data `A5` | `80`: `Size` `U`; `81`?: `Data` `X`; `82`: `Data Complete` `B` |
| Value `A6` | `80`: `Value Lower Limit` `I`; `81`: `Value Upper Limit` `I`; `82`: `Value` `I`; `83`: `Limited Credit` `X(1)` |
| Record `A7` | `80`: `Record Size` `U`; `81`: `Max Records` `U`; `82`: `Record Count` `U`; `83`: `Record Complete` `B`; `A4`: `Record RR` as `04` octet strings * |
| Transaction MAC `A8` | `80`: `Transaction Counter` `U`; `81`: `Transaction MAC` `X(8)`; `82`: `Transaction Key Type` `X(1)`; `83`: `Transaction Key Version` `X(1)`; `84`: `Transaction Key` `X(16)`; `85`: `Previous Reader ID` `X(16)` |
| SDM `A9` | `80`: `SDM Options` `X(1)`; `81`: `SDM Access Rights` `X(2)`; `82`?: `SDM UID Offset` `U`; `83`?: `SDM Counter Offset` `U`; `84`?: `SDM PICC Data Offset` `U`; `85`?: `SDM MAC Input Offset` `U`; `86`?: `SDM MAC Offset` `U`; `87`?: `SDM Encrypted File Offset` `U`; `88`?: `SDM Encrypted File Length` `U`; `89`?: `SDM Counter Limit` `U`; `8A`: `SDM Read Counter` `U` |

The encrypted file offset and length appear together. A complete data file
has exactly `Size` bytes; a complete record file has exactly `Record Count`
records. An incomplete file may hold only a known prefix. Record bytes are
ordered oldest first. For cyclic files, `Max Records` includes the reserved
backup slot. SDM and transaction MAC require their compiled features.

## Version and build checks

Version 4 and 5 use scalar authentication mode values at PICC `82` and
application `85`: D40=0, ISO=1, AES=2. Version 6 uses masks at those same
tags. Version 4 omits card `85` and `86`; version 5 may contain them. Only
version 6 may contain PICC `8F` and application `8B`/`8C`. Readers accept
versions 4–6 and writers emit version 6. A build with a disabled recognized
feature reports unsupported; it does not discard it. The
[migration guide](credential-migration.md) explains the conversion.
