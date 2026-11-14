# 0.5.4 Desktop Feature Parity

This is a release-gate checklist, not evidence that unrun tests passed. Source
implementation and automated verification are distinct from physical validation.
Windows is an unsigned x64 beta. No live input, VM control, device access or
installed-app replacement is authorized by this checklist.

| Workflow / failure state | Windows implementation | Verification |
| --- | --- | --- |
| Explicit clipboard/text-file load; literal text only in Quick Paste | Workspace | Compiler unit tests; offline UI suite |
| Search, create, duplicate, delete, rename, save macro | Sidebar/editor | Library unit tests; offline UI suite |
| Reorder/remove steps; Repeat children; no nested Repeat | Step tree/dialog | Compiler limits; offline UI suite |
| Parameters, clipboard snapshot, UTC timestamp, nonrecursive templates | Review dialog/compiler | Shared Swift/Python fixtures |
| Target-specific shortcuts, delays, Move Mouse ranges, Repeat intervals | Compiler/editor | Shared bytes executed by C engine |
| Stable identifiers/layout IDs/target IDs, schemas 1-3, legacy migration | Library codec | Shared round trips and legacy fixture |
| Size/node/delay/duration/name/UTF-8 limits | Compiler/library | Invalid-input unit tests; C preflight |
| Atomic save, backup, corrupt-file protection, explicit recovery | MacroLibrary | Temp-directory recovery tests |
| Import clones macro IDs; export single or entire library | Library menu | Codec/import unit tests |
| Device list, authenticated pairing guidance, no automatic pairing downgrade | Transport/workspace | Fake BLE; physical unverified |
| BEGIN/chunk ACK/CRC verification before execute; no replay | Transport | Fake BLE + C protocol tests |
| Save plaintext SD snapshot without running; 20-slot error | Review/save path | Fake BLE, C SD capacity/rollback tests |
| Standalone Run, elapsed/remaining timers, attach without executing, Stop | Transport/review | Fake BLE + C state machine; physical unverified |
| USB loss, BLE loss, cancellation during upload/output/pause, stale IDs | Transport/engine | Fake BLE + sanitizer suites |
| Flipper wins reconnect; revision conflicts/busy/SD errors | Keyboard dialog/transport | Swift/Python/C tests |
| Device keyboard overrides local run metadata only | Existing Flipper UI | C snapshot immutability tests; physical unverified |
| Explicit desktop macro layouts retained | Macro model | Shared fixtures; UI synthetic library |
| Fast/Slow/Variable/Custom preferences and saved-job pacing | Review/compiler/opcode 7 | Bounds/capability tests; C pause cancellation |
| Expanded-output review, relevant edits invalidate consent | RunReview/dialog | Unit tests + offline UI suite |
| Optional minimize-after-accept, no focus restoration claim | Run review/workspace | Explicit local setting; physical focus unverified |
| No direct-send/global hook/clipboard watcher/replay | Workspace | No input automation in tests |
| Actual local/remote versions, exact-tag asset availability/offline failure | Info & Companions | Metadata/release-response tests |
| Portable x64 bundle, per-user installer, no external Python | Windows build | Windows CI gates; not locally validated on macOS |
| Reinstall/upgrade path preserves libraries; uninstall preserves data | Inno installer | Isolated Windows CI reinstall/uninstall; previous Windows version does not exist |
| 100/150/200% scaling, minimum size, invalid-input UI | Tk workspace | Genuine Windows CI screenshots; not physical display certification |

Physical BLE/USB, real target focus/layout/global-shortcut behavior, real Windows
upgrade from a previously shipped binary and multi-hour stability remain
**unverified**. No such result may be inferred from a synthetic library, a host
engine test, an installer self-test or a screenshot.
