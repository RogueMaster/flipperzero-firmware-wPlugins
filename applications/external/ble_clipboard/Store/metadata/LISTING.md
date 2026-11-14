# Draft Store listing

All fields are drafts. No Store record, pricing schedule or release is created.

- Name: BLE Clipboard
- Subtitle: USB typing with Flipper Zero
- Primary category: Utilities
- Business model: paid upfront, no in-app purchases or subscription
- Price, seller/team, territories and SKU: owner confirmation required
- Release: manual, after approved beta and App Review
- Support URL: https://github.com/otcan/ble-clipboard/issues
- Privacy policy: publish the reviewed `PRIVACY.md` at a stable public URL first
- Marketing URL: repository README after the development branch is merged
- Source URL: immutable release tag matching the submitted binary, not moving main

## Description

Send text and saved macros from your Mac over Bluetooth to your Flipper Zero,
then type them into a USB-connected device. A Flipper Zero running the compatible
independent BLE Clipboard 0.5.2 application is required. The physical Flipper
device is not included; the open-source companion is distributed separately.

Keep a local library of reusable snippets and structured text, key-combination
and delay steps. Add clipboard, timestamp and prompted values, review the output,
and explicitly start each run. Import and export macro definitions as JSON.

The Mac app has no cloud account, subscription or clipboard history service.
It types USB keyboard input rather than setting the target's OS clipboard.
ASCII text, tabs and line breaks are supported, plus experimental native Turkish
letters with Turkish Q; arbitrary Unicode is not.
Choose from 28 keyboard layouts, which must match the target configuration;
most non-US layouts require their corresponding Flipper SD assets; macOS Turkish
Q uses a built-in QWERTY-PC map.

Only use with devices you own or are authorized to control. Keystrokes cannot
be undone, and line breaks can submit forms or execute input. Review each run.

Independent open-source software, not affiliated with Flipper Devices or Apple.
Source and license notices are available publicly. Purchasing the Store download
supports the maintained Store distribution; the source remains available.

## Screenshots and review assets

Capture real Release UI at Apple's accepted Mac screenshot sizes. Suggested
views: Quick Paste, saved macro steps, parameters and expanded run review. Do
not represent a mocked/offline screen as a successful device connection.
Provide a physical hardware demonstration and exact compatible firmware/app
versions, not just a simulator recording. See REVIEW.md.

## Owner-entered submission fields

Age rating, accessibility claims, encryption/export compliance, current pricing,
seller/trader details and support contact must reflect the actual build and
legal entity. Do not claim VoiceOver, layout, full Unicode or hardware support
that has not been tested. No age-rating or encryption answer is inferred here.
