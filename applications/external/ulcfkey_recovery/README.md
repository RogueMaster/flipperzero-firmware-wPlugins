# ULCFKey Recovery 2.3

Standalone, resumable Flipper Zero workflow for the vulnerable GT23SC4489-style
MIFARE Ultralight-C attack implemented by the original ULCFKey project.

## Workflow

1. Collect repeating `AUTH1` nonces from the card.
2. Emulate those candidates to the authorized reader and save its `AUTH2` responses.
3. Replay a matching response to authenticate to the card.
4. Save a complete authenticated 48-page backup before modifying the card.
5. Tear the dynamic lock bytes, set `AUTH0`, collect the four recovery nonces, and
   zero the four key pages.
6. Recover all four 28-bit effective-DES key segments on the Flipper.
7. Save a ready-to-emulate NFC file under `/ext/nfc/`.
8. Optionally restore the physical card. The app verifies its UID, restores and
   verifies the key first, restores `AUTH0` second, and writes lock bytes last.

## Persistence

The current job is stored in the app's SD-card data directory as `session.bin`.
It includes collected NT/NR material, the authenticated page backup, destructive
step masks, recovery nonces, brute-force position, recovered key, and restore
position. Select **Resume** after reopening the app. Select **New** from the main
screen to delete the active checkpoint and start over.

The main screen is a navigable three-item menu. Workflow screens show an overall
`Section n/8` progress bar plus local progress for countable work. Lock-byte
tearing shows its live delay and attempt number as indeterminate because the
successful timing point cannot be known in advance.

During key recovery, press the physical **Left** button shown as **Pause** (or
Back). The current candidate is checkpointed before returning to the menu.

NT collection is checkpointed every five samples, NR after every signed candidate,
each destructive key-page operation before/after its write, and brute force every
`2^20` candidates plus every recovered segment.

## Safety notes

- This attack intentionally modifies the physical card. Use only on an authorized
  test system and keep the card in place during a write/restore operation.
- The recovered key is authentication-equivalent to the original. DES parity bits
  are not recoverable, so its byte representation may differ in those bits.
- On-device search covers up to four `2^28` spaces and can take many hours.
- Version 2.3 uses precomputed DES subkey contributions and incrementally updates
  only the bits that changed between candidates. Its roughly 4 KiB acceleration
  cache is runtime-only; the compact candidate checkpoint remains on SD and the
  cache is rebuilt automatically after Resume.
- Restoration is offered only after both key recovery and emulation-file saving
  succeed. The UID is checked before any restore write.

Build with `./fbt fap_ulcfkey_recovery`.

Install/launch with a connected Flipper using
`./fbt launch APPSRC=applications/external/ulcfkey_recovery`.

On first launch, an existing checkpoint from the previous `ulcfkey_fixed` app ID
is migrated to the new app's data directory. The old checkpoint is removed only
after the new copy is saved successfully.
