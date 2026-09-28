# DFC credential format, version 6

DFC records a complete DESFire card image in canonical `.dfc` text or `.dfcb`
DER. Writers emit version 6. Readers accept versions 4, 5, and 6 and reject
later versions as unsupported. The format version describes the credential
fields, independently of whether the modeled card is EV1, EV2, or EV3.

## Version history

| Version | Added fields | Authentication representation |
| --- | --- | --- |
| 4 | Base card, application, file, key, and generation-specific state below | One `PICC Authentication Mode` and one mode per application: `D40`, `ISO`, or `AES` |
| 5 | Optional seven-octet `Card Hardware Version` and `Card Software Version` GetVersion overrides | Same scalar modes as v4 |
| 6 | Authentication command masks, optional preferred command, optional application secure-messaging disable byte | Supported and preferred commands at PICC and application level |

Version 6 replaces `PICC Authentication Mode` and `Application NN Authentication
Mode` with `PICC Authentication Commands` and `Application NN Authentication
Commands`. The canonical text value is `None` or a comma-and-space separated
subset in this order: `D40`, `ISO`, `AES`, `EV2First`, `EV2NonFirst`,
`ISO7816`. `ISO` is native command `0x1A`; `ISO7816` is the standard
GetChallenge/ExternalAuthenticate/InternalAuthenticate sequence. AES native
authentication is command `0xAA`. The six mask bits are respectively 0x01,
0x02, 0x04, 0x08, 0x10, and 0x20. A mask preserves the difference between
commands that share a key type. It does not imply that every possible command
is enabled.

The optional `PICC Preferred Authentication Command` and `Application NN
Preferred Authentication Command` hold one of the six command names. The
preferred command must also be enabled in the corresponding command mask.
When absent, the reader chooses among the enabled commands. In v4 and v5,
`Authentication Mode` supplies the preferred native command; the supported
set is inferred when upgrading to v6.

The optional `Application NN SM Disable` is one hexadecimal octet. Its defined
bits are 0x01 (D40), 0x02 (EV1), and 0x04 (EV2 chained write). An absent field
means no application-level disable setting. `PICC SM Disable` retains its
existing meaning. A recognized field that a reduced build omits is reported
as unsupported.

## Model

A card records its generation, storage class, UID provenance, UID, activation bytes, card configuration, keys, applications, and optional static IC signature.

An application records its AID, optional ISO identifiers, key settings, allowed authentication commands, key sets, files, and optional delegated-application material.

A key set records its number, active state, roll state, key type, and ordered key slots. A key slot records its bytes and version.

A file records its number, type, communication mode, access rights, optional ISO identifier, and type-specific state. Supported types are standard data, backup data, value, linear record, cyclic record, and transaction MAC.

Generation-specific settings are explicit tagged values. EV2 state includes secure-channel capabilities, key-set state, transaction-MAC state, reader identifiers, virtual-card state, and a copyable static IC signature. EV3 state adds secure dynamic messaging, transaction timers, file counters, and proximity-check configuration.

Unknown enum values, tags, commands, or generation-specific fields are malformed. A generation cannot contain a feature introduced by a later generation.

Only committed card state is stored. Selection, authentication, command chains, active timers, proximity transcripts, and uncommitted transactions are session state and are not stored.

## Text fields

The text form uses the field order shown by the canonical examples. Optional feature groups are present only when configured. All byte strings use uppercase hexadecimal octets separated by one space. Integer fields use canonical decimal notation.

An EV2 or EV3 PICC can add these groups:

* `PICC EV2 Card Capabilities` contains six bytes.
* The proximity group contains a 16-byte key, a one-byte option, a published response time, and an optional one-byte bitrate. Proximity requires EV3.
* The virtual-card group contains an installation identifier, information byte, two capability bytes, a 4-byte, 7-byte, or 10-byte UID, two 16-byte keys, and two required flags.
* The DAM group contains three 16-byte keys.

An EV2 or EV3 application can replace its ordinary key list with key sets. The key-set header records the key count, maximum key size, settings, and set count. Every set records its number, version, type, initialized flag, and ordered key slots. Set zero is active and must be initialized. The application can also record ten capability bytes and delegated slot, version, quota, and remaining-block values.

A transaction-MAC file records its committed transaction counter, current eight-byte MAC, key type, key version, 16-byte key, and previous 16-byte reader identifier. A standard data file on EV3 can add an SDM group. That group records options, access rights, enabled offsets and lengths, the counter limit, and the committed read counter.

A card can contain a 56-byte static signature. The UID and virtual-card UID lengths are 4, 7, or 10 bytes.

## Binary fields

The binary form uses these context-specific tags:

* Card `84`: static signature. Version 5 adds primitive `85` and `86` for the
  seven-octet hardware and software GetVersion overrides. Version 4 must omit
  both tags.
* PICC `8B`: EV2 capabilities, `AC`: proximity, `AD`: virtual card, `AE`: DAM keys.
  Version 6 adds primitive `8F` for the preferred authentication command.
* Application `A8`: key sets, `89`: capability data, `AA`: delegated state.
  Version 6 adds primitive `8B` for application secure-messaging disable.
  Version 6 adds primitive `8C` for the preferred authentication command.
* File `A8`: transaction-MAC contents, `A9`: SDM state.

PICC primitive `82` and application primitive `85` hold an INTEGER in all
versions. In v4 and v5, its value is the scalar authentication enum: 0 D40,
1 ISO native, 2 AES. In v6, the same tag holds the six-bit unsigned command
mask. The older scalar records the preferred native command; supported commands
are inferred from the key type and card generation. Only the v6 mask restricts
the enabled commands. Decoders interpret the tag using the credential version.
Unknown bits and combinations incompatible with the application's key type
are malformed. An optional v6 field appearing in an older version is malformed.
The preferred-command INTEGER is a single set bit from the command mask.
A build that omits an enabled command reports the credential as unsupported;
it does not remove the command from a v6 mask. A preferred command outside its
mask, or more than one preferred command, is malformed.

For a 2TDEA PICC, `ISO` and `ISO7816` may be supported while `D40` is disabled.
For an AES application, `AES`, `EV2First`, `EV2NonFirst`, and `ISO7816` may be
supported together. Native `ISO` uses a TDEA key and is invalid in that AES
mask. The physical EV3 test card returned `91 AE` to native `ISO` on an AES
application; its successful ISO 7816 AES exchange is captured separately in
the conformance corpus.

Proximity uses `80` key, `81` option, `82` published time, and optional `83` bitrate. Virtual-card state uses `80` through `87` in the text-field order. DAM state uses `80` authentication, `81` MAC, and `82` encryption keys.

Key sets use `80` key count, `81` maximum key size, `82` settings, and `A3` ordered sets. A set uses `80` number, `81` version, `82` key type, `83` initialized, and `A4` ordered keys. Applications with key sets omit the ordinary `A6` key list. Applications without key sets omit `A8`.

Delegated state uses `80` slot, `81` version, `82` quota, and `83` free blocks. Transaction-MAC contents use `80` counter, `81` current MAC, `82` key type, `83` key version, `84` key, and `85` previous reader identifier.

SDM uses `80` options, `81` access rights, optional `82` through `89` values in the text-field order, and required `8A` read counter. Integers are non-negative canonical DER integers. Constructed fields use canonical DER lengths and field order.

EV2 fields, including the static signature, require EV2 or EV3. Proximity and SDM require EV3. A reduced implementation must reject a recognized disabled feature as unsupported. It must not discard the field.

## Wire vectors

Vector schema 2 identifies the wire dialect on each exchange:

* `native` records a native command and its native response code.
* `native-apdu` records a native command wrapped in a proprietary ISO APDU.
* `iso-apdu` records a standard ISO command APDU.

Request and response octets are evidence. A runner must not translate one dialect into another before comparison.

Static IC signatures are in scope because an emulator can replay them with the recorded UID. A capture of a factory originality challenge without the factory secret is evidence of card behavior, not a replayable successful authentication fixture.

## Security state

EV2 and EV3 secure sessions record the authentication phase, transaction identifier, command counter, session encryption key, session MAC key, and negotiated capabilities. Session fields are test fixtures only. A production credential may contain its configured long-term keys, but must not persist live session keys.

Every state-changing hardware capture has an append-only operation log. Each entry records the intended state, command, response, and observed state before the next mutation. Vectors identify whether their responses came from a physical card or were generated synthetically.
