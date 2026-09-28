<!-- SPDX-FileCopyrightText: Mistial Dev -->
<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# Contributing to tiny_crypto_c

Contributions are welcome under GPL-2.0-or-later.

Run the fast checks while developing:

```sh
make test
```

Before submitting a change, run:

```sh
make test-full
make test-sanitize
```

Keep the library suitable for small firmware:

- Do not allocate memory dynamically.
- Keep optional algorithms and modes behind compile-time switches.
- Check length arithmetic before reading or writing buffers.
- Compare authentication tags with `TC_ct_equal`.
- Wipe secret state with `TC_secure_zero` when `TC_ZEROIZE` is enabled.
- Add comments where a security contract, lifetime rule, or embedded tradeoff
  is easy to miss. Avoid comments that only repeat the code.
- Use the shared `TC_status` result model for fallible APIs.

CMake owns the build logic. The Makefile must remain a thin frontend that
forwards `TINY_CRYPTO_*` options.

Generated vectors belong under `tests/vectors/`; generators belong under
`tools/`. Preserve third-party copyright and license notices.
