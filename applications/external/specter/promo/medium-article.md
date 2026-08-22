# One bit, 500 times a second

## What it actually took to make a Flipper Zero hear NFC readers

A card skimmer hidden inside a payment terminal has a problem: to steal from a card, it has to be powered on and waiting. And anything waiting for an NFC card is shouting.

Every powered-on 13.56 MHz reader continuously pings the air with a carrier, asking "is there a card here yet?" It does this hundreds of times a minute, forever, whether or not anyone is standing at the terminal. That constant shouting is the thing you can find.

I wrote an app called **Specter** to find it with a Flipper Zero. It's free, MIT, and needs no extra hardware. This is the story of the parts that were harder than they looked — including one bug that could freeze your Flipper and had been in there since the first release.

---

## The mechanism: one bit

The Flipper's NFC chip is an ST25R3916. Buried in it is a hardware **external field detector** — a single bit that answers "is someone else's carrier present right now?" It exists so the Flipper can emulate a card and know when a reader is talking to it.

Specter parks the chip in detect-only mode and samples that bit about 500 times a second. It never switches on its own transmitter. Not once. It is a microphone, not a speaker — which matters both legally and ethically when the thing you're pointing it at is someone else's payment terminal.

One bit doesn't sound like much. But sampled fast enough, it carries two completely independent signals:

**How much** — what fraction of the time a carrier is up. That behaves like proximity.

**What rhythm** — the timing of the on/off edges. That fingerprints *what kind* of thing is emitting.

Everything in the app is built on those two.

---

## The 31% problem

The first version showed the duty-cycle directly as a "FIELD %" meter. Needle on a dial, percentage next to it. Simple.

Then someone laid their Flipper directly on top of a reader and it read **31%**.

Not 100%. Not even close. And moving nearer changed nothing.

My first instinct was that detection was broken. It wasn't. Readers **poll**: a short burst, a sleep, another burst. A typical access reader or payment terminal is only actually radiating **20–35% of the time**. So with the Flipper resting on it, 31% wasn't a weak reading — **31% was saturation.** That was the ceiling, and no amount of getting closer could raise it, because the reader itself was off for the other 69%.

I had been taking a perfect detection and drawing it as a third of one.

Worse, it had quietly broken two other things. The proximity words `CLOSE` and `STRONG` triggered at 45% and 70% — on a scale that physically stopped at ~31%, they were **unreachable**. The app could only ever say `FAINT` or `NEAR`. And the site-survey verdict had a rule saying "peak above 50% means definitely a reader," which for a polling reader — the exact device the app exists to find — **could never fire.**

One wrong assumption, three user-visible bugs, none of which looked related.

The fix was to map the real polling band onto the full dial. Raw duty of 30% and up now reads 100%. Room noise at 3% still reads a quiet 10%. The floor didn't move; only the ceiling did.

But the raw number never gets thrown away. The noise floor, the auto-calibration, and the fingerprinting all still work in true duty-cycle, because those describe **the signal**, not your distance from it. Only what you *look at* is scaled.

And when the meter is pegged, it says `MAX` rather than sitting silently at 100. A needle that stops moving should tell you it stopped, not leave you wondering if it broke.

---

## The bug I'd rather not have shipped

A user called **drdelaney** opened an issue with a description I wish every bug report looked like:

> "It seems it triggers if you hit any other key in the middle of the noise floor scan. It locks everything out."

Every button dead. Screen still animating. Only a full reboot brought it back.

I spent a while looking at input handling, because that's where a dead button *should* live. It was fine. The views weren't swallowing keys, the scene manager wasn't consuming the back event. The bug was somewhere else entirely.

Here's the line. It's the pause between samples:

```c
furi_delay_us(2000);   // wait ~2 ms, then sample again
```

That reads like "sleep for 2 ms." It isn't. The firmware documents it precisely:

> *Implemented using Cortex DWT counter. Blocking and non aliased.*

It's a **busy-wait**. It spins on a cycle counter and never hands the CPU back to the scheduler.

So for the entire duration of every scan, that thread sat at 100% CPU, at normal priority, hammering the internal SPI bus — the same bus the display and the SD card share. Everything else on the system had to fight it for time. The UI's input queue stopped draining fast enough. And once that queue filled, the GUI thread blocked trying to post into it — taking the whole interface down with it.

That's why pressing keys *during* the scan was the reliable trigger: that's when the most input was queued, and when a settings write to the SD card was landing on the same congested bus.

The fix is three characters different:

```c
furi_delay_tick(sample_ticks);   // actually yields
```

Plus dropping the sampler below the UI in thread priority. Sampling a couple of milliseconds late is invisible. An unresponsive Flipper is not.

**The lesson I'd hand to anyone writing embedded firmware:** a function named `delay` is not necessarily a function that *sleeps*. Check whether your wait yields. Mine hadn't, in every release for a month, and it took a stranger with a good bug report to find it.

---

## Testing the parts that make claims

There's a category of code in this app that worried me more than the rest: the bits that turn a number into an **assertion**.

"This is a polling reader." "This room is clean." "You're as close as the meter can resolve."

Those are claims a person might act on, and they're exactly the code you cannot meaningfully verify by staring at a 128×64 screen with a card in your hand.

So they're all pure C — no hardware dependencies, no firmware headers — sitting in their own files with a plain `make` that compiles and runs them on a laptop. **316 checks**, covering things like:

- a reader polling every 200 ms must never read as absent
- ten seconds on one reader is *one* contact, not forty
- the presence latch must survive the tick counter wrapping past 2³²
- a room with nothing in it is the only route to a `CLEAN` verdict

CI runs them before it will build the firmware. Several real bugs died in that test file before they ever reached a device.

The corollary is that the *impure* parts — the drawing code, the radio worker — are where every shipped bug has come from. That's not a coincidence, and it's a reasonable argument for pushing as much logic as you can into the part you can test on a real computer.

---

## Designing for what it can't do

The most important sentence in the README isn't about a feature:

> **`CLEAN` means clean at the sensitivity you chose.**

A dormant skimmer that only wakes on a real tap is invisible to this tool. So is anything shielded. So is every 125 kHz reader on earth, because the Flipper's low-frequency path has no equivalent detect bit.

The app now carries that honesty in the interface, not just the documentation:

- Timings measured near the 2 ms sampling floor are shown with a `~` and their confidence is **discounted** — the tool would rather flag its own resolution limit than quote a precise-looking number it can't stand behind.
- The survey returns `CLEAN` / `TRACE` / `ACTIVE` rather than a letter grade, because a grade implies a calibrated scale for how compromised a room is, and no such scale exists here.
- Fingerprinting shows the raw carrier waveform underneath its verdict, so the conclusion never has to be taken on faith.

A detector that overstates itself is worse than no detector, because someone will trust it in a car park at 11pm.

---

## What it is now

Five modes, each answering a different question:

| Mode | Question |
|---|---|
| **Sweep** | Where is it? |
| **Fingerprint** | What kind of thing is it? |
| **Site Survey** | Is this room clean? |
| **Watch** | Did one appear while I was away? |
| **Logbook** | What did I find, and when? |

It's MIT licensed, needs no devboard, and runs on a stock Flipper Zero:

**github.com/at0m-b0mb/Specter-FlipperZero**

Use it on your own equipment, or on hardware you're explicitly authorised to assess. It's a listen-only, defensive tool — but that's a statement about the software, not a legal opinion about wherever you're standing.

---

*If you find a bug, open an issue. The best one so far took a fortnight to reach me and turned out to have been broken since day one.*
