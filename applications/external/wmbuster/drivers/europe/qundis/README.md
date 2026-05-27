# Qundis / ista / Brunata

These three vendors share the same 12-byte HCA preamble (control
byte, status, current/previous reading u16, two M-bus type-G
dates, optional room/radiator temperatures). `hca_compact.c`
implements the shared decoder and registers three driver entries
(one per brand) so the user sees the right title in the meter
list.

`qundis_water_heat.c` covers the OMS-conformant Q-Water v2,
Q-Heat v2 and Q-Smoke families — those use the generic walker.
