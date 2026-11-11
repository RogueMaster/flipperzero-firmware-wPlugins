# Protocol references and attribution

A/C Master is developed for Flipper Zero and distributed under the MIT License (see [LICENSE](../LICENSE)). Its application code uses Flipper's public infrared APIs.

Protocol timing constants, frame layouts, checksums and manufacturer-specific command behaviour were researched using public documentation and open-source references, including:

- [IRremoteESP8266](https://github.com/crankyoldgit/IRremoteESP8266) by its contributors, particularly the LG and Gree protocol implementations. That project is licensed under LGPL-2.1.
- Publicly documented Mitsubishi Electric, Mitsubishi Heavy Industries and Fujitsu infrared protocol descriptions and sample captures.

These references were used to inform the Flipper-specific protocol implementations. A/C Master does not include or link the IRremoteESP8266 library. The source includes reimplementations of protocol handling in C for Flipper's API.

**Licence review note:** Public protocol characteristics (such as timing values and frame fields) and copyrighted implementation code are different things. If any implementation code has been adapted from an LGPL-licensed source, additional attribution, notices and/or licence obligations may apply. Contributions should preserve upstream authorship and licence notices where required. This record is intended to provide transparency about reference material; it does not, by itself, establish licence compatibility.

The protocol compatibility notes in the README remain important: support varies by device model, and some paths have received only limited hardware testing.
