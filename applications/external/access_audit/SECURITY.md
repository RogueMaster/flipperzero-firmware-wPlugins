# Security Policy

## Reporting a vulnerability

If you find a security issue in this project, please report it privately via [GitHub's vulnerability reporting](../../security/advisories/new) rather than opening a public issue.

Include:
- A description of the issue
- Steps to reproduce
- Any relevant files or output

I'll respond as soon as possible and keep you updated on the fix.

## Scope

This project is a passive NFC/RFID auditing tool. It does not connect to the internet, store credentials, or interact with external services. Most security concerns will relate to:

- Incorrect card classification leading to misleading risk scores
- Logic errors in the scoring or rule engine
- Incorrect advice that could lead a user to believe a vulnerable system is secure

## Authorized use

This tool is intended for security professionals, system owners, and researchers assessing systems they own or are permitted to test. Misuse is outside the scope of this project and this policy.
