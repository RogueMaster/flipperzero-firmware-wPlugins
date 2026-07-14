# Contributing to PocketLab

Thanks for your interest in improving PocketLab.

## Development setup

```sh
pipx install ufbt            # or: pip3 install --user ufbt
ufbt                         # build
ufbt launch                  # build + run on a connected Flipper
ufbt lint                    # check formatting
ufbt format                  # auto-format
```

The app targets the **official** Flipper firmware SDK so it stays eligible for
the Flipper Application Catalog. Avoid APIs that only exist on custom firmware.

## Code style

- Run `ufbt format` before every commit; CI-style review expects the canonical
  Flipper `.clang-format` output.
- Keep functions small and named `module_verb_noun`. Types use `PascalCase`,
  everything else `snake_case`.
- No dead code, no commented-out blocks. Comments explain *why*, not *what*.

## Adding content

Most contributions are new labs, which are pure data in
`helpers/pocketlab_content.c`. A lab is an array of steps plus a `PocketLabLab`
entry. Provide both English and Russian strings, and keep lines short enough to
fit the 128 px screen.

## Pull requests

- One logical change per PR.
- Update `CHANGELOG.md`.
- Describe what you tested on real hardware.
