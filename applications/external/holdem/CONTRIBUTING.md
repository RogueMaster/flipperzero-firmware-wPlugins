# Contributing to Hold 'em

Thanks for contributing.

This project targets Flipper Zero runtime constraints, so correctness and simplicity matter more than complexity.

## Ways to Contribute

- Report bugs with reproduction steps
- Improve gameplay UX and readability
- Submit performance or memory optimizations
- Add tests and validation tooling
- Propose roadmap items with clear requirements

## Development Setup

1. Install `ufbt` and toolchain.
2. Use the most recent active feature branch if you want to run or contribute to upcoming release work. `main` is kept as the stable public release line, and working from the latest feature branch helps minimize conflicts.
3. Build from repo root:
   - `ufbt update`
   - `ufbt`
4. Artifact output:
   - `dist/holdem.fap`

## Coding Expectations

- Keep code portable within Flipper external-app APIs.
- Prefer descriptive names over short/cryptic ones.
- Add comments where logic is non-obvious.
- Keep hot paths efficient (UI draw, hand loop, AI decision flow).
- Avoid speculative abstraction unless it reduces maintenance cost now.
- Keep modules purpose-specific and contributor-friendly.
- Prefer extending the split app structure (`holdem_ui_common`, `holdem_ui_render`, `holdem_ui_flow`, `holdem_gameplay`) over pushing everything back into one large file.

## PR Guidelines

Before opening a PR:

- Build succeeds locally with `ufbt`
- No regressions in menu controls and back-button behavior
- Save/load still works for both startup choices (load vs new)
- Hand progression works across all streets and showdown
- README/docs updated if behavior changed

PR description should include:

- What changed
- Why it changed
- How it was validated
- Any known limitations

## Bug Reports

Please include:

- Firmware flavor and version
- Exact input sequence to reproduce
- Expected vs actual behavior
- Whether issue occurs on fresh install or with existing save

## Release Discipline

For each release cycle:

- Keep `docs/changelog.md` current with concise user-facing notes
- Keep `.catalog/CHANGELOG.md` aligned with the latest public release notes used for catalog submission
- Put major future work in `docs/roadmap.md`
- Keep private deployment/submission notes out of this repo
