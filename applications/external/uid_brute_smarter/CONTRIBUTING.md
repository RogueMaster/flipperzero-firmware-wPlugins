# Contributing to UID Brute Smarter

Thanks for your interest in contributing to UID Brute Smarter! This document provides guidelines for contributions.

## Getting Started

1. Fork the repository
2. Clone your fork: `git clone https://github.com/YOUR_USERNAME/uid_brute_smarter.git`
3. Create a feature branch: `git checkout -b feature/amazing-feature`
4. Make your changes
5. Test thoroughly
6. Commit your changes: `git commit -m 'Add amazing feature'`
7. Push to your fork: `git push origin feature/amazing-feature`
8. Open a Pull Request

## Development Setup

### Prerequisites
- Flipper Zero device (optional but recommended)
- Momentum Firmware development environment
- Basic C programming knowledge
- Git

### Building
```bash
# Clone Momentum Firmware
git clone https://github.com/Next-Flip/Momentum-Firmware.git
cd Momentum-Firmware

# Add the app
cp -r path/to/uid_brute_smarter applications_user/

# Build
./fbt fap_uid_brute_smarter

# Test on device
./fbt launch APPSRC=uid_brute_smarter
```

## Code Standards

### Style Guidelines
- Follow existing code style
- Use 4 spaces for indentation (no tabs)
- Keep lines under 120 characters
- Add meaningful variable and function names
- Comment complex logic

### Code Quality
- No compiler warnings
- No memory leaks
- Proper error handling
- Input validation
- Thread safety where applicable

### Testing
- Add unit tests for new features
- Ensure all existing tests pass
- Test on actual hardware when possible
- Check edge cases

## Pull Request Process

1. **Update Documentation**: Update README.md if needed
2. **Add Tests**: Include tests for new functionality
3. **Clean History**: Squash commits if needed
4. **Description**: Provide clear PR description
5. **Screenshots**: Include screenshots for UI changes

### PR Title Format
- `feat:` New feature
- `fix:` Bug fix
- `docs:` Documentation only
- `style:` Code style changes
- `refactor:` Code refactoring
- `perf:` Performance improvements
- `test:` Test additions/fixes

Example: `feat: add configuration persistence`

## What We're Looking For

### High Priority
- Bug fixes
- Performance improvements
- Pattern detection enhancements
- UI/UX improvements
- Test coverage

### Ideas Welcome
- New pattern detection algorithms
- Export/import features
- Configuration profiles
- Advanced logging
- Integration features

## Testing

### Running Tests
```bash
cd tests
make clean
make all
./run_tests.sh
```

### Test Coverage
We aim for >90% coverage on critical components:
- Pattern detection engine
- Key management
- NFC parsing

## Community

### Communication
- Open issues for bugs/features
- Discuss major changes first
- Be respectful and constructive
- Help others when possible

### Code of Conduct
- Be welcoming and inclusive
- Respect differing viewpoints
- Accept constructive criticism
- Focus on what's best for the project

## Recognition

Contributors will be recognized in:
- README.md contributors section
- Release notes
- Project credits

## Questions?

Feel free to:
- Open an issue for clarification
- Start a discussion
- Contact maintainers

Thank you for contributing to make UID Brute Smarter better!