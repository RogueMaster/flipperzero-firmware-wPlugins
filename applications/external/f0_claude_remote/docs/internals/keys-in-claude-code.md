---
description: Why every command is a Ctrl chord, why voice is Ctrl+Q held down, and what each effort level keeps.
---

# Keys in Claude Code

Each fact ends with its kind and its source:
documented, read in Claude Code 2.1.311's package, or seen on the Mac.
A Claude Code update can break the second kind;
the package publishes no browsable source, so its links go to the archived change that read it.

## Ctrl chords

Typed, `/effort high` comes out under a non-Latin layout as `.уаащке рушр`,
and Enter then sends it as a message
(seen: [effort-actions, proposal](https://github.com/petr-korobeinikov/f0-claude-remote/blob/develop/openspec/changes/archive/2026-11-04-effort-actions/proposal.md)).

Under a non-Latin layout, in a terminal that does not report a key's position,
Claude Code matches what the terminal sends:
an ASCII control code triggers the Latin shortcut,
and a Cyrillic character matches no binding
(documented: [Keybindings](https://code.claude.com/docs/en/keybindings#non-us-keyboard-layouts)).
A Ctrl letter arrives as a control code under any layout,
so each command is a chord of Ctrl letters,
bound in `keybindings.json` to `command:<name>`, which runs `/<name>`
(documented: [Keybindings](https://code.claude.com/docs/en/keybindings)).
The text after `command:` runs as typed, argument and all
(Claude Code 2.1.311's package: [effort-actions, design](https://github.com/petr-korobeinikov/f0-claude-remote/blob/develop/openspec/changes/archive/2026-11-04-effort-actions/design.md)),
and the draft in the prompt stays
(seen: [effort-actions, task 4.1](https://github.com/petr-korobeinikov/f0-claude-remote/blob/develop/openspec/changes/archive/2026-11-04-effort-actions/tasks.md)).

Ctrl+L alone only redraws the screen
(documented: [Keybindings](https://code.claude.com/docs/en/keybindings)),
so `/clear` is `ctrl+x ctrl+l`.
Sent during a turn, `/clear` waits for the turn's end
(Claude Code 2.1.311's package: [remote-actions, design](https://github.com/petr-korobeinikov/f0-claude-remote/blob/develop/openspec/changes/archive/2026-11-04-remote-actions/design.md)),
and `/resume` lists the conversation it left
(documented: [Commands](https://code.claude.com/docs/en/commands)).

## Voice

`voice:pushToTalk` takes one key at a time
(documented: [Voice dictation](https://code.claude.com/docs/en/voice-dictation#rebind-the-dictation-key)),
so Ctrl+Q replaces Space, on the Mac's keyboard too.
Claude Code compares that key with the character it receives
(Claude Code 2.1.311's package: [remote-actions, design](https://github.com/petr-korobeinikov/f0-claude-remote/blob/develop/openspec/changes/archive/2026-11-04-remote-actions/design.md)),
so Option+K, the first choice, started no dictation under a non-Latin layout
(seen: [remote-actions, proposal](https://github.com/petr-korobeinikov/f0-claude-remote/blob/develop/openspec/changes/archive/2026-11-04-remote-actions/proposal.md)).

Claude Code tells a held key by its key repeats
(documented: [Voice dictation](https://code.claude.com/docs/en/voice-dictation#hold-to-record)).
The first press arms a 2000 ms fallback,
then a 200 ms gap between repeats ends the recording
(Claude Code 2.1.311's package: [remote-actions, design](https://github.com/petr-korobeinikov/f0-claude-remote/blob/develop/openspec/changes/archive/2026-11-04-remote-actions/design.md)),
so with key repeat off a recording ends about 2.2 s into the hold.
The device holds Ctrl+Q down for as long as OK is held, and the Mac repeats it.
On the Mac's keyboard, a 10-second hold under a non-Latin layout dictated throughout
(seen: [remote-actions, task 1.2](https://github.com/petr-korobeinikov/f0-claude-remote/blob/develop/openspec/changes/archive/2026-11-04-remote-actions/tasks.md)),
and a hold of OK over Bluetooth dictated as well
(seen: [remote-actions, task 4.1](https://github.com/petr-korobeinikov/f0-claude-remote/blob/develop/openspec/changes/archive/2026-11-04-remote-actions/tasks.md)).

## Effort levels

Effort high is `ctrl+x ctrl+f`, and effort max `ctrl+x ctrl+x`:
with `ctrl+q` bound to voice, `ctrl+x ctrl+q` started a recording
(seen: [remote-actions, task 1.3](https://github.com/petr-korobeinikov/f0-claude-remote/blob/develop/openspec/changes/archive/2026-11-04-remote-actions/tasks.md)),
and Ctrl+X twice needs no key the device does not already send.

`high` is saved as the default for the model, and `max` lasts the session
(documented: [Commands](https://code.claude.com/docs/en/commands);
[Model configuration](https://code.claude.com/docs/en/model-config#adjust-effort-level)).
Typed during a turn, `/effort` applies from the next message
(documented: [Model configuration](https://code.claude.com/docs/en/model-config#when-a-new-effort-level-starts));
a keybinding's command runs at once
(Claude Code 2.1.311's package: [effort-actions, design](https://github.com/petr-korobeinikov/f0-claude-remote/blob/develop/openspec/changes/archive/2026-11-04-effort-actions/design.md)),
and the level changed inside a turn with no line printed
(seen: [effort-actions, task 4.2](https://github.com/petr-korobeinikov/f0-claude-remote/blob/develop/openspec/changes/archive/2026-11-04-effort-actions/tasks.md)).

## What the schema warns of

The schema allows no space in a command's name
(documented: [`claude-code-keybindings.json`](https://www.schemastore.org/claude-code-keybindings.json)),
so an editor validating against `$schema` marks both effort lines.
Claude Code only notes the space in its debug log and runs them
(Claude Code 2.1.311's package: [effort-actions, design](https://github.com/petr-korobeinikov/f0-claude-remote/blob/develop/openspec/changes/archive/2026-11-04-effort-actions/design.md)).
