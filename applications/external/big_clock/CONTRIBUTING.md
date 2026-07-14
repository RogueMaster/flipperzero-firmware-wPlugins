# Contributing to Flipper Apps

Thank you for your interest in contributing! This document provides guidelines for contributing to this repository.

## Code of Conduct

Be respectful and constructive in all interactions. We welcome contributors of all skill levels.

## How to Contribute

### Reporting Bugs

1. Check existing [issues](../../issues) to avoid duplicates
2. Create a new issue with:
   - Clear, descriptive title
   - Steps to reproduce
   - Expected vs actual behavior
   - Flipper Zero firmware version
   - Screenshots if applicable

### Suggesting Features

1. Open an issue with the `enhancement` label
2. Describe the feature and its use case
3. Explain why it would benefit users

### Submitting Changes

1. **Fork** the repository
2. **Create a branch** from `main`:
   ```bash
   git checkout -b feature/your-feature-name
   ```
3. **Make your changes** following the code style below
4. **Test** on actual Flipper Zero hardware
5. **Commit** with clear messages:
   ```bash
   git commit -m "Add feature: description"
   ```
6. **Push** to your fork:
   ```bash
   git push origin feature/your-feature-name
   ```
7. **Open a Pull Request** to `main`

## Development Setup

### Prerequisites

- Python 3.10+
- Poetry (recommended) or pip
- Flipper Zero device for testing

### Setup

```bash
# Clone your fork
git clone https://github.com/<your-username>/flipper-apps.git
cd flipper-apps

# Install dependencies
poetry install

# Build an app
cd apps/<app-name>
poetry run ufbt

# Build and deploy to Flipper
poetry run ufbt launch
```

## Code Style

### C Code

- Use 4 spaces for indentation
- Follow Flipper Zero SDK conventions
- Keep functions focused and small
- Add comments for complex logic
- Use descriptive variable names

### File Structure

Each app must contain:

```
apps/<app-name>/
├── application.fam      # App manifest (required)
├── <app_name>.c         # Main source file (required)
├── README.md            # Documentation (required)
├── changelog.md         # Version history (required)
├── VERSION              # Version number (required)
├── icon.png             # 10x10px 1-bit icon (required)
└── screenshots/         # qFlipper screenshots (required)
    └── screenshot1.png
```

### Commit Messages

- Use present tense: "Add feature" not "Added feature"
- Be concise but descriptive
- Reference issues when applicable: "Fix #123"

## Adding a New App

1. Copy the template:
   ```bash
   cp -r apps/_template apps/your-app-name
   ```

2. Update `application.fam`:
   - Set unique `appid`
   - Set `name`, `fap_version`, `fap_category`
   - Update `fap_description`

3. Implement your app in `app.c`

4. Update `README.md` and `changelog.md`

5. Create icon and take screenshots

6. Test thoroughly on hardware

## Pull Request Guidelines

- PRs must pass CI checks
- Include tests if applicable
- Update documentation as needed
- Keep changes focused (one feature/fix per PR)
- Respond to review feedback promptly

## Versioning

We use semantic versioning for apps:

- **Major.Minor** format (e.g., `1.0`, `1.1`, `2.0`)
- Increment minor for new features and fixes
- Increment major for breaking changes

Update version in both:
- `application.fam` (`fap_version`)
- `VERSION` file

## Questions?

Open an issue or start a discussion. We're happy to help!
