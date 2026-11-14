# BLE Clipboard Protocol 4

Independent typed-macro transport. Current source is 0.5.4; old protocols are not
accepted. Bluetooth controls an authenticated Flipper app; USB emits keyboard/mouse
reports to the target. This is not BLE HID or a target clipboard service.

## BLE Service

| Purpose | UUID |
| --- | --- |
| Service | `A6F1D9C0-7B21-4C9A-8E52-3D7710AA0301` |
| TX notifications | `A6F1D9C0-7B21-4C9A-8E52-3D7710AA0302` |
| RX authenticated writes | `A6F1D9C0-7B21-4C9A-8E52-3D7710AA0303` |

The custom profile retains bonded numeric-comparison pairing. The hello string
checks compatibility; it is not a password. CRC is not authentication/encryption.

Frames are `[version=4:u8, command:u8, payloadLength:u16LE, payload]`.
Requests contain at most 520 payload bytes; responses at most 124. GATT writes
are at most 128 bytes, with response. The frame parser accepts fragmented writes
and combined frames. The client waits for application acknowledgements as well
as GATT callbacks; it never blindly retries a job.

## Commands

All integers below are unsigned little-endian. Each job uses a nonzero random
32-bit ID; stale responses cannot execute or complete a different job.

| ID | Command | Payload |
| --- | --- | --- |
| 1 | HELLO | ASCII `BLECLIP/4` |
| 2 | BEGIN | job:u32, layout:u8, target:u8, size:u32, CRC32:u32, flags:u8 |
| 3 | CHUNK | job:u32, exact next offset:u32, 1-512 program bytes |
| 4 | COMMIT | job:u32 |
| 5 | EXECUTE | job:u32 |
| 6 | CANCEL | job:u32 |
| 7 | SAVE | job:u32, UUID:16 bytes, nameLength:u8, UTF-8 name:1-79 bytes |
| 8 | INFO | empty; optional read-only query after HELLO |
| 9 | GET_KEYBOARD | request:u16 (nonzero); optional read-only query and subscription |
| 10 | SET_KEYBOARD | request:u16, schema:u8=1, layout:u8, target:u8, overrideSaved:u8=0/1, expectedRevision:u32 |

| ID | Response | Payload |
| --- | --- | --- |
| 0x81 | READY | capacity:u32, chunkSize:u16, layoutCount:u8, targetMask:u8, capabilities:u8, UTF-8 device name |
| 0x82 | BEGUN | job:u32 |
| 0x83 | CHUNKED | job:u32, acknowledged next offset:u32 |
| 0x84 | VERIFIED | job:u32 |
| 0x85 | STARTED | job:u32 |
| 0x86 | WAIT_USB | job:u32 |
| 0x87 | PROGRESS | job:u32, program cursor:u32, total:u32, elapsedMs:u32, mouseRemainingMs:u32 |
| 0x88 | DONE | job:u32, result:u8 (0 complete, 1 cancelled) |
| 0x89 | SAVED | job:u32; save complete, no execution |
| 0x8A | ACTIVE | job:u32, total:u32, elapsedMs:u32, mouseRemainingMs:u32, flags:u8 |
| 0x8B | INFO_REPLY | schema:u8=1, appMajor:u16, appMinor:u16, appPatch:u16, wire:u8, buildApiMajor:u16, buildApiMinor:u16, target:u8 |
| 0x8C | KEYBOARD_REPLY | schema:u8=1, request:u16, result:u8, layout:u8, target:u8, overrideSaved:u8=0/1, revision:u32 |
| 0xFF | ERROR | job:u32 (0 for link-wide errors), code:u8, UTF-8 diagnostic |

Targets are 0 macOS, 1 Windows, 2 Linux; targetMask is 7. Stable layout IDs 0-27
are defined in `KeyboardLayout.swift` and `keyboard_layout.h`; 0=US, 1=Turkish Q.
Other layouts load fixed allowlisted SD filenames, never a remote path. macOS
Turkish Q uses the built-in QWERTY-PC map.

BEGIN flags: 0 normal run, 1 explicitly standalone run, 2 save-only. Other values
are rejected. Save-only jobs cannot EXECUTE; normal jobs cannot SAVE. Capabilities
is 255 (legacy mouse=1, saved programs=2, standalone=4, Move Mouse=8, Repeat=16,
optional companion info=32, optional keyboard sync=64, optional typing pace=128).
The client requires only the first five bits. Non-Fast pacing requires bit 128
before BEGIN, including save-only uploads; never strip pacing silently. If bit
32 is present, it queues one empty INFO query after READY. An older server with
31 is not queried and its app version stays Unknown. INFO_REPLY is sent only in
response to a query, so older clients never see an unsolicited unknown response.
The 13-byte response describes the application build, not a detected firmware
version. Invalid/missing optional metadata leaves the version Unknown without
resetting a transfer or initiating output. INFO never changes job state, storage,
timers or HID reports. HELLO during an autonomous run emits
READY then ACTIVE, attaching remote Stop to the existing job without restarting it.

## Keyboard Settings

Bit 64 opts the client into GET_KEYBOARD after READY. Older servers with 31/63
are not queried, and servers never send KEYBOARD_REPLY to an older client that
has not requested keyboard settings. Disconnect clears the subscription, not
the keyboard preference. GET works during a run without changing its state.

GET and SET use nonzero request IDs; responses echo them. Physical changes notify
subscribed clients with request=0. Result codes are 0 success, 1 stale revision,
2 busy, 3 invalid request, 4 unavailable layout and 5 SD save failure. Every result
includes the actual current setting. Settings errors do not use the job ERROR
path, drop a job, release keys or alter a timer. SET requires idle state and the
current revision; it never retries a stale change. Successful changes increment
the revision only after durable storage succeeds. Identical values are a no-op;
revision wrap is rejected. Settings changes never load a map into an active engine.

The Mac allows one settings request at a time and blocks new transfers until its
reply or a separate ten-second settings timeout. Malformed, uncorrelated and
older-revision replies cannot change preferences or reset a job. A timeout does
not reconnect or resend; Refresh reads the actual device state. On reconnect the
device preference wins, including changes made while the Mac was disconnected.
Offline Mac defaults persist locally but are not blindly pushed on reconnect.

The SD preference is a 16-byte `BCK1` record: layout=4, target=5, overrideSaved=6,
reserved zero=7, revision:u32=8, CRC32 of bytes 0..11=12. Files are `.keyboard-v1`,
`.keyboard-v1.tmp` and `.keyboard-v1.bak` in app data. Save writes/syncs/verifies
staging before atomic replacement. Startup keeps a valid committed record or
recovers its valid backup; it never promotes staging. Missing/corrupt preferences
default to original saved-job layouts, not a forced English override.

Choosing a device keyboard enables its optional local-run override; the menu's
Saved runs: Original choice disables it. Effective target/layout are shown before
physical Run. A changed revision invalidates an open Run confirmation. The
snapshot is loaded and verified using its original metadata before applying an
in-memory override and performing whole-program preflight. Unsupported text or a
missing map rejects the run, never silently falls back to US. Explicit key records
retain their compiled modifiers; changing OS cannot recompile saved shortcuts.
Mac-triggered runs and existing Mac definitions retain their explicitly reviewed
layout/target; Quick Paste and newly created macros use the synced defaults.

## Typed Program

Program header: ASCII `BCM4`, operationCount:u16LE. Between 1 and 128 operations
are allowed; the entire encoded program is at most 65,536 bytes.

| Opcode | Record |
| --- | --- |
| 1 | byteLength:u32, literal UTF-8 text |
| 2 | modifiers:u8, keySymbol:u16 |
| 3 | milliseconds:u32 |
| 4 | durationMs:u32, intervalMs:u32; alternating relative mouse moves, no buttons |
| 5 | xMin:i8, xMax:i8, yMin:i8, yMax:i8; relative Move Mouse |
| 6 | durationMs:u32, intervalMinMs:u32, intervalMaxMs:u32, bodyCount:u16, bodyBytes:u32, body records |
| 7 | extraPauseMs:u16, variationMs:u16; first top-level record only |

Opcode 7 is optional and never legal in Repeat or later in a sequence. Pause is
0-1000 ms; variation cannot exceed pause and pause + variation cannot exceed 1000.
Fast omits the record. Slow uses 80/0, Variable 80/30, Custom uses validated bounds.
The extra pause is sampled after a literal character's final key release, in
addition to normal hold/release timing; it does not retime explicit chords or
mouse movement. The nonblocking deadline remains interruptible by Stop, USB/BLE
loss and Repeat expiry. The record is part of saved-job bytes and CRC.

Text must be nonempty printable ASCII/Tab/LF/CR, plus the 12 native Turkish
letters only when layout=1. Both desktops normalize NFC and CRLF/CR to LF. Malformed,
overlong or unsupported UTF-8 is rejected. Brackets are ordinary text bytes.
Literal text mappings may not contain Control/GUI modifiers.

Key modifiers are Control=1, Shift=2, Alt=4, GUI/Command=8. Key symbols are lowercase
ASCII a-z / 0-9, or ENTER=0x100, TAB=0x101, ESC=0x102, BACKSPACE=0x103, DEL=0x104,
UP=0x105, DOWN=0x106, LEFT=0x107, RIGHT=0x108, HOME=0x109, END=0x10A,
PGUP=0x10B, PGDN=0x10C, SPACE=0x10D, F1-F12=0x110-0x11B. Unknown symbols or
modifier bits are rejected. Only explicit key records can request shortcuts.

Each delay is 1-30,000 ms; total explicit delay is at most 120,000 ms. A program
must contain text, a key or mouse movement, not just delays. Mouse duration is
1-86,400,000 ms, at most 24 hours combined, interval 1,000-3,600,000 ms. There must
be no trailing bytes.

Opcode 4 remains for old saved jobs, not as a selectable action. Move Mouse ranges
are ordered within -127...127; zero/zero is excluded. Repeat has one body level,
1,000-3,600,000 ms intervals, 128 records per sequence and 512 total nodes. Bodies
must have output and cannot contain Repeat or opcode 4. Timed operations share
the 24-hour budget. Whole-program preflight includes all body bounds and mappings.

Range selection uses the SDK random callback and bounded multiply-high mapping,
without a retry loop that could stall Stop. Equal bounds need no entropy. Failure
stops the job. Cycles never overlap; their intervals are measured from cycle starts.
The deadline is checked before pending input, releases keys without a dead-key
finisher, and advances past the block. Reported cursor progress is a high-water
mark. None of this is an anti-detection or human-activity guarantee.

## Execution and Recovery

BEGIN loads the selected layout and creates RAM or temporary SD storage. CHUNK
must match the exact received offset. COMMIT requires the complete length,
IEEE CRC-32 (initial/final XOR 0xFFFFFFFF, polynomial 0xEDB88320), and whole-program
validation, including every text character and key. No HID reports are sent yet.
Only a subsequent EXECUTE for that verified job can start output.

Small programs use 4 KiB RAM; larger ones use the app's `.macro-v4.tmp` SD file
with bounded read caching. There is no history/replay. Temporary contents are
cleared/removed on completion, Stop, exit and next startup. Explicit standalone
runs retain their active storage while disconnected.

Execution uses a nonblocking state machine, an initial 900 ms focus allowance,
12 ms key holds and at least 8 ms release gaps. Delays never block command
processing. Keys are released after each event and on failure/cancellation.
The Mac Turkish grave accent is finished with Space during normal execution;
Stop releases immediately and can leave a partially entered dead key.

Receive inactivity expires at 15 seconds; verified and USB-wait jobs at 30
seconds. USB loss always stops execution. BLE/status loss discards normal jobs;
only an explicitly standalone job already running survives it. Partial uploads,
verified but unstarted jobs and USB-wait jobs never survive disconnection.
Stop keeps an otherwise healthy connection ready for another explicit run.
Old job IDs, reconnect, startup and duplicate EXECUTE after completion never replay
input. Progress is bytes processed, not application-level delivery confirmation.

## Saved Programs

SAVE only follows COMMIT/VERIFIED, uses a UUID rather than a remote path, and never
emits input. The 20-slot library uses fixed UUID filenames under the app's `saved`
directory. A file has a 128-byte `BCS4` header followed by the compiled program.
Offsets: layout=4, target=5, UTF-8 name byte length=6, reserved=7, program size=8,
program CRC=12, UUID=16, zero-padded name=32..111, reserved zeros=112..123,
CRC of bytes 0..123=124. Integers are little-endian. Metadata and body are separately
checksummed. Empty IDs, control characters, invalid UTF-8, oversized names and
noncanonical padding are rejected; the filename must match the header UUID.

Replacement writes/syncs/verifies `.tmp`, renames old `.bcm` to `.bak`, promotes
the new file and verifies it before deleting the backup. Recovery keeps a valid
final or restores a valid backup; it never auto-promotes staging. Power loss may
leave staging/recovery plaintext on SD. Deletion is not secure erasure.

Physical Run requires USB already connected, copies the selected file into bounded
job storage, verifies CRC and all operations/layout, then gives three seconds to
focus the target. Local runs are autonomous. Left/Back stops; idle Back exits and
restores USB. Delete has its own confirmation. Saved files are not a run queue;
no execution state is persisted or resumed after restart.
