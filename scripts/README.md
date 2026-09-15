# FIBP hardware-free simulator

`fibp_simulator.py` creates a raw pseudo-terminal, prints its slave path on
stdout, and emulates one FIBP endpoint. It does not make a real network request.
All diagnostics are written to stderr.

Run a simulated Flipper for a desktop helper that accepts a manual serial path:

```sh
python3 scripts/fibp_simulator.py \
  --role flipper \
  --fragment-sizes 1,3,7,64
```

Run a simulated Mac peer for a Flipper-side protocol client:

```sh
python3 scripts/fibp_simulator.py \
  --role mac \
  --fragment-sizes 2,5,64
```

The first stdout line is a path such as `/dev/ttys003`. Open that path in the
component under test using raw serial settings.

Corrupt the first outgoing PING frame's trailer CRC:

```sh
python3 scripts/fibp_simulator.py \
  --role flipper \
  --corrupt-crc frame \
  --corrupt-message PING \
  --fragment-sizes 1,2,3
```

Run the hardware-free test suite:

```sh
python3 -m unittest -v tests.test_fibp_codec tests.test_fibp_simulator
```
