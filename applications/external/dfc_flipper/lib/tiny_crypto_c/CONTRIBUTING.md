<!-- SPDX-FileCopyrightText: Mistial Dev -->

<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# Contributing to tiny_crypto_c

Contributions are welcome. The project is distributed under GPL-2.0-or-later
with the ESP-IDF linking exception at the top of [LICENSE](LICENSE).

## Contributor agreement

tiny_crypto_c adopts the
[Mistial Developer Contributor Copyright Assignment, version 1.0](https://gist.github.com/mistial-dev/d3697027c54c3c8c3545a580287a6912).
Every contributor must accept it before a contribution can be merged.

Accept the agreement through [CLA Assistant](https://cla-assistant.io/mistial-dev/tiny_crypto_c),
the service provided by SAP. When you open your first pull request, the CLA
Assistant check links to the agreement and records your acceptance. The check
must pass before the pull request is merged. One acceptance covers later pull
requests under the same agreement version.

- If an employer or other organization owns your contribution, an authorized
  signer must accept the agreement for that organization.
- Identify third-party material and its license in the pull request.
- Identify any substantial part of the contribution that an AI tool generated.
- To contribute outside GitHub, email
  [opensource@mistial.dev](mailto:opensource@mistial.dev) before sending the
  change.

## Development checks

Install the formatting hook once in each clone:

```sh
python3 -m pip install pre-commit==4.6.2
pre-commit install
```

The hook formats staged first-party C and C++ files with clang-format 22.1.8,
formats Markdown with mdformat 1.0.0 and its GitHub Flavored Markdown plugin,
and fixes trailing whitespace and final newlines in project text files. Stage
its edits and commit again. To format C or C++ files manually, use the same
version:

```sh
python3 -m pip install clang-format==22.1.8
clang-format -i path/to/file.c
```

To check all files without changing them, run the same check as CI:

```sh
pre-commit run --all-files --hook-stage manual
```

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
- Wipe secret state with `TC_secure_zero` on every return path.
- Add comments where a security contract, lifetime rule, or embedded tradeoff
  is easy to miss. Avoid comments that only repeat the code.
- Use the shared `TC_result` result model for fallible APIs.

CMake owns the build logic. The Makefile must remain a thin frontend that
forwards `TINY_CRYPTO_*` options. A new feature gets a `config.h` macro with
its default and dependency rules, and one entry in
[`cmake/features.json`](cmake/features.json) that names the macro and the
sources it compiles. The CMake option follows from the macro name.

Generated vectors belong under `tests/vectors/`; generators belong under
`tools/`. Preserve third-party copyright and license notices.
