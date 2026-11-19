## v0.5.1 (2026-11-19)

- Fix portable desktop USB discovery to use official Flipper VID/PID
  `0483:5740`, shared by single and dual CDC; HELLO identifies the bridge.
- Recognize generic Windows serial-device names by hardware identity in the
  CLI, while retaining name hints only when USB metadata is unavailable.
- Enforce the initial HELLO deadline even when a wrong port continuously sends
  unrelated bytes, allowing scanning to continue to the correct CDC port.
- Add regression coverage for real USB IDs, generic COM names, unrelated USB
  devices and noisy-port recovery. Preserve explicit consent, pairing, protocol
  identity checks, radio behavior and all Toolbox tools.
- Include bounded transport-error details in portable desktop diagnostics.
- Build/test the native macOS release on a clean Apple Silicon CI runner.

## v0.5.0 (2026-10-09)

- Keep currency amount-editor copies explicitly bounded for newer compilers.
  Desktop packaging CI uses an editable checkout so the source package does not
  hide the installed radio decoder; Windows/Linux jobs run independently.
- Completed the reusable source SDK with explicit USB/Bluetooth connection,
  nonce-bound computer approval, code/status polling and recognition revocation.
  Added a ready-made connection screen and dependency-free one-command export.
  The example builds the exported SDK with two wildcard source patterns; the
  legacy USB shortcut remains available. Pairing still requires separate desktop
  internet consent. Added SDK/modal/export regression tests.
- Moved the Flipper application from USB to Tools to reflect both USB and
  Bluetooth modes. Kept its application ID and filename unchanged; updated
  README installation paths and the latest interface/validation notes.
- Fixed National Today word spacing and real-font wrapping. Removed the extra
  subtitle to show four body lines, retain inline bold names, and explicitly mark
  shortened previews without increasing the response buffer. Added extractor
  and glyph-advance regression tests for macOS and the portable companion.
- Refreshed Wikipedia and National Today with padded, measured text readers;
  preserve National Today's inline bold names, remove unreadable Unicode glyphs,
  and keep search/refresh and Back behavior. Weather and ISS use small paged
  telemetry cards without increasing radio buffers or adding a second FAP.
- Added a compact optional Qt Widgets desktop interface for Windows/Linux,
  matching macOS USB/Bluetooth controls with independent owners, explicit
  cancellable consent/code dialogs, Pairings and bounded Diagnostics. The CLI
  remains available. Added backend, real-widget and virtual-serial regression tests,
  including rapid Off/On waiting for every still-closing serial owner.
- Removed the terminal serial writer's unbounded drain/flush operation so a
  stalled driver cannot prevent consent/cancellation processing. Writes remain
  bounded by the existing write timeout.
- Added target-platform GUI packaging/preview CI and recoverable Linux GUI
  installation; physical Windows/Linux validation is still pending. Keep the
  macOS build-staging app in Build.noindex to avoid duplicate Spotlight icons.
- Use Internet Bridge on Flipper and Flipper Internet Bridge on desktop; the first screen
  still has separate USB Internet Bridge / Bluetooth Internet Bridge modes.
- Renamed the product to Internet Bridge. The first Flipper screen selects USB
  Internet Bridge or Bluetooth Internet Bridge; Back ends that mode and returns
  to the selector. Bluetooth no longer lives in Demo; radio remains USB-only.
- Split native macOS connection ownership into independent USB/Bluetooth
  coordinators, network clients and consent lifetimes with a compact SwiftUI
  status panel. Owner-scoped modal queuing prevents one connection's shutdown
  from dismissing another's code/consent prompt. Preserve app identifiers,
  stored USB grants and Bluetooth recognition credentials.
- Added independent-session and prompt cancellation regression tests, plus
  portable transport-specific menu/USB-only radio checks.
- Removed Bluetooth radio only; kept Bluetooth Demo, Requests, Pairings and all
  Toolbox tools. Old Bluetooth radio profiles are rejected before networking.
- Fixed USB radio memory pressure by moving the same minimp3 decoder and mono
  resampler to the desktop helper. Flipper keeps its original 14,493 Hz, 16-bit
  speaker ring, gain and PWM path. No separate Radio FAP or runtime FFmpeg.
  Update both the helper and FAP; PCM requires explicit capability negotiation.
  Shared Windows/Linux hosts include the same optional compiled C decoder.
- Apply USB endpoint backpressure rather than dropping packets when the speaker
  ring fills. Bound terminal-host write batches so Cancel/Ping remain serviceable.
  Reject oversized wire chunks, unnegotiated PCM, wrong content type and incomplete
  samples. Keep response limits and the reduced, actual memory admission guard.
- Fixed false frame/request timeouts when callbacks update progress after a
  timer samples the clock; added regression tests for that race and tick wrap.
- Renamed the macOS Bluetooth management menu/window to Demo. Added Flipper
  Demo > Pairings > Known Computers, paginated host-ID aliases and Keep/Revoke
  confirmation. Revocation affects only bridge recognition, not USB grants or
  system bonds; active peers receive a nonce/host-bound revocation notice.
- Added opt-in Windows/Linux terminal Demo, secure-keyring pairing storage and
  Bleak transport with manual Pair/Connect and fresh Allow Once/Deny. Native
  Windows/Linux BLE hardware and binary packaging remain unverified locally.
- Serialize macOS pairing revocation through the BLE writer; do not interleave
  internet-permission frames during teardown. Remove stale desktop recognition
  when Flipper requires a new code after a local/offline revoke.
- Added Flipper Demo > Requests (Connection Requests): Pair/Connect/Reject before bridge
  code or recognition acceptance, including known hosts. Alpha On does not
  automatically show the bridge code. Separate bounded selection/code deadlines
  and a required BLE-only capability keep old automatic-code peers fail-closed.
  One incoming candidate is shown by random host-ID alias, not a nearby-Mac scan.
  Native OS PIN and fresh Mac internet consent stay independent; USB unchanged.
- Fixed BLE discovery advertising overflow: retain the FZ Bridge device-name prefix with the
  existing firmware 16-bit serial discovery marker instead of the oversized
  128-bit advertisement. Mac also checks advertised local-name prefix; GATT,
  bridge-code and internet-consent gates remain unchanged. Added budget tests.
- Replaced Bluetooth block/manual-Forget management with bridge code recognition:
  Revoke Pairing deletes the row/Keychain credential, reconnect requires code,
  recognized peers skip code but still require fresh internet consent. No system
  settings links or Pair Again button. Advertise the bridge with the FZ Bridge prefix.
  Native BLE bonds/security and USB behavior remain separate and unchanged.
- Added opt-in Demo > Bluetooth Alpha at the bottom of the Flipper main menu,
  plus a matching native macOS toggle. Uses a distinct BLE profile and existing
  FIBP framing, fresh one-time consent, acknowledged bounded transfers, and an
  8 KiB GET response cap. USB remains default; radio remains USB-only.
- Restore the normal firmware Bluetooth profile when the alpha is disabled or
  the FAP exits; retain existing bonds and cancel the old bridge session.
- Allocate the radio's 28 KiB reservoirs only when playback starts, and drain
  in-flight body callbacks before freeing them during transport switches/exit.
- Initialize USB session state before starting callbacks, preventing stale
  connectivity from overwriting a newly established HELLO during USB recovery.
- Added rounded, padded magnitude/depth fields and location cards, plus a
  single dictionary reader panel with a small scrolling rail. Counters are
  measured to fit 10/10; the crowded MAG label and control legend stay removed.
- Replaced sequential currency entry with two independently selectable
  currency fields and two editable amount fields; the last edited amount
  drives a two-way conversion using the fetched reference rate.
- Added dedicated earthquake cards with separate magnitude/location/depth/time
  areas and dictionary pages with word, part-of-speech and scrollable meanings.
- Added bounded card parsing, reverse-conversion and host-only UI input/
  scrolling tests, including callbacks after releasing the view-model lock.
- Added Latest Earthquakes with the ten newest USGS earthquake records,
  magnitude, UTC time, depth, and manual refresh.
- Added Currency Converter with 21 selectable currencies, decimal amounts,
  Frankfurter v2 reference rates, and the provider date.
- Added English Dictionary with up to three meanings, examples, source and
  license attribution, and bounded ASCII display text.
- Added matching bounded response transformations to macOS and cross-platform
  desktop hosts. These tools require helper version 0.5.0 or newer.
- Cancel requests when leaving a tool so late responses do not reopen it.
- Added shared provider fixtures and Toolbox parser tests to CI.

## v0.4.0 (2026-09-21)

- Grouped Wikipedia, Weather, National Today, ISS, Internet Radio, and Markets
  inside a dedicated Toolbox menu with consistent Back navigation.
- Added symbol search for Binance Spot coins with USDT pairs and silent,
  near-live price-card refreshes.
- Fixed automatic refresh being postponed by repeated rendering of the same
  response; Markets now refreshes every two seconds while its card is open.
- Fixed Markets timestamp rendering and a bounds error in its timestamp
  validation that could reject valid provider responses.
- Added sanitizer-backed Markets and protocol parser checks to CI.
- Replaced the WTI and external Brent feeds with Binance Futures BZUSDT.
- Added silver pricing and a compact Markets price card; placed Markets directly
  below Internet Radio in Toolbox.
- Kept the price card text clear of the Refresh button area.
- Added Brent crude oil pricing to Markets.
- Fixed Back navigation while Markets waits for connection permission.
- Reject incomplete market responses, duplicate price fields, and nested data.

- Added a single Markets menu with BTC/USDT, ETH/USDT, and gold in USD per
  troy ounce, including refresh and bounded response validation.
- Added a complete, independently buildable client SDK example FAP.

## v0.3.0-rc.2 (2026-09-15)

- Added a source-level Flipper Bridge Client SDK for other FAP applications.
- Added a cross-platform Windows/Linux/macOS command-line host.
- Added direct TLS, DNS/redirect SSRF checks, terminal permission prompts, and hashed persistent grants to the cross-platform host.
- Added automated Windows and Linux binary packaging.
- Added the macOS application icon to the Windows executable and a no-root
  Linux desktop launcher package with the same icon.

## v0.2

- Initial Apps Catalog release.
- Added an authorized HTTPS bridge through a macOS menu bar helper.
- Added Wikipedia search, weather, National Today, ISS tracking, and internet radio.
- Added bounded streaming, cancellation, timeouts, and SSRF protections.
