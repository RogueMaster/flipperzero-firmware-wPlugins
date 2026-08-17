# Passive Listening

`Training → Passive listening` is the hands-off trainer. It sends a callsign or lesson group in Morse, leaves a pause for head copy, then speaks the answer character by character. It can send the Morse once more afterwards. This follows the useful shape of [Morse Code Ninja practice](https://morsecode.ninja/practice/): hear it, commit to an answer, then find out what the bloody thing actually was.

The spoken answer is real sampled voice, and it does play through the Flipper's built-in buzzer; an external audio circuit is not required. If the global audio path is `P2 (HD)`, both Morse and voice go to `P2 / PA7` instead for cleaner external audio.

In `Callsign` mode it uses the same [callsign generator](501-callsign-generator.md), rather than a short list which eventually becomes familiar. `Lesson` mode draws from the selected character lesson. Either way, the screen reveals the answer as it is spoken, though looking at it rather defeats the hands-free premise.

This is practice for dog walking, driving, washing dishes, or doing housework which occupies the hands while leaving the language-processing bit of the brain idle. Keep driving sessions genuinely hands-off and subordinate to the road; Morse is useful, but not sufficiently useful to explain a dented car.
