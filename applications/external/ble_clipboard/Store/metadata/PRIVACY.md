# Privacy policy draft

This draft describes the current source, not an already published Store service.
Review it against the exact shipping binary, add the accountable publisher and
effective date, then publish it at the Store listing's public privacy-policy URL.

BLE Clipboard does not use analytics, advertising, tracking or a developer-run
server. It does not send clipboard text, macro definitions or run arguments to
the developer. Macro execution data transfer is to the Flipper the user selects over
Bluetooth, then to the USB target through keyboard/mouse input.

Clipboard text is read on an explicit Load Clipboard action or when preparing a
macro that references the clipboard. The app does not monitor clipboard changes.
Run snapshots and prompted values are kept in memory and are not intentionally
written to the Mac library. Local crash reports and operating-system behavior
remain controlled by Apple/macOS and the user's settings.

Saved macro definitions and one recovery backup are stored locally in the app's
sandbox. Literal text saved in a definition is persisted. Exported JSON includes
the selected definitions and is not encrypted. Users choose whether and where
to share exports. Local files can be included in device/OS backups.

The Flipper/firmware manages Bluetooth bonding keys. The independent 0.5.2
companion keeps small jobs in RAM and large jobs in a temporary SD file, cleared
on completion, cancellation, Flipper app exit and next startup. Normal BLE loss
clears the job; explicit standalone runs retain it until stopped or complete.
Send to Flipper explicitly stores reviewed snapshots, including expanded clipboard
and parameter values, as plaintext on SD. These survive restart until deleted;
staging/recovery files can remain after interrupted saves. No job auto-runs or
resumes after restart. Physical Left/Back stops a standalone run even if the Mac
is closed. The USB device and input activity remain observable; Mouse Jiggle is
not an undetectability or human-activity guarantee.
Ranged movement and intervals are sampled on Flipper when executed. This does not
read the target cursor position or collect user-activity telemetry. Saved clipboard
and parameter values remain fixed snapshots; random movement values are not Mac
clipboard/template data.
The Mac app does not provide a secure-erasure or password-vault guarantee.
Do not use it to handle secrets on devices/storage you do not trust.

Users can delete individual macros in the library, remove exported copies and
remove the app's container to erase its local library/backup. Deleting a macro
does not securely erase prior backup, export, target, Flipper or OS backup data.
The publisher does not have a server copy to retrieve or delete.

Opening a source/support link uses the user's browser and the destination's
privacy policy. Do not include sensitive clipboard content in public issues.

Opening Info & Companions, or refreshing release availability there, makes an
anonymous HTTPS request to api.github.com for the running app's exact release
version. It uses an ephemeral session without cookies or credentials. GitHub
receives the connection's IP address and a User-Agent containing the app version,
not clipboard contents, macros, parameters, the Flipper name or Bluetooth IDs.
There is no background release polling, analytics or automatic download/install.
Download/document links open the browser and are governed by GitHub's policy.
Store network-client entitlement is used for this explicit release check.

Draft contact route: https://github.com/otcan/ble-clipboard/issues.
Publisher identity and effective date: **required before publication**.

Draft App Store privacy answers: no developer collection or tracking, subject
to a final audit of the shipping build and any added dependencies. The bundled
privacy manifest declares no tracking/collected data/required-reason API use in
the new Store target. Re-audit if runtime code or SDKs change.
