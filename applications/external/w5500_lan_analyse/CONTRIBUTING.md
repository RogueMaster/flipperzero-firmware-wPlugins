# Contributing

Thank you for your interest in contributing to the Flipper Zero LAN Tester!

## Building

```bash
cd lan_tester
ufbt build        # build only
ufbt launch       # build and run on connected Flipper
```

Requires [ufbt](https://github.com/flipperdevices/flipperzero-ufbt).

## Reporting Security Issues

If you find a security vulnerability, please open a GitHub Issue with the `[SECURITY]` tag in the title. Describe the issue, affected code, and potential impact.

## Code Style

- **Language**: C99
- **Naming**: `snake_case` for functions and variables, `UPPER_CASE` for macros/constants
- **Indentation**: 4 spaces, no tabs
- **Braces**: K&R style (opening brace on same line)
- **Comments**: use `/* */` for block comments, `//` sparingly for inline notes

## Documentation Requirements

**Any PR that changes functionality or UI must also update:**

- `README.md` — if features table, usage guide, or architecture diagram are affected
- `CHANGELOG.md` — add an entry under the appropriate section (Added/Changed/Fixed/Security)
- `docs/en/README.md` and `docs/ru/README.md` — keep full documentation in sync with both languages

PRs without corresponding documentation updates will be asked to add them before merge.

## Pull Request Process

1. Fork the repository
2. Create a feature branch (`git checkout -b feat/my-feature`)
3. Make your changes
4. Ensure `ufbt build` completes without errors or warnings
5. Update documentation (see above)
6. Open a PR with a clear description of what and why

## Project Structure

- `hal/` — W5500 hardware abstraction
- `protocols/` — network protocol implementations
- `bridge/` — Ethernet bridge engine + PCAP
- `usb_eth/` — USB CDC-ECM driver
- `utils/` — shared utilities (OUI lookup, packet parsing)
- `lib/` — vendored WIZnet ioLibrary_Driver (do not modify)

## License

By contributing, you agree that your contributions will be licensed under the MIT License.
