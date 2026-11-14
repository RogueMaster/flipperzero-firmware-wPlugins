# Security

BLE Clipboard 0.5.3 turns explicit macro steps into USB keyboard/mouse events. Use it only on devices you
own or are authorized to operate. Newlines and tabs have normal key behavior.

## Trust Model

- Compare Bluetooth pairing codes on both devices. Do not accept a mismatch.
- Pair only with a computer you trust. The protocol's HELLO is not a password.
- Other processes on a paired computer may access the BLE peripheral.
- The target must trust the attached USB HID device. It does not authenticate this app.
- Mouse Jiggle is not guaranteed invisible or undetectable. USB identity and
  input events remain observable; it does not prove human activity.
- USB HID submission does not guarantee which target application received text.
- The application does not execute a shell on the source Mac, expose arbitrary
  firmware/file access, parse text as script tags or run jobs on reconnect.
- Shortcuts are typed records, not text. The Flipper requires a complete bounded
  upload, CRC, whole-program preflight and a separate execution command. CRC is
  not authentication; authenticated Bluetooth protects transport.
- Saved macro definitions/backups, exports and explicit Flipper snapshots are
  plaintext. Active Flipper jobs use RAM or a temporary SD file, cleared on Stop,
  completion, Flipper app exit/startup. Standalone jobs retain it while offline.
  Explicit saved snapshots and interrupted-save staging/recovery files persist
  on SD; deleting a snapshot does not delete the separate Mac definition.
  This is not a secret vault or secure erasure. Pairing keys remain on the Flipper.
- Stop releases keys but cannot undo input or a half-entered dead key. Verify
  focus/layout and inspect the target before retrying any interrupted job.
- Normal jobs stop on BLE loss. Explicit standalone jobs continue after Mac quit
  or disconnection; physical Left/Back must remain accessible. USB loss always
  stops, and no run resumes automatically on cable reconnection or power-up.
- Source builds default to ad-hoc signing; public Mac release finalization uses
  Developer ID signing and Apple notarization. Inspect the published checksums,
  source and Gatekeeper assessment; do not disable OS security protections.
  Historical 0.1.0 downloads remain ad-hoc signed and unnotarized.

## Reporting

Info & Companions checks the exact public GitHub release only when opened or
refreshed. The request has no credentials, cookies, clipboard, macro, device name
or Bluetooth identifier; GitHub receives normal connection metadata and an app
version User-Agent. Both downloads require matching uploaded Mac/FAP/checksum
assets. Unknown, failed or incomplete checks never enable a fallback download.
Links open the browser; they do not install software or interrupt running jobs.

For non-sensitive bugs, open an issue without passwords, pairing keys, or real
clipboard contents. For vulnerabilities, use the repository's GitHub security
advisory reporting when enabled. Do not post exploitable details or private
clipboard data in a public issue.
