Unreleased:

- Limit saved card names to the credential model's 32-character capacity.
- Treat loaded card names as literal text instead of printf format strings.
- Reject invalid buffer capacities and handle reader, listener, and secure-session allocation failures.
- Clear credential keys and reader session keys before releasing their memory.
- Update dfc-core to 1.2.1 and tiny_crypto_c to 2.0.0.

v1.1.1:

- Reset the full DESFire activation when the reader field turns off or the
  ISO-DEP session ends. A new activation now starts at PICC level instead of
  retaining the previously selected application.

v1.1:

- Add full EV3 emulation with EV1 and EV2 commands, up to four applications,
  16 files, and 3 KiB of file data while keeping editable `.dfc` credentials.
- Correct ATQA and ISO-DEP framing so the ACR1552 reader detects and exchanges
  commands with emulated credentials.
- Reduce emulation and credential-loading memory use. Transaction snapshots
  and incoming command chains are allocated only when needed. Load the text
  codec only while importing or saving editable `.dfc` files.
- Add host credential compilation and repeatable protocol and memory probes.
- Use the consolidated tiny_crypto_c backend and dfc-core v1.2.0.

v1.0:

- Initial DFC release.
- Read and emulate generic MIFARE DESFire-compatible credentials.
- Support legacy DES, ISO 3DES, and AES authentication.
- Load .dfc and compiled .dfcb credentials.
- Create and save writable blank-card credentials.
- Support standard, backup, value, and record files.
