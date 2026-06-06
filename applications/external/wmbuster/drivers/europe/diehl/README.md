# Diehl Metering

Hydrus, Sharky and Aerius all ship OMS-conformant payloads;
their driver entries are pure descriptors. **IZAR** uses a
custom PRIOS stream cipher and produces garbage when run through
the OMS walker — `diehl.c` registers an explicit IZAR driver
that outputs a `Decoder TODO` message + labelled hex dump until
somebody ports `_refs/wmbusmeters/src/driver_izar.cc` (322 lines).
