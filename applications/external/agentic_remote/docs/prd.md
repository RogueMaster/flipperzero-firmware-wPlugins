# Claude Remote — Product Requirements Document

## 1. Product Overview

**Product name:** Claude Remote
**Platform:** Flipper Zero (FAP app)
**Goal:**
Turn the Flipper Zero into:
1. A **one-handed remote** for controlling Claude Code (numbered approvals + voice input).
2. An **offline Claude Code manual**, browsable entirely on the Flipper.

The same app should let the user:
- Drive Claude Code using 1/2/3 + Enter + Voice from the D-pad.
- Read Claude Code docs, shortcuts, and best-practice "playbooks" when away from their Mac.

---

## 2. Primary Use Cases

1. **Remote agent control while watching the terminal:**
   - User is at their Mac, Claude Code is running.
   - Flipper is paired via Bluetooth as HID.
   - User uses the Flipper to answer numbered prompts (1/2/3) and trigger voice input and Enter.

2. **Hands-off voice prompting:**
   - User presses a "mic" button on the Flipper.
   - This sends the macOS dictation shortcut (e.g., F5).
   - User speaks; macOS handles transcription and insertion.
   - User hits Enter from the Flipper to send.

3. **Offline learning / reference:**
   - User is away from their Mac.
   - Opens the app's "Claude Manual".
   - Reads structured docs: keybindings, slash commands, skills, recipes.

---

## 3. Target Users

- AI-heavy developers using Claude Code as their main coding agent.
- Power users comfortable with terminal and hardware toys.
- People who want minimal friction to approve/guide autonomous agents.

---

## 4. Core Features

### 4.1 Remote Control Mode

**Behavior:**

- On launch, user sees a home screen with:
  - App name.
  - Button legend (1/2/3, Enter, Voice).
  - Hint: "Back = rotate / Menu = manual".

- Entering "Remote Control" mode:
  - The app starts sending HID keycodes over Bluetooth.
  - No extra pairing logic (rely on Flipper's HID controller capability).

**Button mapping (portrait, USB at bottom):**

| Button | Action | HID Keycode |
|--------|--------|-------------|
| Left | Send `1` | HID_KEYBOARD_1 |
| Up | Send `2` | HID_KEYBOARD_2 |
| Right | Send `3` | HID_KEYBOARD_3 |
| OK (center) | Enter (submit) | HID_KEYBOARD_RETURN |
| Down | Voice toggle | HID_KEYBOARD_F5 |

**Claude semantics assumed:**
- `1` = Yes / approve
- `2` = No / decline
- `3` = Other / alternative (when present)

**Orientation toggle:**
- Back (short press) toggles between portrait normal and portrait flipped (180).
- Physical-to-logical mapping inverts so user experience stays consistent.

### 4.2 Claude Manual Mode

An offline, structured manual fully browsable on Flipper.

**Sections:**
1. Getting Started — What Claude Code is, basic concepts
2. Keybindings — Default Mac keybindings, voice/dictation shortcuts
3. Slash Commands — Common commands with one-line explanations
4. Skills & Tools — What skills are, how they're invoked, examples
5. Playbooks (Recipes) — Step-by-step guides for common workflows

**Technical representation:**
- Plain text files on SD card: `/ext/apps_data/claude_remote/manual/01_*.txt` through `05_*.txt`
- App scans directory, orders by numeric prefix, renders with scrolling.
- Falls back to compiled-in defaults if SD card files are missing.

### 4.3 Updatable Docs

- App prefers SD card text files if present, compiled-in defaults otherwise.
- External tooling (Mac script) can regenerate `.txt` content from Claude Code docs.

---

## 5. Non-Functional Requirements

- **Latency:** HID key send <100ms from button press.
- **Battery:** No busy loops; event queue with idle timeout.
- **Robustness:** Show "Not connected" if HID not paired; never crash.
- **Portability:** Official Flipper APIs only.
- **UX:** Clear HUD with orientation and button legend. Clear manual navigation hints.

---

## 6. Out of Scope for v1

- Audio capture on Flipper.
- Local speech-to-text on Flipper.
- Multi-agent switching, macros, or tmux integration.
- Non-Claude agents.

---

## 7. User Stories

### Remote Control

1. As a Claude Code user, I want to press Left/Up/Right on my Flipper to send 1/2/3 to Claude Code so I can answer approval prompts without touching my keyboard.
2. As a user, I want the OK button to send Enter so I can submit prompts or confirmations from the Flipper.
3. As a user, I want the Down button to toggle macOS voice input (F5) so I can speak prompts like a TV remote mic button.
4. As a user, I want Back to rotate the UI and remap buttons for flipped portrait so I can use the remote in either orientation.
5. As a user, I want an on-screen legend showing which button sends 1/2/3, Enter, and Voice so I can learn the layout quickly.
6. As a user, I want the app to show a warning when Bluetooth HID is not connected so I understand why presses don't reach Claude Code.

### Manual / Docs

7. As a Claude Code user, I want to open a "Claude Manual" on my Flipper so I can read how Claude Code works offline.
8. As a user, I want sections like Getting Started, Keybindings, Slash Commands, Skills, and Playbooks so I can navigate docs like a small handbook.
9. As a user, I want to scroll text with Up/Down and change sections with Left/Right so reading feels natural on the Flipper.
10. As a user, I want manual content stored as text files on the SD card so I can update it without reflashing the app.
11. As a maintainer, I want chapters loaded in numeric-prefix order so I can restructure the manual by renaming files.
12. As a maintainer, I want compiled-in fallback pages so the app works out of the box.
