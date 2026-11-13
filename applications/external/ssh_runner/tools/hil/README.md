# HIL Harness

[← Documentation index](../../docs/README.md)

Scripts that stand in for the Flipper and drive the ESP32 over its USB console:

- `keys_matrix.py`: SSH keys staged exactly as the Flipper does it, against the test sshd.
- `sec_matrix.py`: Protect Secrets, on the virtual-eFuse test build only.

Both need the board; the docstring at the top of each script says how to run it.

## SSH Test Target

A `linuxserver/openssh-server` container, run with `--rm`, serves as the test sshd.
Test credentials live **only** in `tools/hil/target/local.env`, which is git-ignored.
Copy `target/local.env.example` and fill it in. Never commit real values.
