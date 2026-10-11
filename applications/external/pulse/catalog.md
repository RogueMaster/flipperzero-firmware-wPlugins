# Pulse BPM

A tap-along heart-rate counter for coaches and clinicians. Feel the pulse at the wrist (or neck), press **OK** on every beat, and read a live BPM that **locks after 20 seconds**.

BPM = (beats - 1) x 60 / elapsed_seconds

The first tap starts the window. Each later tap updates the average. Taps closer than 250 ms are ignored (debounce / double-press). When 20 seconds have elapsed from the first beat, the value freezes.

## How to use

1. Splash, then the ready screen.
2. Find the pulse. Press **OK** on the first beat - the timer starts.
3. Keep tapping **OK** on each beat. The large number is the running BPM; the bar fills toward 20 s.
4. At 20 s the result **locks**. **OK** starts over, **Back** / **Left** exits.

This is a timing aid, **not a medical device**. It does not replace a pulse oximeter or ECG. Reaction time and a missed beat affect the reading.

## Install

Copy dist/pulse_bpm.fap to the microSD card under apps/Tools/. On the Flipper: **Apps → Tools → Pulse BPM**.

Build with ufbt. Firmware must match the API printed by APPCHK.
