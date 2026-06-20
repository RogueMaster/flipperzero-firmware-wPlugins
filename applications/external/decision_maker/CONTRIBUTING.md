# Contributing to Random Decision Maker

Thank you for your interest in contributing! This document explains how to report bugs, request features, and submit code changes.

---

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [How to Contribute](#how-to-contribute)
  - [Reporting Bugs](#reporting-bugs)
  - [Requesting Features](#requesting-features)
  - [Submitting a Pull Request](#submitting-a-pull-request)
- [Development Setup](#development-setup)
- [Coding Guidelines](#coding-guidelines)
- [Commit Message Format](#commit-message-format)

---

## Code of Conduct

Be respectful and constructive. Harassment, trolling, or discriminatory language of any kind will not be tolerated.

---

## Getting Started

1. **Fork** the repository on GitHub.
2. **Clone** your fork locally:
   ```bash
   git clone https://github.com/<your-username>/random_decision_maker.git
   cd random_decision_maker
   ```
3. **Set up the Flipper Zero build environment** following the [official fbt guide](https://github.com/flipperdevices/flipperzero-firmware/blob/dev/documentation/fbt.md).
4. Place (or symlink) the repository inside `applications_user/` in your firmware tree.

---

## How to Contribute

### Reporting Bugs

Open an [issue](https://github.com/Gerijacki/random_decision_maker/issues) and include:

- Flipper Zero firmware version (e.g., `0.98.3` or `unleashed-07.xx`)
- Steps to reproduce the bug
- Expected vs. actual behaviour
- Any relevant screenshots or logs

### Requesting Features

Open an [issue](https://github.com/Gerijacki/random_decision_maker/issues) with the `enhancement` label. Describe:

- The problem you are trying to solve
- Your proposed solution (if any)
- Any Flipper Zero API constraints you are aware of

### Submitting a Pull Request

1. Create a branch from `main` with a descriptive name:
   ```bash
   git checkout -b fix/crash-on-empty-list
   # or
   git checkout -b feat/persistent-storage
   ```
2. Make your changes (see [Coding Guidelines](#coding-guidelines)).
3. Build and test on a real device or in [qFlipper](https://github.com/flipperdevices/qFlipper) if possible.
4. Push your branch and open a pull request against `main`.
5. Fill in the PR description explaining **what** changed and **why**.

Pull requests should:
- Target a single logical change
- Not break existing functionality
- Keep all user-facing strings in **English**
- Pass the firmware's built-in linter (`./fbt lint`)

---

## Development Setup

```bash
# From the root of the flipperzero-firmware tree:
./fbt APPSRC=applications_user/random_decision_maker launch
```

Useful targets:

| Command | Description |
|---------|-------------|
| `./fbt APPSRC=... launch` | Build, flash, and run on a connected Flipper |
| `./fbt APPSRC=... fap` | Build `.fap` file only |
| `./fbt lint` | Run the firmware code style linter |

---

## Coding Guidelines

- **Language**: C11 (`-std=c11`). No C++.
- **Style**: Follow the [Flipper Zero firmware code style](https://github.com/flipperdevices/flipperzero-firmware/blob/dev/documentation/coding_style.md) — 4-space indentation, snake_case, Allman braces.
- **Safety**: Always bounds-check string copies (`strncpy` + explicit null-terminator). No dynamic allocation inside draw/input callbacks.
- **Thread safety**: Never access view models directly from the timer callback. Use `view_dispatcher_send_custom_event()` instead.
- **Strings**: All user-visible text must be in **English**.
- **Constants**: Add named `#define` constants rather than magic numbers.

---

## Commit Message Format

Use the imperative mood and keep the subject line under 72 characters:

```
<type>: <short summary>

[Optional body explaining why, not what]
```

Types: `feat`, `fix`, `refactor`, `docs`, `chore`, `test`.

Examples:
```
feat: add persistent storage for decisions
fix: prevent crash when decision list is empty
docs: update controls table in README
```

---

Thank you for helping make this project better!
