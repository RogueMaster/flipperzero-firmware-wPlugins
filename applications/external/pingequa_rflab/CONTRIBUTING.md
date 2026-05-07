# Contributing to PINGEQUA RF Lab

Thanks for your interest in contributing. This project is built for the [PINGEQUA 2-in-1 RF Devboard](https://www.pingequa.com/products/flipper-zero-nrf24-cc1101-2-in-1-rf-devboard?utm_source=github&utm_medium=contributing&utm_campaign=rflab) — having that hardware on hand makes contributing dramatically easier.

## Quick start

```bash
git clone https://github.com/pingequa/pingequa_rf_toolkit
cd pingequa_rf_toolkit
pip install --user ufbt
ufbt update --channel=dev
ufbt              # build
ufbt launch       # build + sideload + run on connected Flipper
```

See [docs/BUILD.md](docs/BUILD.md) for full setup.

## What we're looking for

✅ **Welcome contributions:**
- Bug fixes (with reproduction in the issue)
- New use case demos and screenshots for `docs/USE_CASES.md`
- Translations of the README (Spanish, French, German, etc.)
- Hardware compatibility reports via [the hardware issue template](.github/ISSUE_TEMPLATE/hardware.yml)
- Documentation clarifications

🤔 **Discuss first** (open an issue before PR):
- New scenes or major UI changes
- Major architectural changes
- Removing existing features

❌ **Out of scope:**
- Forks targeting other hardware as the primary supported device (please fork the repo and rename — PINGEQUA® is a trademark)
- Active RF emission features without clear opt-in flag and legal review

## Code style

- C99, follows existing patterns in `core/`, `scenes/`, `views/`
- 4-space indentation
- Comments in English
- Pull requests should preserve existing module boundaries — don't refactor unrelated areas in a feature PR

## Pull request process

1. Fork the repo and create a branch off `main`
2. Make your changes with clear commit messages
3. Make sure `ufbt` build passes
4. Update `CHANGELOG.md` under `[Unreleased]` if user-facing
5. Submit the PR using [our template](.github/PULL_REQUEST_TEMPLATE.md)
6. Be prepared to iterate on review comments

## Testing

This project doesn't have host-side unit tests — testing happens on real hardware. Please describe in your PR:

- What firmware you tested on (Momentum / Unleashed / etc.)
- What hardware you used
- Specific scenarios verified
- Any regressions you checked against (e.g. "Sub-GHz Read still works after my changes")

## Communication

- **Issues**: bug reports, feature requests, hardware compatibility
- **Discussions**: design questions, ideas not yet ready for an issue
- **Email**: [support@pingequa.com](mailto:support@pingequa.com) for non-public concerns

## Reviewer expectations

We aim to acknowledge issues within 7 days and review PRs within 14 days. Complex PRs take longer. If you don't hear back, ping the issue.

## License

By contributing, you agree your work will be released under the [MIT License](LICENSE).
