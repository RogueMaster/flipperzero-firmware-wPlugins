# Kamstrup

MULTICAL, flowIQ and OmniPower telegrams are OMS-conformant once
the link header is stripped, so the generic walker handles them.
The MULTICAL 21 short telegram (CI 0x78/0x79) wraps two pre-set
fields in a compact frame and would benefit from a dedicated
decoder — port `_refs/wmbusmeters/src/driver_multical21.cc` if
you need MULTICAL-21 details.
