# Artwork

Master app icon: `BLEClipboardMac/Resources/AppIcon.png`.
Generated with the built-in OpenAI image-generation tool on 2026-11-01.
`scripts/build_icon.sh` resizes the master and produces `AppIcon.icns`.

## Generation Prompt

> Use case: logo-brand. Asset type: production macOS application icon, square
> 1024x1024, for a native utility called BLE Clipboard that transfers clipboard
> text wirelessly to a Flipper Zero and types it through USB. Create one polished,
> immediately legible macOS app icon: a thick matte off-white clipboard silhouette
> and a single vivid orange right-pointing transfer arrow through its lower half,
> on a softly raised graphite-black macOS superellipse tile with subtle restrained
> dimensional lighting. A tiny mint-green circular connected indicator near the
> clipboard's upper-right. Simple bold geometry, excellent silhouette at 32x32,
> precise high contrast, premium understated desktop utility, centered on canvas
> with normal macOS icon safe margins. The rounded-square tile is the icon itself;
> outside that tile use a genuinely transparent alpha background. No text, no
> letters, no watermark, no extra objects, no animals or mascots, no background
> scene, no large shadow outside the tile. Return a single icon, not a sheet or mockup.

The icon uses a status indicator as part of its mark; it does not indicate the
application's live connection state. Screenshots in the README show the actual
native app rather than an image-generated UI mockup.

## Documentation Screenshots

### Live Run and USB Output

Captured on 2026-11-05 with the owner's permission, using the installed 0.5.3
Mac and Flipper apps, not a preview or simulated connection:

- `docs/assets/mac-live-running.jpg`: 2040x1440 native Mac window capture while
  README Demo was running, during its final 25-second delay after all text had
  reached the USB target. Progress and Stop are the real app state.
- `docs/assets/mac-live-complete.jpg`: 2040x1440 capture of the same app after
  it reported USB output complete.
- `docs/assets/usb-macro-output.jpg`: 1400x866 capture of the actual 99-byte USB
  output in a temporary native AppKit text receiver on the target Mac. This is
  a scratch test tool, not a shipped companion or the Flipper screen. Its window
  was resized before capture; the image was not cropped.

The successful macro contained one literal-text step between two delays, with
no shortcuts, mouse movement, clipboard access, templates or SD snapshot save.
Received text matched byte-for-byte; see the
[live validation record](HARDWARE-VALIDATION.md#053---live-text-and-delay-run)
for scope and interrupted earlier attempts. The temporary macro was removed
and the scratch receiver closed afterward, preserving the existing macros.
All three JPEGs are original window captures without compositing, retouching,
overlaid status or generated UI elements.

### Offline Mac Previews

`docs/assets/mac-quick-paste.jpg` and `docs/assets/mac-macros.jpg` are unedited
2040x1440 captures of the native 0.5.3 Mac UI. They were captured from a separate
copy of the released bundle with the isolated
`com.oguzcanunver.BLEClipboardMac.Readme.InfoPreview` identity and `--ui-testing`.
That mode disables Bluetooth and uses a temporary library with built-in example
macros. The checklist is manually entered, non-sensitive sample text. No clipboard
load, connection, job execution or saved-job transfer was performed. The temporary
app was closed after capture; the owner's installed app was not replaced or quit.
The JPEG files are the original window captures, without cropping, compositing,
retouching or generated UI elements.

### Flipper Device Screens

`BluetoothClipboard/screenshots/info.png` and `downloads.png` are the existing
genuine 0.5.3 device screenshots exported by qFlipper. Their original PNG bytes
are unchanged. See the [device capture record](../BluetoothClipboard/screenshots/README.md)
for the read-only Info-mode procedure and provenance.

The older `docs/assets/mac-app.png` records the historical 0.1.0 UI; it is not
used to represent the current app.

### Windows 0.5.4 Offline UI

`docs/assets/windows-editor.png` (960x640) and `windows-review.png` (760x660)
are original PNG client-area captures of the native Tk Windows app, copied
byte-for-byte from [CI run 42702860795](https://github.com/otcan/ble-clipboard/actions/runs/42702860795),
source `3df96891806fa80eb80a0e4a01aa7d916310e4e5`.
The runner was Windows Server 2022 x64 with Python 3.12.10. Capture used Pillow's
Windows HWND capture, not a mockup, generated UI, annotation or composite.
No pixels were cropped, resized, retouched or replaced after capture.

The synthetic temporary library contains only example text/actions. Bluetooth is
disabled by construction (`worker is None`); no clipboard read, pairing, saved-job
transfer, mouse output or typing occurred. Run is disabled in offline review.
The source version is 0.5.4; these screenshots do not certify a physical Windows
BLE/USB session or a multi-hour run.

The separate CI UI artifact retains editor/Quick Paste/review/errors, step and
parameter dialogs, 32-parameter review and minimum sizes at 100/150/200% Tk
scaling. The runner's small desktop requires overlapping, unedited viewport tiles
for larger windows. Each tile has a pixel-offset receipt; tiles are not stitched
into a misleading full-window screenshot. The two README captures fit completely
inside the runner work area and contain no other application's content.
