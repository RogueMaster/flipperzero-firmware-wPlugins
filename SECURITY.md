# Security Policy

## Intended use

Time Clock performs **badge identification only**: it reads a badge's UID to
recognize it for personal time tracking. It does **not** emulate badges, clone
them, or attempt to bypass any access-control or authentication system. Only use
it with badges and systems you are authorized to use.

## Supported versions

| Version | Supported |
|---------|-----------|
| 1.0.x   | Yes        |
| < 1.0   | No        |

## Reporting a vulnerability

**Please do not open a public issue for security vulnerabilities.**

Instead, contact the maintainer privately through GitHub (**@vladpereverzyev**)
- for example via a GitHub Security Advisory on the repository, or a private
message. Please include:

- a description of the issue and its impact,
- steps to reproduce,
- the affected version / firmware, and
- any suggested fix if you have one.

You can expect an initial acknowledgement within a reasonable time frame. Once a
fix is available, we will coordinate disclosure.

## Known security properties & limitations

These are intentional, documented trade-offs for a self-contained device app:

- **PIN storage.** The PIN is never stored in clear text - only a salted
  FNV-1a hash and its salt are saved in `config.txt`. This avoids plaintext
  storage and gates the on-device UI, but it is **not** a cryptographically
  strong defense against an attacker with physical access to the microSD card
  who brute-forces a 4-digit PIN offline.
- **Protected mode.** When a PIN is set, the app blocks the software paths for
  leaving it (Back button, in-app Exit). It **cannot** prevent a hardware
  power-off or a firmware-level force-quit (e.g. the reboot key combo or
  removing power) - no Flipper app can.
- **No hidden bypass.** There is deliberately no secret PIN-recovery backdoor.
  Removing `config.txt` from the SD card resets the settings and the PIN.
- **Local data only.** All data stays on the Flipper's microSD. The app does not
  transmit data to any external service.
