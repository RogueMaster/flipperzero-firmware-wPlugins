## 0.4.0 - Signal storm (WiFi devboard)
 - Optional ESP32-S2 WiFi devboard support: when attached over the GPIO
   expansion UART and running the reporter firmware (see `esp32/`), Forage also
   senses nearby WiFi access-point density. A busy area triggers a "signal
   storm" that boosts catch tiers and egg odds on top of the sub-GHz reading.
 - Auto-sensing: the game listens ~1.5 s for a `DRAGO wifi=N rssi=X` line during
   each Forage. Heard -> storm; not heard -> plays exactly as before. The board
   is pure enrichment; nothing is required to attach it.
 - Dedicated animated **Signal Storm screen**: a sweeping radar that pings each
   nearby network as a blip, live AP count + RSSI, and the catch result. Plain
   sub-GHz catches keep the small "RF"-tagged bottom banner, so the two hunt
   modes look completely different.
 - A storm adds three things beyond better odds: **exclusive "storm eggs"**
   (a board-only collectible, tracked in Inventory, that can also hatch heirs
   with the best care head-start), **WiFi-themed flavour** (signal-wisp /
   cyber-wyrm / data-drake / sky-serpent, "Data cache" treasure), and a
   **guaranteed floor** (8+ APs never yields a small-prey dud). Catching a storm
   egg sets off a sparkle celebration on the radar screen.
 - Forage cooldown shortened to 90 s and now visible: the menu shows
   "Hunt (Ns)" while cooling down, and a too-soon Hunt says "Hunt ready in Ns".
 - Version shown in the menu header. Save format bumped (0xD9 -> 0xDA) for the
   storm-egg counter.
 - ESP firmware lives in `esp32/` (MicroPython + `main.py`), with a flash script
   and uploader. The ESP32-S2 is WiFi-only (no BLE), so density = WiFi APs. The
   reporter transmits the cached scan continuously (~5 Hz) while rescanning on a
   slow timer, so the Flipper reliably catches a report during its listen window.

## 0.3.1 - Real airwaves
 - Hunting now reads the REAL sub-GHz airwaves: Forage sweeps 315/433/868/915 MHz,
   measures RSSI (receive-only), and busier surroundings yield better/rarer
   catches. Where you physically are now changes your luck.
 - Fixes the earlier radio crash: ignore subghz begin()'s false return and give
   the logic thread a 4KB stack (a 1KB stack overflowed during the sweep).

## 0.3.0 - Expeditions
 - Send the dragon on a timed offline expedition: Short 30m / Long 2h / Epic 8h
   (Menu -> Expedition). Needs pause while it's away (it can't starve at home).
 - Main screen shows an "On Expedition" state with a live countdown; only Stats/
   Inventory/Settings are reachable until it returns (no early recall).
 - On return it resolves loot (prey/treasure/eggs, scaled to the trip) and shows
   a journey log; longer trips give more but risk coming back hurt (never fatal).

## 0.2.2 - Inventory
 - New Inventory screen (Menu -> Inventory): treasure broken down by tier
   (small/med/large) + total hoard value, eggs by rarity, and lifetime catches
   (prey / eggs / total). Now tracks per-tier treasure and lifetime catch counts.

## 0.2.1 - UI/UX overhaul
 - Calm main screen: pet + heart meters + a clear "OK: Menu" hint (the confusing
   one-at-a-time action cycler is gone).
 - Proper scrollable Menu (OK): Care / Hunt / Stats / Settings, plus Hatch Heir
   when relevant. Care is its own submenu (Feed/Play/Clean/Medicine/Scold/Lights).
 - Visual Stats screen (name/age, heart meters, care, rank, hoard, discipline, eggs).
 - Catch/action reveal moved to a bottom banner so it no longer covers the meters.

## 0.2.0 - Hunt
 - Forage: a one-tap hunt yields a surprise catch - prey (feeds it), treasure
   (builds a hoard), or a rare egg (hatchery) - with a cooldown + catch banner.
 - Hoard rank/title and egg counts on the Stats screen.
 - Legacy: when your dragon dies, hatch an heir egg to continue with a care
   head-start (hoard persists across lineage).
 - Note: catch luck currently uses a safe internal source; true sub-GHz
   "airwave" sensing is a separate upcoming spike.

## 0.1.2
 - Heart meters: Hunger / Joy / Health now show as rows of 4 hearts.
 - Dedicated eating (open-mouth chomp) and sleeping (eyes-closed + Zzz) sprites,
   plus a poop glyph on screen and state motion/overlays.
 - Night is now 20:00-08:00 (was 22:00).

## 0.1.1
 - Fix: pet could die within hours / overnight. Two causes fixed:
   - Uncleaned poop no longer drains health directly (it is a sickness risk,
     via higher illness odds, not an instant bleed).
   - The pet now truly sleeps at night: hunger, happiness, health drain, and
     pooping all pause while it sleeps (22:00-08:00), so overnight is safe.
     Lights-out is a care bonus; lights-on a mild penalty, never lethal.

## 0.1 (Dragotchi, fork of MrModd's Matagotchi)
 - Dragon theme with care-driven branching evolution:
   Egg -> Hatchling -> Wyrmling -> Drake -> Adult (White / Grey / Black by care).
 - New needs & interactions: Hunger, Happiness, Health, Hygiene (poop),
   Sickness, Sleep (day/night), Discipline. Actions: Feed, Play, Clean,
   Medicine, Scold, Lights.
 - Care score drives which adult you get; impeccable care (>=90) makes an adult
   immortal (revocable). Neglect kills.
 - Deepened offline simulation (time passes while closed) + an attention alarm
   when you reopen to a pet in trouble, and live attention/discipline calls.
 - Custom main view with meters + animated dragon + action selector.
 - Hand-drawn 1-bit dragon sprites (white=well raised, black=neglected).
 - New save format (fresh start; not compatible with Matagotchi saves).
