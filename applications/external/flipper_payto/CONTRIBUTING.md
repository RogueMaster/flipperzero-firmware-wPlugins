# Contributing

Thanks for contributing to PayTo Wallets for Flipper Zero.

## Development setup

Install the Flipper external-app build tool, [ufbt](https://github.com/flipperdevices/flipperzero-ufbt), and ensure a compatible Flipper firmware SDK is available.

Run the host-side parser tests from the repository root:

```sh
make test
```

Build the FAP locally with:

```sh
ufbt faps
```

## Code guidelines

- Keep NFC, storage, and UI work scoped to the app lifecycle.
- Preserve strict PayTo and blockchain address validation.
- Add or update tests for parser and validation changes.
- Use tabs for indentation in C source where practical.
- Keep user-facing labels short enough for the Flipper display.

## Issues and pull requests

Use the provided issue templates for bug reports and feature requests. Blank issues are disabled; include reproduction steps or a concrete use case.

Pull requests should explain the behavior change and include the relevant test results. For NFC changes, describe the NTAG/Mifare Ultralight tag type and NDEF payload used for testing.

## Releases

The release workflow builds and attaches the FAP when a GitHub release is published and is not marked as a prerelease. The regular build workflow also runs on pushes to `main` and can be started manually. Both workflows use the official ufbt action with its `app-dir` input.

## License

Contributions are made under the [CORE License](LICENSE).
