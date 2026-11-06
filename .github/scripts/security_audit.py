#!/usr/bin/env python3
"""Static security audit of an external pull request.

Source of truth: ataman-lab/overclock-lab .github/scripts/security_audit.py (copied verbatim to the
child repos). Run by .github/workflows/security-audit.yml on pull_request_target, from the BASE branch:
the PR's code is fetched as git objects and only diffed, never checked out or executed.

Usage: security_audit.py <base-sha> <head-sha> [report.md]
PR title/body come from env PR_TITLE / PR_BODY (data, never interpolated into shell).
Prints the verdict (CLEAN | REVIEW | RISK) on stdout. Exit code 0 unless the audit itself crashed.
Test: python3 .github/scripts/security_audit_test.py
"""
import os
import re
import subprocess
import sys

HIGH, MEDIUM = "HIGH", "MEDIUM"

# Paths whose change can reach the autopilot, CI secrets or a developer's machine.
PATH_RULES = [
    (HIGH, r"^\.github/", "CI/workflow or GitHub config change (could steal secrets or hijack the autopilot)"),
    (HIGH, r"(^|/)(CLAUDE|AGENTS)\.md$|^\.claude/|^\.mcp\.json$", "AI agent instructions/config change (prompt injection into the autopilot)"),
    (HIGH, r"^infra/board-hook/|^infra/child-task/", "autopilot infrastructure change"),
    (HIGH, r"^\.devcontainer/|^\.vscode/|^\.husky/|^\.githooks/|(^|/)\.envrc$", "editor/devcontainer/git-hook config runs code on open or commit"),
    (HIGH, r"(^|/)build\.rs$|(^|/)setup\.py$|(^|/)\.npmrc$|(^|/)\.cargo/config(\.toml)?$|(^|/)\.pth$", "build or install script runs code at build/install time"),
    (MEDIUM, r"(^|/)(package\.json|package-lock\.json|yarn\.lock|pnpm-lock\.yaml|Cargo\.toml|Cargo\.lock|"
             r"requirements[^/]*\.txt|pyproject\.toml|Pipfile(\.lock)?|poetry\.lock|go\.(mod|sum)|Gemfile(\.lock)?)$",
     "dependency change (new or swapped package)"),
    (MEDIUM, r"^scripts/|\.(sh|ps1|bat|cmd)$|(^|/)(Makefile|Dockerfile)$", "executable script changed (may run in CI or the autopilot)"),
]

# Patterns on ADDED lines (and the PR title/body for the injection group).
SECRET_RULES = [
    (HIGH, r"\bsecrets\.|CLAUDE_CODE_OAUTH_TOKEN|ANTHROPIC_API_KEY|\bBOT_TOKEN\b|ROUTINE_TOKEN|EXPO_TOKEN|"
           r"ACTIONS_ID_TOKEN_REQUEST|ACTIONS_RUNTIME_TOKEN|\bGITHUB_TOKEN\b|\bGH_TOKEN\b", "references a secret or CI token"),
    (HIGH, r"curl[^\n|]*\|\s*(ba|z)?sh\b|wget[^\n|]*\|\s*(ba|z)?sh\b|/dev/tcp/|\bnc\s+-[a-z]*e\b|"
           r"IEX\s*\(|Invoke-Expression|DownloadString|powershell[^\n]*-enc", "download-and-execute / reverse shell pattern"),
    (HIGH, r"webhook\.site|requestbin|ngrok\.(io|app)|pipedream\.net|interact\.sh|\.oast\.|burpcollaborator|"
           r"pastebin\.com|transfer\.sh|discord(app)?\.com/api/webhooks", "known exfiltration endpoint"),
    (HIGH, r"base64\s+(-d|--decode)|\beval\s*\(\s*(atob|Buffer\.from|base64)|exec\s*\(\s*(base64|codecs)", "decodes and runs hidden code"),
    (MEDIUM, r"(std::env::var|process\.env|os\.environ|getenv)\W[^\n]*(TOKEN|SECRET|KEY|PASS)", "reads a credential from the environment"),
    (MEDIUM, r"https?://\d{1,3}(\.\d{1,3}){3}", "URL to a raw IP address"),
]
INJECTION_RULES = [
    (HIGH, r"ignore\s+(all\s+|any\s+)?(the\s+)?(previous|prior|above|earlier)\s+(instructions|prompts|rules)|"
           r"disregard\s+[^\n]{0,40}instructions|forget\s+(all\s+|your\s+)?(previous\s+)?instructions",
     "prompt injection: tells an AI to ignore its instructions"),
    (HIGH, r"\byou\s+are\s+(now\s+)?(claude|an?\s+ai|chatgpt|the\s+assistant)\b|\bsystem\s+prompt\b|"
           r"<\s*/?\s*(system|instructions|assistant)\s*>|(new|updated|additional)\s+instructions\s+for\s+(claude|the\s+ai|the\s+assistant|the\s+agent)",
     "prompt injection: addresses the AI agent directly"),
    (HIGH, r"do\s+not\s+(tell|inform|notify)\s+(the\s+)?(user|owner|maintainer|vlad)|\bclaude-task\b|needs-vlad|"
           r"\bCHAIN_AUTOMERGE\b|gh\s+(pr\s+merge|secret|api\s+[^\n]*secrets)", "targets the autopilot or merge flow"),
]
MD_HIDDEN = (MEDIUM, r"<!--", "hidden HTML comment in Markdown (can carry invisible instructions)")
# Invisible / bidi characters (Trojan Source, hidden prompt text).
HIDDEN_CHARS = re.compile("[‪-‮⁦-⁩​-‍⁠﻿\U000e0000-\U000e007f]")
MAX_ADDED = 3000


def git(*args):
    return subprocess.run(["git", "-c", "core.quotepath=off", *args], check=True, capture_output=True,
                          text=True, encoding="utf-8", errors="replace").stdout


def parse_diff(diff):
    """-> {path: [(line_no, text), ...]} of added lines."""
    files, path, line = {}, None, 0
    for raw in diff.splitlines():
        if raw.startswith("+++ "):
            path = raw[6:] if raw.startswith("+++ b/") else None
            if path is not None:
                files.setdefault(path, [])
        elif raw.startswith("@@"):
            m = re.search(r"\+(\d+)", raw)
            line = int(m.group(1)) if m else 0
        elif path is not None and raw.startswith("+"):
            files[path].append((line, raw[1:]))
            line += 1
        elif path is not None and not raw.startswith("-"):
            line += 1
    return files


def audit(changed, added, binaries, title="", body=""):
    """changed: [paths]; added: {path: [(n, text)]}; binaries: [paths]. -> list of (level, where, why)."""
    found = []
    for p in changed:
        for level, rx, why in PATH_RULES:
            if re.search(rx, p):
                found.append((level, p, why))
                break
    for p in binaries:
        found.append((MEDIUM, p, "binary file (cannot be reviewed as text)"))
    total = 0
    for p, lines in added.items():
        total += len(lines)
        rules = SECRET_RULES + INJECTION_RULES + ([MD_HIDDEN] if p.lower().endswith((".md", ".mdx", ".html")) else [])
        if p.endswith("package.json"):
            rules = rules + [(HIGH, r'"(pre|post)?install"\s*:|"prepare"\s*:', "npm install hook runs code on npm install")]
        for n, text in lines:
            if HIDDEN_CHARS.search(text.lstrip("﻿") if n == 1 else text):
                found.append((HIGH, f"{p}:{n}", "invisible/bidi unicode characters (hidden text or Trojan Source)"))
            for level, rx, why in rules:
                if re.search(rx, text, re.IGNORECASE):
                    found.append((level, f"{p}:{n}", why))
    if total > MAX_ADDED:
        found.append((MEDIUM, "diff", f"{total} added lines: too big for a reliable automatic audit"))
    for where, text in (("PR title", title), ("PR body", body)):
        if HIDDEN_CHARS.search(text):
            found.append((HIGH, where, "invisible/bidi unicode characters"))
        for level, rx, why in INJECTION_RULES:
            if re.search(rx, text, re.IGNORECASE):
                found.append((level, where, why))
        if "<!--" in text:
            found.append((MEDIUM, where, "hidden HTML comment"))
    # One line per (where, why).
    return list(dict.fromkeys(found))


def verdict(found):
    levels = {f[0] for f in found}
    return "RISK" if HIGH in levels else "REVIEW" if MEDIUM in levels else "CLEAN"


def report(v, found, changed):
    icon = {"CLEAN": "🟢", "REVIEW": "🟡", "RISK": "🔴"}[v]
    out = [f"{icon} **Security audit: {v}** ({len(changed)} files changed)", ""]
    if v == "RISK":
        out.append("This PR touches something that can reach secrets, CI or the autopilot. Do not merge, "
                   "label or hand it to Claude until a maintainer has read every flagged line.")
    elif v == "REVIEW":
        out.append("Nothing clearly malicious, but read the flagged lines before merging.")
    else:
        out.append("No risky paths or patterns found. This is a pattern check, not a proof: still read the diff.")
    if found:
        out += ["", "| Level | Where | Why |", "|---|---|---|"]
        for level, where, why in found[:60]:
            safe = re.sub(r"[`|<>\n\r]", "_", where)[:120]
            out.append(f"| {level} | `{safe}` | {why} |")
        if len(found) > 60:
            out.append(f"| | | ... {len(found) - 60} more |")
    out += ["", "_Static check, no PR code was run. The autopilot never runs for external authors._"]
    return "\n".join(out) + "\n"


def main(argv):
    base, head = argv[1], argv[2]
    rng = f"{base}...{head}"
    diff = git("diff", "--no-ext-diff", "--no-textconv", "--no-color", "-U0", rng)
    changed = [p for p in git("diff", "--no-ext-diff", "--name-only", "--no-renames", rng).splitlines() if p]
    binaries = [l.split("\t")[-1] for l in git("diff", "--numstat", "--no-renames", rng).splitlines() if l.startswith("-\t-\t")]
    found = audit(changed, parse_diff(diff), binaries, os.environ.get("PR_TITLE", ""), os.environ.get("PR_BODY", ""))
    v = verdict(found)
    if len(argv) > 3:
        with open(argv[3], "w", encoding="utf-8") as f:
            f.write(report(v, found, changed))
    print(v)


if __name__ == "__main__":
    main(sys.argv)
