# z5flip

Flipper Zero app for the IronLogic Z-5R key base (v1.0). Build: `ufbt launch` in this folder.
Runs on Unleashed (tested on device) and builds cleanly for official firmware 1.5.1 / dev; a FAP must be built
against the SDK of the firmware it runs on (API minor differs). Wiring, menus and limits: ../README.md.
Host tests: `tests/test_ctl.c` (build line at its top, Linux / WSL gcc).
