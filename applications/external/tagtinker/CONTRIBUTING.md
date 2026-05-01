# Contributing to TagTinker

Thanks for wanting to help out! Here's how to get involved.

## Getting Started

1. Fork the repo
2. Create a feature branch: `git checkout -b my-feature`
3. Make your changes
4. Test on hardware if possible (or at minimum, ensure `ufbt build` passes)
5. Commit with a clear message
6. Open a pull request

## Code Style

- C99, no C++ features
- 4-space indentation (no tabs)
- `snake_case` for functions and variables
- Keep functions short and focused

## What We're Looking For

- Bug fixes (especially memory-related — the Flipper has very limited heap)
- Support for new ESL tag sizes and models
- NFC tag support for additional ESL formats
- UI/UX improvements
- Android companion app improvements

## Pull Request Guidelines

- Use **"Create a merge commit"** when merging to preserve contributor attribution.
- Keep commits focused — one feature or fix per PR.

## Reporting Issues

Please open a GitHub issue with:
- What you expected to happen
- What actually happened
- Your Flipper firmware version
- The ESL model you're testing with (if applicable)

## License

By contributing, you agree that your contributions will be licensed under GPL-3.0.
