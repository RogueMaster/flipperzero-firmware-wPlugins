# CLAUDE.md

Spec-driven development on **OpenSpec**, Claude-only:
a change is planned as an OpenSpec change before its code is written,
unless it leaves every spec true.

## Sessions

- **A session plans or implements, never both,
  and ends with every file it wrote committed:**
  no session hands uncommitted work to the next.
- **Nothing is committed to `develop` or to a `feature/` or `bugfix/` branch
  before the owner has reviewed its diff in CLion;**
  a passing hook or a request to finish does not replace it.
- **Memory keeps decisions and preferences, the direction included,
  never the state of work:**
  what is planned, uncommitted or under review is read from git and `openspec list`.
- **Replies are concrete, dry and strictly to the point:**
  the answer first, no filler, no recap.
  A full screen is justified only by substance;
  half a screen of filler is already too much.

## Planning

- **One OpenSpec change per session — per agent when several are planned at once — and no code,**
  not even in a scratch clone.
  Planning starts on `develop` with a clean tree,
  a replan of a change under implementation on its branch,
  and each artifact follows what `openspec instructions <artifact> --change <id>` returns.
  A fact the docs and the code cannot settle is probed only with the owner's yes,
  or recorded as an assumption that the first task checks.
- **Before writing, the session says how hard the change is, and why, in one line,
  and offers the model and effort for it:**

  | Change | Model | Effort |
  | --- | --- | --- |
  | Relatively simple | Opus 5.5 | `high`, its default |
  | Complex or very complex | Opus 5.5 | `max` |
  | Needs very deep study and understanding | the newest Fable | `max` |

  The owner switches or not, and the session goes on with whatever is set.
  The proposal records how hard the change is: that decides where it is implemented.
- **A plan lands on `develop` as one commit, `chore(openspec): plan <id>`,**
  and touches nothing outside `openspec/changes/`;
  no branch is started or finished for it.
- **A set of changes that depend on each other is better not planned ahead:**
  keep the direction in a roadmap or in memory,
  and plan each change once the one it builds on has been applied.
  A plan that does build on queued ones accounts for them,
  as the `rules` in `openspec/config.yaml` require.
  Queued plans are applied in an order that follows from what each builds on,
  or that the owner sets.
- **Before a plan lands, three checks pass:**
  `wc -w` keeps its artifacts within the budgets in `openspec/config.yaml`;
  `openspec validate <id> --strict` passes;
  and in a copy of the repository, never in the checkout,
  `openspec archive -y` runs for every queued plan in order, this one last,
  after which `openspec validate --specs --strict` flags no spec it did not flag before the archives.
- **A replan is planning, too:**
  a session of its own and one commit, `chore(openspec): replan <id>` —
  on `develop` while the plan is queued, on its branch once it is under implementation —
  and every plan queued after it passes the checks again.
- **Several plans at once take one agent per plan, run by one session:**
  - Before the launch, the owner confirms the number of agents,
    their model and effort by the table above, and the items.
  - The session maps what the items share — symbols, requirements, files —
    and which plan answers for each.
    Items that change one symbol or requirement are chained, not run side by side.
  - Each agent works in its own worktree under `.claude/worktrees/`,
    cut before its launch from the local `develop` commit,
    or, in a chain, from the commit of the agent before it.
    It writes one plan by the rules above, asks nothing,
    marks in its proposal each arguable decision for the owner to strike,
    runs the three checks, and commits on its own branch.
  - The session brings every plan into the checkout, staged, where the IDE shows it,
    reads each whole — a report is a claim —
    and fixes what lies between them:
    a symbol nothing reads any more, or one added for a neighbor;
    one thing written under two plans;
    a requirement two plans change;
    a count that ignores the plans before it.
    It runs the checks on the whole queue,
    and after the owner's review lands each plan as one commit, in order.
  - The agents' branches and worktrees stay until the owner removes them.

## Implementation

- **An implementation session works on one change, on Opus 5.5 at `high`, its default.**
- **Implemented, a change reaches `develop` as one commit with all of it:**
  code, tests, ticked tasks and, for a planned change, the specs it changes and its archived plan.
  No fix-up, sync or archive commit follows it.
  Its subject is the one the proposal names, reviewed with the plan,
  and a replan that changes what the change does rewrites it;
  a change with no plan gets its subject at the commit.
- **Where it is made depends on the change:**
  - a simple change that leaves every spec in `openspec/specs/` true needs no plan
    and is made on `develop`;
  - a planned change its proposal calls simple is implemented on `develop`;
  - a planned change its proposal calls complex, or harder,
    is implemented on `feature/<id>`, started with `git flow feature start <id>`,
    so it can be pushed for the team's review.
- **On `feature/<id>`, commits follow the work:**
  each logical, self-contained and green, in the order a reviewer reads them;
  nothing is squashed while the work goes on.
  `git flow feature finish <id> --squash-message "<the proposal's subject>"` lands the branch as its one commit:
  `.gitflow` squashes features, and only the flag gives the subject.
- **Once the last task is done, `openspec archive <id> -y`**
  applies the change's deltas to `openspec/specs/` and moves the change into `openspec/changes/archive/`:
  on `develop` within the change's one commit,
  on a branch within its last commit before the finish.
- **Before the finish, the owner reviews `git diff develop...feature/<id>`**
  against the subject the proposal names.
- **A stop** — a decision the plan did not make, or a change wider than its specs —
  first moves a change made on `develop` to `feature/<id>`,
  as `git flow feature start <id>` takes the uncommitted work along.
  The finished tasks are committed, the unfinished task's code is dropped,
  the replan lands as a commit on the branch, and a fresh session resumes.
- **A `feature/` or `bugfix/` branch may be rebased onto `develop` and force-pushed;
  no other branch ever is.**

## Commits and releases

Commit messages follow Conventional Commits, in English:
`type(scope): description`.
The scope names the area of code or the tracker's task id —
`feat(catalog): …`, `fix(PROJ-32): …` —
and is left out only where nothing narrower than the whole project fits,
as in `chore: release 0.1`.
The subject is in the imperative mood,
lowercase after the prefix,
without a closing period,
72 characters at most and 50 where it can be.

A commit carries no body:
the message is the subject line alone,
and trailers, if any, follow it after a blank line.
No line credits an AI agent or model in any form —
no `Co-Authored-By:` trailer for a machine address,
no "Generated with" line;
every other trailer is allowed.

A release is named by a counter, `MAJOR.MINOR`:
the first is `0.1`, and each one after it raises `MINOR` by one.
A `.fap` stores only these two numbers,
and the Flipper Apps Catalog takes an update only under a higher version,
so a third part would go unseen.
`mise run release 0.1`, run on a clean `develop`, cuts the release whole:
it starts `release/0.1`, makes the commit `chore: release 0.1`, the branch's last,
finishes the branch and pushes `main`, `develop` and the tag atomically.
The tag is `v` followed by the name —
`v0.1`, put by the finish, as `tagPrefix` is `v` in `.gitflow`.
`fap_version` in `application.fam` carries the name, never the `v`,
and is set by that commit,
which also regenerates `CHANGELOG.md` with git-cliff
and puts the release's section on top of `catalog/changelog.md`;
nothing else writes either file.

## Generated files

The `opsx` / `openspec-*` glue under `.claude/` is generated —
regenerate it via `mise exec -- openspec update`, don't hand-edit.
