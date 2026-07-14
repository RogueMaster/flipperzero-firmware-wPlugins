# Badge Audit

A defensive access-control auditor. Almost every Flipper NFC tool reads, clones, or emulates a card — Badge Audit does the opposite: it tells you how secure an access credential actually is.

Hold a badge to the back of your Flipper and it will:

- Identify the real card type (MIFARE Classic / Ultralight / DESFire / Plus, ISO14443-3A/4A/B, ISO15693, FeliCa, ST25TB).
- Read and show the UID, SAK and ATQA.
- For MIFARE Classic, test every sector against the common factory/default keys and report how many are still unprotected — the #1 real-world access-control weakness.
- Give a clone-resistance score (0-100) with a WEAK / MEDIUM / STRONG / CRITICAL verdict.

## Controls

- Tap a card to the back to analyze it.
- OK — save a posture scorecard to the SD card.
- Arrows — rescan a new card.
- Back — exit.

## Important

Badge Audit is read-only. It does not clone, emulate, write, or bypass anything. Use it only on credentials and systems you own or are explicitly authorized to assess.
