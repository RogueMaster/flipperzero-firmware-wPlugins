# Changelog

## 0.1

- Initial release.
- Cover, opening-animation, and page screens for a book-of-answers oracle.
- Answers seeded from live device state: BLE, SubGHz, and NFC sampling
  (receive only), device time and battery telemetry, and the hardware random
  generator, folded into a 32-bit hash.
- Five answer categories (Forward, Observe, Warning, Explore, Reflection) with
  Normal, Rare, and Legendary rarity rolls.
- Each opened page is appended as a record to the app data log.
