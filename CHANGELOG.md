## v0.4.0 (unreleased)

- Added symbol search for Binance Spot coins with USDT pairs and silent,
  near-live price-card refreshes.
- Fixed automatic refresh being postponed by repeated rendering of the same
  response; Markets now refreshes every two seconds while its card is open.
- Replaced the WTI and external Brent feeds with Binance Futures BZUSDT.
- Added silver pricing and a compact Markets price card; placed Markets directly
  below Internet Radio in the main menu.
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
