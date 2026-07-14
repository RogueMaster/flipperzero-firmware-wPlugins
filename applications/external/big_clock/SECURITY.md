# Security Policy

## Supported Versions

| Version | Supported          |
| ------- | ------------------ |
| Latest  | :white_check_mark: |
| Older   | :x:                |

We only provide security updates for the latest version of each app.

## Reporting a Vulnerability

If you discover a security vulnerability, please report it responsibly:

### Do NOT

- Open a public GitHub issue
- Disclose publicly before we've addressed it
- Exploit the vulnerability

### Do

1. **Email** the maintainer directly (see repository owner profile)
2. **Include**:
   - Description of the vulnerability
   - Steps to reproduce
   - Potential impact
   - Suggested fix (if any)

### What to Expect

- **Acknowledgment** within 48 hours
- **Status update** within 7 days
- **Resolution timeline** based on severity

## Security Considerations

### Flipper Zero Apps

These apps run on Flipper Zero hardware with the following considerations:

- Apps have access to device hardware (screen, buttons, radio modules)
- Apps run in userspace with SDK-provided APIs
- No network access unless explicitly using radio modules
- Data stored on SD card is accessible to all apps

### Our Commitments

- No malicious code or backdoors
- No unnecessary permissions or hardware access
- No data collection or telemetry
- Open source for full transparency

### Best Practices for Users

- Only install apps from trusted sources
- Review source code if concerned
- Keep Flipper Zero firmware updated
- Report suspicious behavior

## Scope

This security policy covers:

- All apps in the `apps/` directory
- Build and deployment scripts
- Documentation that could lead to security issues

Out of scope:

- Flipper Zero firmware vulnerabilities (report to [Flipper Devices](https://github.com/flipperdevices/flipperzero-firmware))
- Third-party dependencies (report to respective maintainers)
