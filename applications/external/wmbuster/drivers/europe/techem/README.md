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

- The Vario heat decoder converts MJ to kWh with integer math
  (raw × 5 / 18, i.e. raw / 3.6), rounded to the nearest kWh.
- Smoke alarm bit semantics are not public; we surface the
  status byte verbatim so a non-zero value flags an event.
