# Changelog

## v1.7 production candidate

- Added a new catalog-compliant 10 x 10 pixel, 1-bit application icon combining
  an RFID chip, radio waves, and a GPIO connector.
- Expanded README documentation for every main-menu item, tag action, Clone
  option, Configure setting, data file, compatibility rule, and safety warning.
- Documented adaptive bank-size handling, rewritable-tag limitations,
  model-aware UMI learning, and verified PC3000/PC3400 conversion.
- Added an exact qFlipper screenshot plan, production acceptance checklist, and
  Flipper Application Catalog manifest template.
- Added eight original, unmodified qFlipper screenshots covering the main menu,
  antenna information, reads, history, saved dumps, Clone, bank info, and settings.
- Added an exact five-wire Flipper-to-YRM100X connection diagram, power guidance,
  and three photographs of the tested antenna and protoboard assembly.
- Kept production logging fully compiled out for minimum loader and runtime RAM.

## v1.6 production candidate

- Removed file logging, log rotation, logger buffers and mutexes, console
  diagnostics, and the Configure **Log Level** option to reduce RAM use.
- Kept the main-menu free-memory indicator using the direct system heap value.
- Preserved v1.5 model-aware UMI discovery and PC3000/PC3400 cloning behavior.

## v1.5 diagnostic build

- Connected **Test UMI Auto** to Clone through a persistent eight-model marker
  registry keyed by the first four TID model bytes.
- Test UMI Auto now learns transitions in both directions, including tags that
  already report PC3400/UMI=1, and stores both verified User-word states.
- Clone PC3400/PC3000 now selects a compatible marker by target TID and refuses
  unknown models before writing any data.
- Added a safe unknown-model screen directing the user to Test UMI Auto and
  automatic marker aliases when Clone rewrites the target TID.
- Fixed a Delete/Deleted-screen timer race that could freeze the app after pressing Exit.
- Made saved-tag deletion scrolling thread-safe and removed redraw-time heap allocations.
- Added a PC3000/PC3400 mismatch warning before Clone writes EPC data.
- Added user-confirmed conversion in both directions by changing only the
  required User W0 marker, with final PC verification and rollback on failure.
- Preserve the source PC3000/PC3400 mode after copying User data, preventing a
  normal Clone from changing mode mid-operation before the EPC write.
- Added a worker-side no-write guard in case the target tag changes after the
  initial scan but before confirmation.
- Added a live **Clone attempts: X/Y** retry indicator to the writing screen.
- Made the Clone retry counter large and persistent, and retained the final
  attempt count on the failure screen so fast retries cannot hide it.
- Hardened User-bank fitting: exhausted transient 0x10 responses now narrow
  the search instead of discarding an already proven writable prefix.
- Synchronized diagnostic version 1.5 across FAP metadata, the About
  screen, the log header, and the versioned release filename.
- Added two visible antenna connection attempts at startup with a one-second
  delay between failed attempts.
- Restored persistent file logging for field testing, with selectable **Info**,
  **Warn**, and **Debug** levels (**Info** by default).
- Keeps at most 10 session logs and caps each file at 1 MiB.
- Fixed intermittent normal Clone failures by leaving tag-managed StoredCRC
  untouched and writing EPC memory from PC word 1.
- Added persistent **Clone Attempts** configuration from 1 to 5, defaulting to 5,
  for retryable Clone I/O failures.

## v1.0

- Prepared the first production release under the YRM100X_PRO name.
- Added automatic reader detection and user-confirmed antenna information.
- Added Multi, Full Multi, Forever, Single, and Full Single read modes.
- Added complete tag dumps, saved-dump pagination, editing, and deletion.
- Added selective cloning and dedicated diagnostics for PC 3400 tags.
- Clarified the PC3400 User marker, added an on-demand explanation, and made
  the clone start action visually distinct.
- Moved the Read Forever history counter clear of the **More** button.
- Added memory-bank size and rewritability diagnostics.
- Added warnings for operations that write to or permanently modify tags.
- Added connection-aware blocking of reader-dependent actions.
- Added reader configuration, tag profiles, feedback options, and persistent settings.
- Removed development logging from the production FAP to reduce loader RAM and
  allow the application to run alongside qFlipper Remote Control.
- Added an on-screen free-memory indicator and lazy view allocation to prevent
  RAM exhaustion.
- Hardened UART frame parsing, worker cancellation, retries, and write verification.
