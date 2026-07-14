# Flipper Wedge - Documentation Index

Welcome to the comprehensive documentation for the Flipper Wedge Flipper Zero application.

---

## Documentation Overview

This project has extensive documentation to support **maintenance, stability, and cross-firmware compatibility** as the primary goals.

### For Claude (AI Assistant)

**Start Here**: [../CLAUDE.md](../CLAUDE.md)
- Comprehensive maintenance guide
- Testing & validation protocols
- Firmware update procedures
- Code architecture reference
- Quality gates and best practices

### For Developers & Maintainers

**Core Documentation**:
1. **[CLAUDE.md](../CLAUDE.md)** - Primary maintenance guide (start here!)
2. **[ARCHITECTURE_PATTERNS.md](ARCHITECTURE_PATTERNS.md)** - Code patterns and critical architecture
3. **[QUICK_REFERENCE.md](QUICK_REFERENCE.md)** - Fast lookup for common tasks
4. **[FIRMWARE_COMPATIBILITY.md](FIRMWARE_COMPATIBILITY.md)** - Compatibility matrix & tested versions
5. **[API_MIGRATION_LOG.md](API_MIGRATION_LOG.md)** - Historical API changes & adaptations
6. **[TESTING_AUTOMATION.md](TESTING_AUTOMATION.md)** - Testing strategy & automation

**Build & Deploy**:
- **[BUILD_MULTI_FIRMWARE.md](../BUILD_MULTI_FIRMWARE.md)** - Multi-firmware build instructions
- **Build Scripts**: `build.sh`, `deploy.sh`, `build-all-firmwares.sh` (in project root)

**Version History**:
- **[changelog.md](changelog.md)** - Version history and changes

### For End Users

**User Documentation**:
- **[README.md](../README.md)** - User guide, installation, usage, features
- **GitHub Releases** - Pre-built FAP files and release notes

---

## Document Structure

```
flipper_wedge/
├── CLAUDE.md                          # 🤖 AI Assistant: Main maintenance guide
├── README.md                          # 👤 Users: Installation & usage
├── BUILD_MULTI_FIRMWARE.md            # 🔧 Developers: Build instructions
└── docs/
    ├── README.md                      # 📚 This file (documentation index)
    ├── ARCHITECTURE_PATTERNS.md       # 🏗️ Code patterns & architecture
    ├── QUICK_REFERENCE.md             # ⚡ Quick lookup & commands
    ├── FIRMWARE_COMPATIBILITY.md      # ✅ Compatibility matrix
    ├── API_MIGRATION_LOG.md           # 📝 API change history
    ├── TESTING_AUTOMATION.md          # 🧪 Testing guide
    └── changelog.md                   # 📅 Version history
```

---

## Quick Navigation

### I want to...

**Build the app**
→ See [Build Scripts](#build-scripts) or [BUILD_MULTI_FIRMWARE.md](../BUILD_MULTI_FIRMWARE.md)

**Fix a bug**
→ See [CLAUDE.md - Maintenance Philosophy](../CLAUDE.md#maintenance-philosophy)

**Handle a firmware update**
→ See [CLAUDE.md - Firmware Update Procedures](../CLAUDE.md#firmware-update-procedures)

**Run tests**
→ See [CLAUDE.md - Testing Protocols](../CLAUDE.md#testing--validation-protocols)

**Check compatibility**
→ See [FIRMWARE_COMPATIBILITY.md](FIRMWARE_COMPATIBILITY.md)

**Find a command**
→ See [QUICK_REFERENCE.md](QUICK_REFERENCE.md)

**Understand API changes**
→ See [API_MIGRATION_LOG.md](API_MIGRATION_LOG.md)

**Automate tests**
→ See [TESTING_AUTOMATION.md](TESTING_AUTOMATION.md)

**Release a new version**
→ See [Release Checklist](#release-checklist)

---

## Build Scripts

The project includes three build scripts for multi-firmware support:

### 1. Build for Specific Firmware
```bash
./build.sh [firmware] [--branch BRANCH] [--tag TAG]
```

**Examples**:
```bash
./build.sh official              # Latest stable (recommended)
./build.sh unleashed             # Unleashed release branch
./build.sh momentum              # Momentum release branch
./build.sh official --tag 1.3.4  # Specific version
```

**Output**: `dist/<firmware>/<version>/flipper_wedge.fap`

### 2. Build and Deploy
```bash
./deploy.sh [firmware]
```

Builds and uploads to connected Flipper via USB.

### 3. Build All Firmwares
```bash
./build-all-firmwares.sh
```

Builds FAPs for all supported firmwares (Official, Unleashed, Momentum, RogueMaster).

---

## Supported Firmwares

| Firmware | Status | Build Command | Notes |
|----------|--------|---------------|-------|
| **Official** | ✅ Primary | `./build.sh official` | Main target, most stable |
| **Unleashed** | ✅ Supported | `./build.sh unleashed` | Fully compatible |
| **Momentum** | ✅ Supported | `./build.sh momentum` | Includes Xtreme |
| **RogueMaster** | ⚠️ Secondary | `./build.sh roguemaster` | Test thoroughly |

**Details**: See [FIRMWARE_COMPATIBILITY.md](FIRMWARE_COMPATIBILITY.md) for tested versions and known issues.

---

## Release Checklist

Before creating a new release:

1. **Code Quality**
   - [ ] Builds on all 4 firmwares without warnings
   - [ ] Full regression test suite passed
   - [ ] No open critical bugs

2. **Documentation**
   - [ ] [changelog.md](changelog.md) updated
   - [ ] [FIRMWARE_COMPATIBILITY.md](FIRMWARE_COMPATIBILITY.md) updated with tested versions
   - [ ] [README.md](../README.md) updated (if user-facing changes)
   - [ ] Version bumped in `application.fam`

3. **Build & Tag**
   - [ ] Build FAPs: `./build-all-firmwares.sh`
   - [ ] Create git tag: `git tag v1.X`
   - [ ] Push tag: `git push origin v1.X`

4. **Release**
   - [ ] Upload FAPs to GitHub release
   - [ ] Write release notes (based on changelog)
   - [ ] Test download links

**Detailed Checklist**: See [QUICK_REFERENCE.md - Version Release Checklist](QUICK_REFERENCE.md#version-release-checklist)

---

## Maintenance Philosophy

This project is **feature-complete**. Focus is on:

✅ **Stability** - No changes without necessity
✅ **Compatibility** - Test all firmwares before release
✅ **Regression Prevention** - Validate all existing features
✅ **API Monitoring** - Proactive firmware update tracking
✅ **User Experience** - Consistent behavior across firmwares

❌ **Avoid**: Unnecessary refactoring, feature creep, untested changes

**Full Philosophy**: See [CLAUDE.md - Maintenance Philosophy](../CLAUDE.md#maintenance-philosophy)

---

## Resources

### Official Links
- **Flipper Devices**: https://docs.flipper.net/
- **Official Firmware**: https://github.com/flipperdevices/flipperzero-firmware
- **Unleashed**: https://github.com/DarkFlippers/unleashed-firmware
- **Momentum**: https://github.com/Next-Flip/Momentum-Firmware
- **RogueMaster**: https://github.com/RogueMaster/flipperzero-firmware-wPlugins

### Community
- **Flipper Forum**: https://forum.flipper.net/
- **Project Issues**: [GitHub Issues](../../issues)
- **Discussions**: [GitHub Discussions](../../discussions)

---

## Support

If you encounter issues or have questions:

1. Check the [Troubleshooting](../README.md#troubleshooting) section in user docs
2. Review [QUICK_REFERENCE.md - Common Issues](QUICK_REFERENCE.md#common-issues--quick-fixes)
3. Review [open issues](../../issues) on GitHub
4. Create a [new issue](../../issues/new) if your problem isn't already reported
5. Join discussions in the [Discussions](../../discussions) section

---

*This documentation suite is designed to help maintain the Flipper Wedge app across firmware versions and years of development. Keep it updated and it will serve you well.*
