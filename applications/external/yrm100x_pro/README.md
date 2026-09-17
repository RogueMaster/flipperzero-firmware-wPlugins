# YRM100X_PRO

YRM100X_PRO is a UHF RFID toolkit for Flipper Zero and compatible UART readers
from the YRM100/YRM100X family. It can inventory tags, acquire complete memory
dumps, inspect and edit memory banks, save tag records, clone selected data,
diagnose rewritable tags, and configure the external reader.

The current production-candidate build is version 1.7. File and console logging
are compiled out of this build so the application can run with the limited RAM
available on Flipper Zero, including while qFlipper Remote Control is active.

## What makes YRM100X_PRO different

- **Adaptive memory-bank handling.** The application does not assume that every
  tag has the same EPC, TID, User, or Reserved capacity. Full reads report each
  bank separately, and Clone probes the target where necessary so it can use a
  verified writable prefix instead of overrunning a smaller bank.
- **Rewritable-tag support beyond EPC.** EPC, TID, User, and standard Reserved
  password bytes can be selected independently. A bank is written only when it
  exists, fits the selected data, is writable, and accepts the supplied access
  password. TID remains read-only on most ordinary tags.
- **Model-aware PC3000/PC3400 conversion.** A PC3400/UMI state can depend on a
  model-specific value in User memory. Test UMI Auto learns the reversible
  marker for the detected TID model, and Clone later selects that marker
  automatically. Unknown models are rejected before Clone starts modifying the
  tag.
- **Bidirectional mode assistance.** When source and target use different
  PC3000/PC3400 states, Clone explains the mismatch and asks whether it should
  convert the target. The conversion is verified, and its marker is rolled back
  if a later required stage fails.
- **Verified writes.** Retryable Clone stages use the configured attempt count,
  display live Attempt X / Y progress, and read data back before reporting
  success. StoredCRC is left under tag control rather than copied as ordinary
  data.
- **Tag profiles.** Generic mode covers normal EPC Gen2 tags. The imp. 4QT
  profile applies conservative handling for Impinj Monza 4QT layouts and legacy
  dumps.
- **RAM-conscious interface.** Larger screens are created only when needed and
  released when returning to the main menu. The top-right main-menu value shows
  current free heap in kilobytes.

Support always depends on the actual tag IC. A read-only, permanently locked,
password-protected, damaged, or otherwise incompatible bank cannot be made
writable by software.

## Hardware and wiring

![Flipper Zero to YRM100X wiring](docs/images/yrm100x-flipper-wiring.png)

The tested five-wire connection uses the Flipper Zero USART port. Cross the
UART data lines:

| YRM connector | Signal | Flipper Zero GPIO |
| --- | --- | --- |
| Pin 1 | VCC | Pin 1, 5V |
| Pin 2 | RXD | Pin 13, USART TX |
| Pin 3 | TXD | Pin 14, USART RX |
| Pin 4 | EN | Pin 9, 3.3V |
| Pin 5 | GND | Pin 8, 11, or 18, GND |

VCC is the reader's 5 V supply; EN is a separate 3.3 V enable signal. The
application enables Flipper's 5 V OTG output when USB-C VBUS is absent. Some
reader boards or transmit-power settings may require more current than Flipper
Zero can provide reliably. For external power, use one suitable regulated 5 V
supply for reader VCC, share ground with Flipper, and leave Flipper pin 1
disconnected. Never join the positive outputs of two 5 V sources.

Do not connect or disconnect wiring while either device is powered.

The [detailed hardware guide](docs/HARDWARE.md) contains the vector diagram,
power notes, and three photographs of the tested Flipper, protoboard, YRM1005
module, and antenna assembly.

![YRM100X reader and Flipper Zero assembled](docs/images/yrm100x-flipper-assembled.jpg)

## Startup and basic controls

At startup the application makes two reader connection attempts. The screen
shows **Antenna connecting attempts [1/2]** and, after a failed first attempt,
waits one second before the second attempt.

When the reader responds, the application displays its hardware version,
firmware version, and manufacturer information. Press the center **OK** button
to enter the main menu. If both attempts fail, the application shows
**Antenna Not Connected!** Reader-dependent actions remain blocked until a
connection is available.

Common controls:

- **Up/Down** moves through menus or result pages.
- **Left/Right** performs the labeled action at the bottom of a custom screen.
- **OK** opens a menu item or starts/stops the current operation.
- **Back** returns to the previous screen. While a reader operation is active,
  physical Back first performs the same safe stop action as the on-screen Stop
  button, so the RF operation is not left running in the background.

## Main menu

**Read (Multi)** performs a fast inventory of all visible tags. It is intended
for quickly finding EPC values and signal strength when several tags may be in
the RF field. Press **Stop** when the inventory is complete. If **Full After
Stop** is enabled, the application then attempts to enrich the collected EPCs
with full-bank data. **Auto Save Multi** controls automatic saving.

**Read Full(Multi)** inventories multiple tags and then acquires EPC, TID, User,
and Reserved data for each tag. Every bank keeps its own success or failure
status, so a protected or missing User bank does not hide a valid EPC/TID read.
**Full Attempts** controls the maximum number of complete acquisition passes.
Use this mode when detailed dumps matter more than inventory speed.

**Read Forever** repeatedly performs complete scan cycles. Between completed
cycles it shows the **Forever Delay** countdown, then starts the next scan. Once
more than one result has been collected, **Prev** and **Next** browse the
history and the counter shows the current item, for example **2/5**. Use
**Stop** or physical Back to stop the loop before leaving the screen.

**Read (Single)** stops after selecting one visible tag and is useful for quick
targeted work. Keep only the intended tag in the RF field whenever a later
write, lock, password, or kill action will use this result.

**Read Full (Single)** acquires a complete dump from one tag, including
per-bank availability and status. It uses the configured full-read attempts and
is the preferred source for a saved record that will later be inspected or
cloned.

**Get Info Bank on Tag** opens two diagnostics:

- **Get Size Bank** probes EPC, TID, User, and Reserved and reports the detected
  size or status of each bank together with PC/CRC information. Results reflect
  the current tag, password, lock state, reader link, and RF conditions.
- **Check TAG Rewritable** tests whether each bank can be rewritten and restored.
  This is a destructive-risk diagnostic: it temporarily changes tag data, and
  restoration can fail if the tag is removed, loses power, becomes inaccessible,
  or rejects a write. Keep exactly one tag in the RF field and save a known-good
  dump first.

**Saved Dumps (N)** opens the persistent database; N is the current number of
saved records. The list is paged so a large collection does not consume all
available RAM. Selecting a record opens its actions. The bottom command can
delete all saved records and standalone auto-dump files after confirmation.

**Test UMI Auto** learns how a particular rewritable tag model changes between
the PC3000 and PC3400/UMI states. It keys the result by the first four TID model
bytes and stores markers for up to eight models. During the test the application
temporarily writes candidate User words, immediately restores each original
value, verifies the restore, and records only a proven reversible transition.
This function writes tag memory; use one tag, do not move it during the test,
and stop if **RESTORE FAILED** appears.

**Configure** contains connection, reader, acquisition, cloning, and feedback
settings. Each item is described below.

**About** shows the application name, version, project URL, and credits.

## Read results and tag data

After a read, the EPC result screen shows EPC, PC, CRC, RSSI, and available
navigation commands. **More** opens detailed bank information and actions.
When several results exist, **Prev/Next** changes the selected result and the
history counter identifies its position.

The detailed data pages separate EPC, TID, User, and Reserved values. A missing
or failed bank is shown as unavailable rather than being silently replaced with
zeroes. Long values are split across pages so the complete bank can be reviewed
on the Flipper screen.

## Live-tag actions

A tag reached directly from a read result has the following actions:

- **Save** adds the complete current record to Saved Dumps.
- **Update** edits and writes a selected bank back to the currently targeted tag.
- **Lock** changes the Gen2 lock state of Kill password, Access password, EPC,
  TID, or User memory.
- **Kill** permanently disables a tag using its Kill password.
- **Clone** copies selected data to another compatible rewritable tag.
- **Update AP** changes the tag Access password.
- **Update KP** changes the tag Kill password.

Live write actions are EPC-targeted after the tag has been found. Nevertheless,
keep only the intended tag in range and verify the displayed EPC before
confirming any change.

## Saved-dump actions

Selecting a record in Saved Dumps opens:

- **Tag Data** to inspect EPC, PC, CRC, TID, User, and Reserved data.
- **Rename** to change the display name without changing tag contents.
- **Write** to choose a stored bank value and write it to a detected target.
- **Delete** to remove that saved record after confirmation.
- **Clone** for normal selectable-bank cloning.
- **Clone_PC3400** only when the saved source PC is exactly 3400. Its presence
  is therefore data-dependent, not firmware-dependent.

## Write, password, lock, and Kill operations

The Write screen lets the user choose an available source bank, edit its hex
value where applicable, and send it to the target. **Save on Write** controls
whether the resulting state is also added to Saved Dumps.

The lock screen accepts the Access password, a memory bank, and one of these
Gen2 lock modes:

- **Unlock** makes the selected bank accessible according to ordinary password
  rules.
- **Perm-U** permanently unlocks the selected bank when the tag supports it.
- **Lock** password-protects the selected bank.
- **Perm-L** permanently locks the selected bank.

Permanent lock modes cannot normally be reversed. The Kill operation is also
permanent. Never test either operation on a tag you need to recover.

## Clone workflow

Clone begins with a bank-selection screen:

- **EPC** copies the source PC/EPC layout that fits the target. StoredCRC is not
  copied as an ordinary word; the tag calculates and maintains it.
- **TID** is available only for a target whose TID memory is genuinely writable
  and large enough. Most retail tags have a factory-programmed read-only TID.
- **User** copies source User data. When a target bank is smaller, the adaptive
  fitting logic uses only a verified writable prefix where the selected mode
  permits it; required exact data is never silently reported as complete when
  it did not fit.
- **User PC3400** in dedicated PC3400 Clone uses **Auto TID**: the target TID
  model selects a marker previously proven by Test UMI Auto or a built-in
  verified profile. Pressing this row explains the rule on screen.
- **Reserved[!]** copies only standard password bytes supported by both source
  and target: four bytes for Kill password or eight bytes for Kill plus Access
  password. The warning marker reflects the risk of changing access control.
- **[ START CLONE ]** becomes **OK** when at least one usable bank is selected.

After **START CLONE**, place the target tag in range. The application reads its
identity and bank capabilities, shows the planned banks, and waits for explicit
**Write** confirmation. During writing it displays **Attempt X / Y**. The value
of Y is **Clone Attempts** from Configure, from 1 to 5.

If source and target disagree on PC3000/PC3400 state, the application offers a
mode conversion before continuing. It modifies only the model-specific User
marker required for the transition, verifies the resulting PC, performs the
selected clone stages, and verifies the final state. On a required-stage
failure it attempts to restore the original marker and reports the final PC.

Clone retries can recover transient UART, RF, singulation, or tag-response
failures. They cannot bypass a wrong password, permanent lock, read-only bank,
unsupported IC, insufficient bank capacity, or an unknown UMI marker.

## Configure menu

**Connection — Connect/Disconnect** opens or closes the UART reader connection.
When disconnected, reader-dependent main-menu actions are blocked and hardware
settings show **LOCKED**.

**About Antenna — Open** queries the same reader identification shown at startup
and keeps the result on screen until Back is pressed.

**Power Level — 15 to 26 dBm** changes reader transmit power. Use the lowest
power that gives reliable operation and comply with local RF regulations.

**Baud Rate — 9600/115200/384000** changes the UART speed used by compatible
readers. The normal initial value is 115200. An unsupported value can interrupt
communication until reader and application settings match again.

**Region — USA/EU/Korea/China 800/China 900** selects the reader frequency plan.
Choose the region legally permitted at the physical operating location.

**Default AP** is the four-byte Access password used by protected read/write
operations unless another screen requests a password. The default is 00000000.

**Save on Write — No/Yes** controls whether a successful manual write creates a
new Saved Dumps record. The default is No.

**Session — S0/S1/S2/S3** selects the EPC Gen2 inventory session. The default is
S2. Session behavior affects how quickly inventoried tags return to the ready
state.

**Target — A/B** selects the EPC Gen2 inventory target flag. The default is A.

**Full After Stop — No/Yes** controls whether fast Read Multi performs detailed
bank acquisition after inventory is stopped. The default is Yes.

**Auto Save Multi — No/Yes** controls automatic persistence of Read Multi
results. The default is Yes.

**Tag Profile — Generic/imp. 4QT** selects software interpretation rules.
Generic is the default. Use imp. 4QT for compatible Impinj Monza 4QT tags and
legacy 4QT dumps that need their conservative TID/User handling.

**Full Attempts — 1 to 10** sets the maximum complete acquisition passes used by
full-read workflows. The default is 4.

**Clone Attempts — 1 to 5** sets the maximum attempts for each retryable Clone
I/O stage. The default is 5, and progress is visible while writing.

**Forever Delay — 1 to 60 seconds** sets the pause between completed Read
Forever cycles. The default is 2 seconds; the remaining time is shown on screen.

**Sound — Off/On** enables or disables application sound feedback. The default
is On.

**Vibration — Off/On** enables or disables application vibration feedback. The
default is On.

Settings are stored in the application data directory and survive a FAP update.

## Installation

Install the released FAP through the Flipper Application Catalog when the app
becomes available there. For a manual install, copy **yrm100x_pro.fap** to:

    /ext/apps/GPIO/yrm100x_pro.fap

Application data is stored under:

    /ext/apps_data/yrm100x_pro/

Replacing the FAP does not remove settings, learned UMI markers, or saved dumps.

## Build from source

Install the current [uFBT](https://github.com/flipperdevices/flipperzero-ufbt),
open a terminal in the repository root, and run:

    ufbt

To build, install, and launch on a connected Flipper Zero:

    ufbt launch

To verify source formatting before a catalog submission:

    ufbt lint

The project targets the latest official Flipper firmware release supported by
uFBT. The GitHub workflow builds and lints every push and pull request against
the official release SDK.

## Saved data

The application data directory may contain:

- **Saved_EPCs.txt** — persistent Saved Dumps records and their bank data.
- **Index_File.txt** — a rebuildable saved-record count/index cache.
- **dumps/** — standalone full-read and Read Forever dump files.
- **ReaderSettings.ff** — persistent Configure values.
- **UmiMarkers.ff** — model-specific markers learned by Test UMI Auto.

Use **Delete ALL saved** inside the application to remove the saved database and
standalone auto dumps together. Learned UMI markers and normal settings are kept
separately.

## Safety and legal use

Use YRM100X_PRO only with tags and systems you own or are explicitly authorized
to test. Radio regulations, permitted UHF bands, power limits, and RFID access
rules vary by jurisdiction.

Writing, cloning, password changes, locking, Check TAG Rewritable, Test UMI
Auto, and Kill can alter or permanently disable a tag. RF loss during a
temporary-write diagnostic can prevent restoration. Always keep one target in
the field, use a stable power supply, verify the displayed tag identity, retain
a known-good dump, and read every confirmation screen.

## Catalog screenshots

The Flipper Application Catalog requires original, unmodified screenshots made
with qFlipper. The exact capture list and instructions are in
[screenshots/README.md](screenshots/README.md). The first image should be the
clean main-menu preview. Do not resize, crop, recolor, annotate, or reconstruct
qFlipper output.

## Credits

YRM100X_PRO is based on William Riley Haffner's MIT-licensed
[Simultaneous UHF RFID Reader](https://github.com/haffnerriley/Simultaneous-UHF-RFID-FlipperZero).
The original YRM100 driver lineage is credited in the source to
[frux-c](https://github.com/frux-c/uhf_rfid).

Production maintenance and the YRM100X_PRO feature set are by
[@AlexeySmirnov74](https://github.com/AlexeySmirnov74).

## License

This project is released under the [MIT License](LICENSE).
