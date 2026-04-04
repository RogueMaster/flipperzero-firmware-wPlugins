# r/flipperzero post

**Title:** Tesla FSD unlock app for Flipper Zero — open source, just released

**Body:**

Built an app that unlocks Tesla FSD through the Flipper Zero + CAN Bus Add-On.

Plug into the OBD-II port, open the app, it auto-detects your hardware (HW3 or HW4) and starts modifying CAN frames to enable FSD. Also kills the nag and sets speed profile to max.

No computer needed, no subscription, no MITM — single bus read-modify-retransmit.

**What you need:**
- Flipper Zero
- Electronic Cats CAN Bus Add-On (~$30)
- OBD-II cable to your Tesla

**Supports:** Model 3/Y (HW3 and HW4), Model S/X 2021+

Code: [github link]

Built this because the existing CanFeather solution was being sold for 500 EUR which is absurd for 200 lines of bit flipping. This does the same thing on hardware most of us already own.

---

# r/TeslaModel3 post

**Title:** Open-source FSD unlock using Flipper Zero — no subscription needed

**Body:**

Released an open-source tool that enables FSD on Model 3/Y via CAN bus. Runs on a Flipper Zero with a $30 CAN add-on board.

How it works: plugs into OBD-II, listens for autopilot config frames, flips the FSD enable bits, retransmits. Auto-detects HW3 vs HW4.

It's not permanent — unplug and it reverts to stock. Only touches UI config frames, nothing safety-critical.

Repo: [github link]

Figured I'd share since the commercial versions of this charge 500+ for what amounts to a $20 microcontroller and some bit manipulation.

---

# r/TeslaHacking post (if it exists) / r/CarHacking

**Title:** Flipper Zero app for Tesla FSD CAN bus unlock — HW3/HW4 auto-detect

**Body:**

Just published an open-source Flipper Zero FAP app that enables FSD on Tesla vehicles via CAN bus injection.

Technical details:
- Monitors `UI_autopilotControl` (0x3FD) on Party CAN (Bus 0)
- HW3: sets bit46 (FSD enable), clears bit19 (nag)
- HW4/FSD V14: sets bit46+bit60, clears bit19, sets bit47
- Speed profile from follow-distance stalk (0x3F8)
- Auto-detects HW version from `GTW_carConfig` (0x398) `GTW_dasHw` field

Single bus, no MITM. Read-modify-retransmit through MCP2515 on Flipper GPIO.

Hardware: Flipper + Electronic Cats CAN Add-On + OBD-II cable.

Signal database cross-referenced with commaai/opendbc Tesla DBC files.

[github link]
