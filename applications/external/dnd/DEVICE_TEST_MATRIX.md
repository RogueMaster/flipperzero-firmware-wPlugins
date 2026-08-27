# Physical-device qualification matrix

This is the complete 2.7 hardware procedure. Compiler and automated validation are performed before packaging. A row remains `Pending hardware` until it is performed on a physical Flipper Zero; build success is never substituted for device evidence.

Record device serial suffix, SD-card model/capacity, battery level, firmware commit, operator, date, observed result, and any issue ID for every run.

| ID | Area | Setup and action | Expected result | 2.7 status |
|---|---|---|---|---|
| CTRL-01 | Navigation | Visit every Home destination; use Up/Down, short OK, and Back. | Selection remains visible, screens open once, Back returns one level, Home Back autosaves and exits. | Pending hardware |
| CTRL-02 | Long press | Exercise long OK/Left/Right on profiles, catalogs, Dolphin Bestiary, adventure, initiative, and dice. | Only documented alternate action fires; short action does not also fire. | Pending hardware |
| CTRL-03 | Editors | Edit every attack-template and structured-grant field, exit, reopen, and reboot. | Values autosave, display correctly, and survive reboot. | Pending hardware |
| DISP-01 | Truncation | Enter maximum-length character, item, spell, grant, monster, campaign, and participant text. | Rows remain navigable; labels and numeric values remain distinguishable; no drawing outside 128x64. | Pending hardware |
| DISP-02 | Core stats | Inspect passive statistics, all six saves, all 18 grouped skills, spell attack/DC, and initiative. | Every label/value is visible or predictably abbreviated with no overlap. | Pending hardware |
| DISP-03 | Animation | Roll 1, 2, 10, and 20 dice; roll weapon damage and page results. | Animation completes; every die, dice sum, modifier, and total can be reviewed. | Pending hardware |
| DISP-04 | Icon/sprites | Inspect launcher icon, campaign sprites, monster lists, and diagnostics. | 10x10 icon is crisp; rows and monochrome sprites render without corruption. | Pending hardware |
| SD-01 | Removal while idle | Remove SD on Home, edit a value, navigate through several screens. | App remains responsive, enters read-only fallback, and shows a persistent UNSAVED warning. | Pending hardware |
| SD-02 | Removal during save | Remove SD immediately after a rapid value change. | Save failure is detected; in-memory state remains usable; no false Saved message appears. | Pending hardware |
| SD-03 | Retry | Reinsert SD and select Home > Retry Save / Status. | Read-only mode clears only after success; pending character state is written and warning disappears. | Pending hardware |
| SD-04 | Removal during reads | Remove SD while opening a catalog, campaign, monster, profile list, and language pack. | Each operation fails with a clear status and returns safely without stale pointers or a crash. | Pending hardware |
| PWR-01 | Character interruption | Cut power during character temp-file write and again during publish. | Original or new checksummed generation loads; backup restore remains available; no mixed record is accepted. | Pending hardware |
| PWR-02 | Monster interruption | Cut power during custom block update, index rewrite, and delete. | Transaction recovery completes or rolls back; stable ID survives; bundled records are untouched. | Pending hardware |
| PWR-03 | Campaign interruption | Cut power during campaign progress replacement. | Previous progress backup or complete new progress loads; scene/checkpoint/flags are internally consistent. | Pending hardware |
| MEM-01 | Repeated allocation | Run 100 cycles opening/closing character catalogs, profiles, campaign scenes, bestiary pages, stat blocks, and generated encounters. | Every buffer is released on exit; responsiveness and available heap stabilize. | Pending hardware |
| MEM-02 | Largest data | Repeatedly page through the largest spell/item catalogs and the 340-monster browser in 20-record windows. | No out-of-memory error, reboot, frozen input, or progressive heap loss. | Pending hardware |
| MEM-03 | Forced low heap | With other memory-heavy firmware services active, open catalogs, language pack, monster detail, and encounter transfer. | Allocation failure is reported and control returns safely; existing data is not changed. | Pending hardware |
| LONG-01 | Long session | Keep the app open for 4 hours; perform at least 250 edits/rolls and 50 screen cycles. | Input remains responsive, saves remain valid, and heap readings do not show unbounded decline. | Pending hardware |
| PROF-01 | Profiles | Create, switch, duplicate, rename, export, import, archive, restore, and delete profiles. | Separate character, campaign progress, inventory, party preset, and initiative state remain isolated. | Pending hardware |
| PACK-01 | User packs | Install valid and intentionally broken monster/campaign/language packs. | Valid packs work; diagnostics identify exact invalid IDs/files/links; bundled assets remain read-only. | Pending hardware |

## Automated qualification for 2.7

| Check | Expected | Status |
|---|---|---|
| Host rules/parser/catalog/package suite | All assertions pass | Passed |
| RogueMaster dual-FAP compile and link | No warnings or errors | Passed |
| Firmware API validation | API 88.4 accepted | Passed |
| FAP metadata, packaged assets, FASTFAP, APPCHK | All stages complete | Passed |
| Source archive policy | No `dist`, FAP, ELF, object, or cache output | Passed |

Physical results must be entered only by the operator who ran them. A failed row blocks a stable hardware-qualified release until its issue is fixed and the row is rerun.
