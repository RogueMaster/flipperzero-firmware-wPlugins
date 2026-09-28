# Credential migration

DFC core reads `.dfc` and `.dfcb` versions 4, 5, and 6. It writes version 6.
The version describes the file format, while `Card Generation` describes the
modeled hardware. An EV3 card can be stored in any version whose fields can
represent its state.

## From version 4

Version 4 has no GetVersion overrides. A decoder leaves the optional hardware
and software version fields unset. Its one authentication mode at PICC level
and in each application becomes the preferred command. The supported-command
mask is inferred from the key type and card generation: DES/2TDEA allows D40
and native ISO; 3TDEA allows native ISO; AES allows native AES and, on EV2 or
EV3, EV2 First and NonFirst. ISO 7816 mutual authentication remains an explicit
v6 choice. Check the mask against the physical card before using it as an
authorization policy.
The inferred mask is limited to commands compiled into the reader. To preserve
all card capabilities during migration, perform the conversion with a full
EV3 build and review the resulting mask.

## From version 5

Version 5 adds optional seven-byte hardware and software GetVersion overrides.
They survive conversion unchanged. Its authentication mode becomes the preferred
command, with the same supported-command inference as version 4.

## Writing version 6

Read the older credential with the text or DER decoder, inspect the resulting
command masks and preferred commands, then write it with the corresponding encoder. A new credential
can set `PICC Authentication Commands` and each application's `Authentication
Commands` explicitly, and set an optional `Preferred Authentication Command`
to one of the enabled commands. `ISO` names the native TDEA command; `ISO7816` names the
three standard APDUs. An optional application `SM Disable` byte records a
separate secure-messaging restriction.

Keep the original file until the new file has passed a decode and a card or
emulator check. A v4 or v5 reader cannot decode version 6, even if the modeled
card uses only older features. Unsupported fields are reported instead of
being silently dropped by reduced builds.
