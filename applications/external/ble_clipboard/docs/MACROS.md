# Macros in 0.5.4

Quick Paste and the macro library are available in both the Mac companion and
the full Windows x64 unsigned beta companion. They send protocol-4 jobs to the
Flipper app. The USB target needs no companion. The Mac standard build and
sandboxed Store development target share their UI; Store distribution is separate.

## Definitions

JSON schemas 1/2 stay readable; generic mouse/repeat documents use schema 3.
Each macro has a name, target OS, keyboard layout,
ordered steps and optional named parameters. Existing definitions/exported files
remain readable. The Flipper receives compiled snapshots, not template
definitions or the entire desktop library.

- Literal text is never interpolated or parsed as keys.
- Templates support `{{clipboard}}`, ISO-8601 `{{timestamp}}` and named parameters.
  Replacement values are not recursively evaluated.
- Key combinations support Control, Alt, Shift, GUI and `PRIMARY`; PRIMARY means
  Command on macOS, Control on Windows/Linux. Keys include letters, digits,
  navigation, Enter/Tab/Escape, deletion and F1-F12.
- Delays are 1-30,000 ms each and at most 120 seconds combined.
- Move Mouse has fixed/ranged X/Y relative HID units within -127...127, excluding
  zero/zero. A HID unit is not necessarily a screen pixel.
- Repeat runs a sequence for a duration, with fixed/ranged intervals between
  cycle starts. Cycles never overlap; nested repeats and delay-only bodies are
  rejected. Timed duration is limited to 24 hours total.
- Mouse Jiggle is a preset: eight hours, interval 45...95 seconds, and one Move
  Mouse with X/Y ranges -2...2. Samples are drawn on Flipper at execution time.
  It is not guaranteed invisible, undetectable, human-like, or to satisfy an idle policy.

Definitions allow 128 steps per sequence, 512 total including bodies, and 32 parameters, with nonempty names and
unique identifiers. Imports are at most 2 MB / 1,000 definitions. Invalid schemas,
duplicates, malformed templates and unknown parameters are rejected. Import
copies definitions under new IDs and never runs them.

## Storage and Review

The direct build stores `Application Support/BLE Clipboard/Macros/library.json`;
the Store/preview build uses that path inside its sandbox. Saves are atomic with
one validated backup; corrupt files are preserved until explicit recovery.
Windows uses `%APPDATA%\BLE Clipboard\macros.json` with a `.backup` alongside it.
Its installer upgrades/uninstall preserve the library and preferences.
Use export/import to move between containers. Exports are unencrypted JSON.

Clipboard contents are read only by Load Clipboard, or on Mac when reviewing a
template that uses the clipboard. Windows template review has its own explicit
Load Clipboard action. Parameters and timestamp are captured for the reviewed run.
Literal secrets deliberately saved in macros persist in the library/backup.
No automatic replay queue is maintained on the Flipper. Send to Flipper saves a
plaintext snapshot on SD. Clipboard, timestamp and parameters are fixed at send
time. Up to 20 entries survive restart; sending the same macro ID replaces its
snapshot, while Quick Paste reviews create new snapshots.

Review shows actual expanded text/keys/delays. The target OS and layout are part
of the immutable compiled job. Confirm the target and explicitly Run on USB.
There is no global hotkey, automatic clipboard watching or implicit execution
after reconnect. Hide-before-output does not prove that another app has focus.
Relevant parameter, connection, keyboard, destination or timing edits clear the
confirmation. A fresh confirmation never silently replays a prior transfer.

## Typing Pace

Both desktops remember Fast, Slow, Variable or Custom locally. Fast keeps the
existing 12 ms hold / at least 8 ms release gap. Slow adds 80 ms after each
literal character; Variable adds 80 +/- 30 ms. Custom pause/variation must stay
within 0-1000 ms. Explicit key combinations and mouse movement are not retimed.
The selected pace is encoded in the reviewed program and in saved SD snapshots,
not written into the shared JSON macro definition. Non-Fast jobs require the
Flipper typing-pace capability before upload; older firmware is never silently
given a different pace. A paced text-only program has five fewer payload bytes.

## Text and Layouts

The 28-layout catalog is shared by Quick Paste and saved macros. US is built in.
Most other layouts require the matching official `.kl` file. macOS/Turkish Q uses
the built-in Apple Turkish-QWERTY-PC map. Native Turkish letters are experimental;
other arbitrary Unicode/IME input is not enabled. See the root README for the list.

Text normalizes to NFC and LF. `[ENTER]`, `[GUI+r]`, `[]` or template-like text
inside replacement values remain literal characters, without escape syntax.
Key combinations exist only as typed key records. A 65,536-byte encoded program
includes metadata; a single text-only job can hold up to 65,525 ASCII bytes.

## Transfer and Stop

Both desktops use the bounded upload/verify/execute protocol described in
[PROTOCOL.md](PROTOCOL.md). The Flipper verifies CRC and every operation/layout
mapping before execution. Large jobs use temporary SD storage, not a history.

Stop sends a job-scoped cancellation command and keeps Bluetooth connected.
During an in-flight upload it waits only for the current bounded frame's reply;
it never sends the next chunk or EXECUTE after cancellation. During typing it
releases keys and discards the job. Left/Back stops a busy job; Back from the idle
library exits. Normal runs stop on BLE loss and Mac sleep/quit. Explicit standalone
runs continue offline until complete, physical Stop, USB loss, Flipper app exit or power
loss. Mouse Jiggle defaults the standalone review option on. Reconnect attaches
remote Stop to the active job, without replay. Flipper shows elapsed/remaining time.

Send to Flipper never executes. Up/Down selects a saved entry, OK opens its detail,
and Run needs a second confirmation with a three-second focus allowance. Right
opens a separate Delete confirmation. Physical runs are standalone; USB unplug
stops permanently, rather than queueing for the next USB connection.

Stop cannot undo input or restore a partly entered dead key. Tabs/newlines can
submit forms or commands. Completion confirms HID reports, not receipt by the
intended application. Focus/shortcut behavior remains unverified for 0.5.4.

Legacy mouse definitions migrate in memory to Repeat with +1 X, Delay 50 ms and
-1 X steps, retaining IDs, duration and layout. Existing saved binary snapshots
are not changed. Edit the ranges or create the new preset, then resend to update
the Flipper snapshot under the same macro ID.
