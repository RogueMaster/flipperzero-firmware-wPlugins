# Security Policy

## Reporting a vulnerability

If you discover a vulnerability — a way the app could damage hardware, brick a Flipper, leak unintended data, or be misused for unauthorized RF emission — **please report privately first**.

📧 [support@pingequa.com](mailto:support@pingequa.com) — subject: `[SECURITY] PINGEQUA RF Lab`

We aim to:
- Acknowledge within 72 hours
- Provide an initial assessment within 7 days
- Patch and release within 30 days for confirmed issues
- Credit you in the release notes (if you wish)

Please don't file public GitHub issues for security concerns until we've coordinated a patch release.

## Scope of security concerns

✅ **In scope:**
- Crashes that could damage Flipper hardware
- Code paths that could leave the SPI bus or GPIO pins in a state preventing other apps from working
- Memory corruption / buffer overruns
- Internal API misuse that could cause undefined hardware behavior

⚠️ **Out of scope:**
- Theoretical RF interference scenarios (this is a passive listen tool)
- Issues only present in unsupported firmware forks
- Issues only reproduced on non-PINGEQUA hardware
- Cosmetic UI bugs (file as a regular bug)

## Responsible use disclosure

This software is a **passive RF spectrum visualization tool**. It listens via nRF24L01+ Received Power Detector (RPD) — it does not transmit, decode, or capture data.

Future versions (v1.2+) may include opt-in transmit features (jammer mode, signal generators). Such features will:

1. Be clearly labeled and require explicit user activation
2. Default to disabled
3. Display legal warnings before activation
4. Be subject to additional legal review before release

Active RF emission within the unlicensed 2.4 GHz band is regulated:

- **United States**: FCC §15 (unlicensed devices), §333 (interference prohibition)
- **European Union**: ETSI EN 300 328 (power and duty cycle limits), R&TTE/RED directives
- **Other regions**: equivalent national regulators

Users are solely responsible for compliance with applicable laws. PINGEQUA and contributors accept no liability for misuse of this software, particularly:

- Jamming, spoofing, or interfering with networks you don't own
- Unauthorized eavesdropping on private communications
- Operation in restricted spectrum without proper licensing
- Use in safety-of-life or production environments without certification

If you're uncertain whether your use case is legal in your jurisdiction, consult local regulators before proceeding.

## Disclosures

No security issues have been disclosed to date. This section will be populated as the project matures.
