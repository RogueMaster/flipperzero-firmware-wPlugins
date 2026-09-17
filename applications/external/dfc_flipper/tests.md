# Manual hardware test matrix

Protocol framing, crypto, and credential-file parsing are covered by the
automated unit test suite (see the core test command in README). The scenarios
below require a physical DESFire-compatible card and Flipper hardware, so
they're tracked manually here instead.

| Auth cipher         | Key length | Comm mode  | Result |
| -------------------- | ---------- | ---------- | ------ |
| Legacy DES (0x0A)    | 8 bytes    | Plain      |        |
| Legacy DES (0x0A)    | 8 bytes    | MAC        |        |
| 2K3DES (0x0A)        | 16 bytes   | MAC        |        |
| ISO 3DES (0x1A)      | 16 bytes   | Enciphered |        |
| 3K3DES (0x1A)        | 24 bytes   | Enciphered |        |
| AES (0xAA)           | 16 bytes   | MAC        |        |
| AES (0xAA)           | 16 bytes   | Enciphered |        |

Also verify:
- Reading a card with the default (all-zero) PICC-level key succeeds when no
  saved credential is loaded.
- Emulating a saved credential is accepted by a real reader configured with
  the same AID/key.
- Emulating a saved credential triggers DESFire authentication and file reads
  instead of fallback application probing.
- `hf 14a info` on Proxmark3 identifies the emulator as MIFARE DESFire EV1.
