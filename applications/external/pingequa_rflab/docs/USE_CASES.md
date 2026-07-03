# Use Cases — 10 Real-World Scenarios

PINGEQUA RF Lab is a passive 2.4 GHz spectrum analyzer. Here are practical things to do with it.

## 1. Find the least-busy WiFi channel for your home AP

**Problem**: Your WiFi feels slow. Maybe a neighbor is on the same channel.

**Steps**:
1. Launch app, place Flipper near your router
2. Wait 30 seconds for accumulation
3. Press **OK** when bars stabilize (or wait for auto MAX HOLD)
4. Note which ▲ marker has the **shortest** bar — that WiFi channel (1, 6, or 11) is the least busy
5. Log into your router → set WiFi channel to the least-busy one
6. Re-scan to verify

**What to look for**: a clear "valley" between markers. If all three ▲ have tall bars, your area is congested — consider 5 GHz or wired.

## 2. Locate an unknown 2.4 GHz emitter in your space

**Problem**: Something in your office is causing wireless lag, but you don't know what.

**Steps**:
1. Walk Flipper around the room while scanning
2. Watch for bars that grow when you approach a specific area
3. Move cursor onto the suspect bar to read frequency
4. Compare frequency to known protocols:
   - 2410–2415 MHz → likely WiFi 1 area
   - 2420–2435 MHz → maybe wireless mouse / Bluetooth
   - 2440–2450 MHz → microwave oven leakage / WiFi 6 / BLE 38
   - 2455–2470 MHz → WiFi 11 / wireless A/V

**Pro tip**: bring dwell up to 500 µs to catch sporadic emitters.

## 3. Check if your microwave oven is leaking out-of-band

**Problem**: Microwave ovens operate at 2450 MHz, but worn seals can leak across the band.

**Steps**:
1. Turn microwave on (with water inside)
2. Scan from 1 meter away
3. Healthy microwave: tall bar at ch 50 (2450 MHz), maybe small spread ±5 channels
4. Leaky microwave: significant bars beyond ±10 channels — consider replacement

⚠️ Don't put Flipper inside the microwave 😅

## 4. Discover BLE advertising activity

**Problem**: Curious which BLE devices are nearby and how active they are.

**Steps**:
1. Launch app, start scanner
2. Watch the three ■ BLE markers (channels 2, 26, 80)
3. Brief spikes under any ■ = a BLE device just advertised
4. Sustained activity = many BLE devices or a busy beacon (Apple/Google location, etc.)

This won't tell you *which* devices, but it tells you *whether* the BLE band is busy.

## 5. Verify your RC drone / FPV channel before takeoff

**Problem**: You're at a flying field with other pilots; want to avoid frequency conflict.

**Steps**:
1. Power on your drone in standby (transmitter active)
2. Scan with Flipper
3. The drone's link should appear as a tall bar at one specific channel
4. Move cursor to it, note the frequency
5. Coordinate with other pilots if frequencies overlap

**Note**: Many modern drones use 2.4 GHz with frequency hopping (FHSS). You'll see activity spread across multiple channels rather than one tall bar.

## 6. WiFi audit: is my home congested?

**Problem**: Considering an upgrade — is the issue my router, or the spectrum?

**Steps**:
1. Launch app on a typical evening (7–10 PM, peak congestion)
2. Run for 5 minutes (presses OK if MAX HOLD triggers, lets it keep accumulating)
3. Snapshot the result
4. Compare:
   - Few tall bars on WiFi 1/6/11 → spectrum OK, your router is the bottleneck
   - All channels packed → spectrum congested, consider 5 GHz
   - Tall bars between WiFi channels → non-WiFi interference (BLE, microwaves)

## 7. Wireless keyboard / mouse fingerprinting (defensive)

**Problem**: You suspect rogue 2.4 GHz HID devices in your office (security concern).

**Steps**:
1. Have everyone disconnect their wireless keyboards/mice
2. Scan baseline (should be quiet except for WiFi/BLE)
3. Have one person reconnect and type
4. Note which channels light up
5. Repeat for each device — build a fingerprint of "normal" vs anomalous

**Caveat**: This is reconnaissance only — the app doesn't decode keyboard data. For full defensive analysis, pair with [MouseJack](https://www.mousejack.com/) tools.

## 8. Find quiet windows for low-power IoT deployment

**Problem**: Deploying a Zigbee or Thread mesh; want to pick a least-noisy channel.

**Steps**:
1. Scan for 5+ minutes with high dwell (1000 µs+)
2. Identify channels with **consistently** low bars — not just brief quiet moments
3. For Zigbee: channels 11–26 in 802.15.4 numbering (≈ NRF24 ch 5–80, every 5 MHz)
4. Pick the quietest one for your network

## 9. RF education / classroom demo

**Problem**: Teaching students about RF spectrum, want a live visualization.

**Steps**:
1. Display Flipper screen via overhead camera or pixel-perfect emulator
2. Have a student turn on a WiFi hotspot — see channel light up
3. Boil water in a microwave — see ch 50 saturate
4. Pair an AirPod nearby — see BLE markers blip
5. Move cursor to read each frequency

This makes the invisible, visible — incredibly effective for teaching.

## 10. Lab benchtop spectrum check before measurements

**Problem**: Doing RF testing with proper equipment; want a quick sanity check before powering up.

**Steps**:
1. Quick scan to confirm the test area's baseline isn't already saturated
2. If you see unexpected bars, find them first (your equipment, your neighbor's, building HVAC controls?)
3. Once baseline is understood, power up your DUT
4. Compare with/without DUT to verify it's emitting only intended frequencies

This isn't certification-grade — but it's a 30-second sanity check before the proper lab setup.

---

## Have a use case to add?

[Open a PR](../../../pulls) with your scenario and a screenshot — we'd love to publish it here with credit.

---

🛒 **[Get the PINGEQUA 2-in-1 RF Devboard](https://www.pingequa.com/products/flipper-zero-nrf24-cc1101-2-in-1-rf-devboard?utm_source=github&utm_medium=docs_usecases&utm_campaign=rflab)** — the hardware that makes all this possible.
