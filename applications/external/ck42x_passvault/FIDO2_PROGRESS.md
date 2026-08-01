# CK42X PassVault FIDO2 progress

This is a clean-room, MIT-compatible experimental implementation. It is not a
FIDO-certified or hardened security key.

## Current increment

- CTAPHID framing plus INIT, PING, CBOR, CANCEL, and ERROR dispatch.
- Minimal CTAP2.0 getInfo, makeCredential, and allowList getAssertion core.
- Non-resident ES256 credentials, self `packed` attestation, DER ECDSA,
  SHA-256, versioned records, deterministic test RNG seams, and zeroization.
- BSD-2-Clause micro-ecc pinned to commit
  `541b3a78026420a3e369c4c9281c396b5e531113`, configured for portable 32-bit
  secp256r1 with upstream fast modular reduction enabled.
- Strict host tests. Test sources are excluded from the FAP manifest.
- A bounded 1024-byte CTAPHID USB worker using the exported Flipper U2F HID
  interface. The USB callback only wakes the worker; CTAP and cryptography run
  in thread context, and the prior USB configuration is restored on stop.
- Flipper RNG, interruptible local presence approval/denial with 100 ms polling,
  CTAPHID `UPNEEDED` keepalives and matching-channel cancellation, and a separate
  AES-GCM encrypted `fido2.pv1` store holding up to 20 versioned credentials.
- Store updates are synced to `fido2.pv1.tmp`, the prior valid target is moved
  to `fido2.pv1.bak`, and the temp is renamed into place. The SDK rename call
  overwrites a destination but does not document power-loss atomicity, so this
  sequence favors recoverability: a failed install restores the backup, a
  successful install removes it, and unreadable/authentication-failed targets
  are never treated as empty or overwritten. On startup, a missing target with
  a remaining backup restores that backup; stale temp files are then removed.
- The Security Key screen now starts/stops the runtime and reports
  ready/waiting state.

## Physical acceptance evidence

- Oaspote enumerated as `0483:5741` in U2F HID mode and reported FIDO 2.0 GetInfo.
- Native CTAP2 MakeCredential and GetAssertion completed after explicit physical
  approval. The assertion credential matched, its ES256 signature verified, and
  its counter advanced.
- Chromium completed WebAuthn registration and authentication. Browser output
  reported matching credential IDs and valid assertion signatures.
- A browser credential registered before a FIDO-mode restart authenticated after
  re-entry, proving encrypted credential persistence for the tested workflow.
- Exiting FIDO2 restored Oaspote's `0483:5740` serial endpoint and removed the
  FIDO HID endpoint.
- Enabling micro-ecc fast modular reduction reduced measured native registration
  time from 34.720 seconds to 3.003 seconds, below Chromium's HID timeout.

Final tested artifact:

- Size: 44,544 bytes
- SHA-256: `251790b4401e455d56cb84268898d7dd295eb9f168e473bec9fc14a9176bd417`

## Remaining limitations

- No secure element and no FIDO certification or conformance-suite claim.
- No client PIN, resident credentials, or user verification.
- Interrupted-write recovery has strict source/host coverage, but deliberate
  physical power-loss testing at every rename boundary remains future work.
- This must not be the user's only authenticator or recovery method.
