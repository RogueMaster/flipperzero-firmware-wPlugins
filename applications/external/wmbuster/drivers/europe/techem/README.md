# Techem

Techem ships compact-frame telegrams that are **not** OMS-conformant —
the body uses fixed offsets and a Techem-private date packing. All
custom decoders live in `_compact.{h,c}`; each driver file in this
folder is a thin descriptor that wires an `(M,V,T)` list to the
right decoder.

## Sources

| Driver | wmbusmeters reference |
|---|---|
| `fhkv_hca.c`       | `src/driver_fhkvdataiii.cc`, `drivers/src/fhkvdataiv.xmq` |
| `mkradio_water.c`  | `src/driver_mkradio3.cc`, `src/driver_mkradio4.cc`, `drivers/src/mkradio4a.xmq` |
| `compact5_heat.c`  | `src/driver_compact5.cc` |
| `vario_heat.c`     | `src/driver_vario451.cc`, `drivers/src/vario451mid.xmq` |
| `smoke.c`          | (private protocol — status bytes only) |

## Known limitations

- The Vario heat decoder uses an integer GJ→kWh approximation
  (raw × 250 / 9). The largest possible 16-bit raw value rounds
  to within ±0.06 kWh of the IEEE-double calculation.
- Smoke alarm bit semantics are not public; we surface the
  status byte verbatim so a non-zero value flags an event.
