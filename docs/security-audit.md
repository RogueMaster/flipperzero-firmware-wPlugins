# Security audit for external PRs

Source of truth and full docs: ataman-lab/overclock-lab `docs/security-audit.md`. The files below are
verbatim copies; change them there first.

- `.github/workflows/security-audit.yml` + `.github/scripts/security_audit.py`: static audit of every PR
  from a fork or a non-collaborator (`pull_request_target`, default-branch code only, PR code is never
  checked out or run, no secrets). Comment + `security-audit` status, red on RISK, ntfy push to Vlad.
  Test: `python3 .github/scripts/security_audit_test.py`.
- `.github/workflows/claude-task.yml` `gate` job: the autopilot runs only if the label was added by
  someone with write access and the issue and all its non-bot comments come from OWNER/MEMBER/COLLABORATOR.
