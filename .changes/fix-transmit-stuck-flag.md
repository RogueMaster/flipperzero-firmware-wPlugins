---
kind: fixed
pr: null
---
Only set s->transmitting after pulses are successfully generated

## Release Note
Fixed the transmit screen occasionally getting stuck on "Transmitting..."

This could happen when a sync group's combined signal was too large to send.
