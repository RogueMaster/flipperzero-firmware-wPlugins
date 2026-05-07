# Quickstart — 5 Minutes to Your First Scan

## What you need

1. A Flipper Zero on any modern firmware (Momentum / Unleashed / RogueMaster / OFW)
2. The [PINGEQUA Flipper Zero NRF24+CC1101 2-in-1 RF Devboard](https://www.pingequa.com/products/flipper-zero-nrf24-cc1101-2-in-1-rf-devboard?utm_source=github&utm_medium=docs_quickstart&utm_campaign=rflab) — the only officially supported hardware
3. A USB-C cable
4. A computer with [qFlipper](https://flipperzero.one/downloads) (or `ufbt` for command-line install)

## 1. Install the app

### Easiest: qFlipper drag-and-drop

1. Download `pingequa_rf_toolkit.fap` from the [Releases page](../../../releases)
2. Connect Flipper to your computer via USB
3. Open qFlipper, click File Manager
4. Navigate to `/ext/apps/GPIO/`
5. Drag `pingequa_rf_toolkit.fap` into that folder

### Power-user: ufbt sideload

```bash
pip install --user ufbt
ufbt update --channel=dev
git clone https://github.com/pingequa/pingequa_rf_toolkit
cd pingequa_rf_toolkit
ufbt launch                  # builds and launches automatically
```

## 2. Plug in the board

1. Power off your Flipper (hold Back ~2 seconds → Power off)
2. Plug the PINGEQUA 2-in-1 board onto Flipper's GPIO header (the row of pins on top)
3. Power Flipper back on

## 3. Launch the app

On Flipper:

1. From main menu: `Apps → GPIO → PINGEQUA RF Lab`
2. Splash screen appears showing "Initializing..." with a progress bar
3. After ~1 second, you should see the scanner UI with bars filling in

## 4. First scan walkthrough

The bars represent real-time 2.4 GHz activity. Wait 5–10 seconds for accumulation.

**Try this**:

1. Open WiFi on your phone, browse a website (generates traffic on your home AP)
2. Watch the scanner — bars near the WiFi 1 / 6 / 11 marker (▲) should grow
3. Press **→** several times to move the cursor across the chart
4. The header updates `Ch0XX  2XXX MHz` showing exact channel and frequency
5. The footer's `Cur:N` shows hits at the cursor channel
6. Continue until any bar reaches the top — top-right shows `MAX!`
7. Press **OK** to clear and rescan

## 5. Common first questions

> **Why don't I see any bars?**  
> Either no 2.4 GHz activity nearby (rare in cities), or the board isn't detecting. Check the [Troubleshooting guide](TROUBLESHOOTING.md).

> **Bars filled up too fast — everything is at top?**  
> Lower the dwell time with **↓**. Less dwell = less sensitivity = harder to saturate.

> **App froze / Flipper rebooted?**  
> File a [bug report](../../../issues/new?template=bug.yml) with firmware, hardware, and steps to reproduce.

## 6. Next steps

- Read the [UI Guide](UI_GUIDE.md) — every pixel explained
- Try the [Use Cases](USE_CASES.md) — 10 real scenarios
- Check the [FAQ](FAQ.md) — frequently asked questions

---

🛒 Don't have the board yet? **[Buy the PINGEQUA 2-in-1 RF Devboard](https://www.pingequa.com/products/flipper-zero-nrf24-cc1101-2-in-1-rf-devboard?utm_source=github&utm_medium=docs_quickstart&utm_campaign=rflab&utm_content=footer)**
