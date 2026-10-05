# Security policy

## Supported versions

The latest release, and the `master` branch it was cut from. There are no
maintenance branches: a fix goes into the next release.

## Reporting a vulnerability

Report it privately, through GitHub security advisories:

<https://github.com/filipsedivy/emrtd-flipperzero/security/advisories/new>

Please do not open a public issue for a security problem, and please do not
include material read from a real document - not the MRZ, not a chip dump,
not an export directory. A description of the failure and, where it helps, a
trace of an exchange that never got as far as authentication, is enough to
reproduce nearly anything.

Expect an acknowledgement within a week. If a report turns out to be a real
problem, the fix and the advisory are published together, and the reporter is
credited unless they would rather not be.

## What is in scope

This is a reader for hostile input: a chip decides what it answers with, and
every byte of that answer is parsed on a device with 186 KB of heap. The
interesting failures are therefore:

- **memory safety in the parsers** - `protocol/emrtd_tlv.c` and everything
  built on it, reached by a malformed or deliberately crafted file;
- **the cryptography** - a mistake in Secure Messaging, in the key
  derivation, in the PACE exchange, or in a check that should have been made
  before a value was used;
- **credential handling** - key material that is not wiped, or credentials or
  key material that reach the export, the trace, or a screen other than the
  one the reader says shows them. `Result -> Keys` shows the session keys on
  purpose, to the person holding the document; anywhere else is a finding;
- **anything that writes outside the export directory**, or that turns a
  document number into a path.

## What is not a vulnerability

- **The EF.SOD signature is not verified on the device.** This is a
  documented limitation, not an oversight: it needs RSA and a store of
  trusted country signing certificates, and the firmware's mbed TLS has
  neither. It is stated in the README, in `docs/cryptography.md` and in
  `docs/security.md`. A read proves the data groups match EF.SOD; it does not
  prove the document is genuine.
- **The remembered credentials are stored in plain text** on the SD card.
  That too is documented, in `docs/security.md`, along with why encrypting
  them on this device would not help and how to avoid storing them at all.
- **Reading a document requires physical possession of it.** That is the
  design of BAC and PACE, not a weakness in this application.
- Anything that needs the attacker to already have the document and its data
  page - at that point they can read the chip with a phone.

## What this application cannot do to a document

It sends only SELECT, READ BINARY, GET CHALLENGE, EXTERNAL AUTHENTICATE,
MSE:Set AT and GENERAL AUTHENTICATE. There is no write command anywhere in
the source, and an eMRTD would refuse one. A chip can, however, block itself
after repeated failed authentication, so a key that does not work is worth
checking rather than retrying.
