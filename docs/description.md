# Samsung AC Remote

Infrared remote for Samsung air conditioners. Every button press sends the full Samsung A/C state frame: power, mode, temperature, fan and swing.

## Controls

- Power: on / off
- Mode: cool, heat, dry, fan, auto
- Temp: 16 to 30 °C (locked in fan mode)
- Fan: auto, low, med, high
- Swing: vertical swing on / off

Samsung A/C is stateful: there are no one-shot command codes, so the whole current state is retransmitted on every press. Settings persist between launches.

The protocol implementation (timings, byte-field layout, the dual-section checksum) is based on the reverse engineering in the [IRremoteESP8266](https://github.com/crankyoldgit/IRremoteESP8266) project.
