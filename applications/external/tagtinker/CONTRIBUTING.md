# Contributing to TagTinker

Thanks for wanting to help out. Here's how to get involved.

## Getting started

1. Fork the repo
2. Create a feature branch: `git checkout -b my-feature`
3. Make your changes
4. Test on owned lab hardware if possible (or at minimum, ensure `ufbt build` passes)
5. Commit with a clear message
6. Open a pull request

## Code style

- C99, no C++ features
- 4-space indentation (no tabs)
- `snake_case` for functions and variables
- Keep functions short and focused
- Comment non-obvious logic, but don't over-comment

## What we're looking for

- Bug fixes (especially memory-related — the Flipper has very limited heap)
- New ESL tag compatibility (different sizes and display variants)
- Better compression or transmission reliability
- UI/UX improvements
- Documentation improvements

## Project boundaries

- Keep contributions aligned with educational use on ESL hardware the operator owns.
- Do not add documentation aimed at store use, retail deployment, or third-party systems.
- Prefer generic ESL wording over store- or brand-specific framing in user-facing docs.
- Preserve credit to furrtek's reverse-engineering research where relevant.

## What to avoid

- Don't break existing functionality without good reason
- Don't add external dependencies — this runs on a microcontroller with strict constraints
- Don't submit AI-generated code without reviewing and testing it
- Keep memory usage minimal — every byte counts on the Flipper Zero

## Reporting issues

Open a GitHub issue with:
- What you expected to happen
- What actually happened
- Your Flipper firmware version
- The ESL model you're using (if applicable)

## Hardware safety

Be careful with repeated refresh or LED test loops on your own tags. Power and display behavior vary by model, and long-running tests can have side effects.

## License

By contributing, you agree that your contributions will be licensed under GPL-3.0.
