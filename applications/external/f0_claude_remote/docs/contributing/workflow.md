---
description: How a change is planned, implemented and landed with OpenSpec, git-flow and Claude Code.
---

# Workflow

Development is spec-driven, on [OpenSpec](https://github.com/Fission-AI/OpenSpec),
through Claude Code's `/opsx:*` commands.
[`CLAUDE.md`](https://github.com/petr-korobeinikov/f0-claude-remote/blob/develop/CLAUDE.md)
holds the process; in short:

A Claude Code session either plans or implements, never both,
and commits everything it wrote before `/clear`.

**Planning.**
A plan is one OpenSpec change,
committed on `develop` alone as `chore(openspec): plan <id>`,
with no branch of its own.
Several can be planned at once, one agent per plan;
a set of changes that depend on each other is better not planned ahead.

**Implementation.**
A change reaches `develop` as one commit with all of it:
code, tests and, for a planned change, its specs and archived plan.
A simple change is made right on `develop`;
a complex one runs on its own branch,
where commits follow the work until the finish squashes them:

```shell
git flow feature start <id>
git flow feature finish <id> --squash-message "<the subject its proposal names>"
```

Browse specs and changes with `mise exec -- openspec view`.

To install the agent skills into `.claude/skills/` in a fresh clone,
or to update them:

```shell
mise exec -- npx skills add petr-korobeinikov/skills --skill '*' --copy --agent claude-code -y
```

## Commits

A commit message is a Conventional Commit, in English,
its subject at most 72 characters, and no line of it credits an AI agent or a model:
[Hooks](hooks-and-ci.md#hooks) check all three at `commit-msg`.
The full rules, which agents read, are the Commits and releases section of
[`CLAUDE.md`](https://github.com/petr-korobeinikov/f0-claude-remote/blob/develop/CLAUDE.md#commits-and-releases).
