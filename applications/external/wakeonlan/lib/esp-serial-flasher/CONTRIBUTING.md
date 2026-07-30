# Contributing to ESP Serial Flasher

We welcome contributions to the ESP Serial Flasher project! This guide will help you get started with contributing.

## Before You Contribute

**Please open an issue first** to discuss whether your proposed change could be accepted. This helps ensure that:

- Your contribution aligns with the project's goals
- You don't spend time on changes that might not be merged
- We can provide guidance on the best implementation approach

## Types of Contributions

### Bug Reports

- Use [GitHub Issues](https://github.com/espressif/esp-serial-flasher/issues)
- Check if the issue has already been reported
- Provide detailed reproduction steps
- Include relevant system information (OS, toolchain versions, etc.)

### Feature Requests

- Use [GitHub Issues](https://github.com/espressif/esp-serial-flasher/issues)
- Describe the use case and motivation
- Explain the expected behavior

### Pull Requests

- Follow the guidelines below for code contributions
- Reference the related issue in your PR description

## Development Guidelines

### Code Style

Follow the [ESP-IDF project's contribution guidelines](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/contribute/style-guide.html), which include:

- Code formatting standards
- Naming conventions
- Documentation requirements

### Commit Messages

Use [Conventional Commits](https://www.conventionalcommits.org) format:

```text
type(scope): description

[optional body]

[optional footer(s)]
```

Examples:

```text
feat(uart): Add support for custom baud rates
fix(spi): Resolve timing issue with fast flash chips
docs(readme): Update installation instructions
```

## Setting up Development Environment

### Install Pre-Commit Hooks

We use [pre-commit](https://pre-commit.com/) to automatically enforce code quality standards:

```bash
pip install pre-commit
pre-commit install
```

This will automatically run linters and formatters on your code before each commit.

## Documentation

### Updating Documentation

- Update relevant documentation files when making changes
- Follow the [Espressif Manual of Style](https://mos.espressif.com/)
- Ensure all code examples are tested and working

## Getting Help

### Where to Ask Questions

- **Bug reports**: [GitHub Issues](https://github.com/espressif/esp-serial-flasher/issues)
- **Development questions**: Open an issue with the "question" label

### Additional Resources

- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [Espressif Manual of Style](https://mos.espressif.com/)
- [Conventional Commits](https://www.conventionalcommits.org/)

## License

By contributing to this project, you agree that your contributions will be licensed under the same [Apache 2.0 License](LICENSE) that covers the project.
