v1.0:
 * Initial public release
 * MIFARE Plus SL3 card detection and identification (UID, SAK, ATQA, ATS,
   manufacturer decoding, BCC validation)
 * Full sector dump using a dictionary attack with both KeyA and KeyB
   recovery per sector
 * Bundled default key dictionary (~20 well-known MFP AES keys), users can
   add their own .dic files in /ext/apps_data/mfp_reader/
 * Live visual scan progress with per-sector activity grid
 * Per-sector dump result view (A-only / B-only / A+B / failed)
 * Hex byte view of all dumped sectors with monospace TextBoxFontHex
 * Save dumps as Version 2 .mfp files with editable file names and a
   duplicate-name validator
 * Load saved dumps from the Saved menu
 * Delete saved dumps from inside the app
 * MIFARE Plus SL3 card emulation (AuthFirst, AuthNonFirst, ReadEncrypted,
   WriteEncrypted) with live counters and a sector activity grid view
 * Two emulation modes per dump: Writable (changes persisted to file) or
   Read-only (changes discarded)
 * Scan optimization: tracks the last successful KeyA and KeyB and tries
   them first on each new sector — typical cards reuse the same key set,
   so most sectors auth in a single attempt instead of a full dictionary
   pass
