---
description: The one block of keybindings Claude Code needs, and what else dictation needs on the Mac.
---

# Claude Code setup

Dictation, effort and `/clear` take one block in `keybindings.json`,
in Claude Code's config directory: `~/.claude`, or what `CLAUDE_CONFIG_DIR` names.
It goes into the file's `bindings` array, or its lines into a `Chat` block already there,
and the file reloads by itself:

```json
{
  "context": "Chat",
  "bindings": {
    "ctrl+q": "voice:pushToTalk",
    "ctrl+x ctrl+f": "command:effort high",
    "ctrl+x ctrl+l": "command:clear",
    "ctrl+x ctrl+x": "command:effort max"
  }
}
```

The device sends Ctrl chords rather than typed commands,
since a Ctrl chord arrives the same under any keyboard layout.
Enter, the arrows and Esc need nothing configured.
Without the block, the chords leave the prompt and the conversation as they are.

Tried on Claude Code 2.1.311.
Its schema has no space in a command's name,
so an editor validating against `$schema` marks the two effort lines;
they run all the same.

## Dictation

Ctrl+Q dictates from the Mac's own keyboard too,
since `voice:pushToTalk` takes one key.
Dictation also needs `/voice hold`, once,
and key repeat on, as macOS has it by default:
Claude Code tells a held key by its repeats,
and without them a recording ends about 2.2 s into the hold.
