# Post-MVP research (not implemented)

The following ideas deliberately remain outside the application-proxy MVP:

- CDC-ECM/NCM experiments and the firmware/descriptors macOS would require
- RNDIS only as a Windows compatibility investigation
- a small TCP/IP stack on Flipper and its RAM/flash budget
- concurrent requests and long-lived/WebSocket-style exchanges
- DNS as a public Flipper API rather than helper-internal resolution
- streaming small files directly to SD storage
- graphical Windows and Linux helper applications
- protocol-level application identity for per-FAP persistent permission
- signed/notarized macOS packaging and launch-at-login distribution

Any USB-network-device experiment must be a separate branch and threat model; it
must not weaken or complicate the MVP's explicit per-device consent boundary.
