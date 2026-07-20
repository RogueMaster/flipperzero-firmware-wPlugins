# Hotspot Arcade — Design System (Flipper Zero flavored)

The phone web client borrows the Flipper Zero visual language: a **monochrome,
pixel/retro, tool-like** look with a single hot **orange** accent. Think of the
qFlipper desktop app and the device's 1-bit LCD, translated to a dark, touch
friendly mobile UI. Everything here is **offline and tiny** (no external fonts,
images, or CDNs), so the system is expressed in CSS variables + simple recipes.

Design intent: it should feel like the game is running *on a Flipper*, not like a
generic web app. Sharp edges, chunky bordered panels, uppercase mono labels, a
blinking block cursor, one confident orange.

---

## 1. Color

Brand orange is the only saturated hue that carries meaning by default. Greens/
reds/cyan appear **only** as functional game-state signals, used sparingly.

```css
:root {
  /* Brand */
  --orange:        #FF8200;  /* Flipper orange — primary accent, CTAs */
  --orange-bright: #FF9E29;  /* hover / active / focus glow */
  --orange-dim:    #B85E06;  /* pressed, muted orange text */

  /* Surfaces (qFlipper dark) */
  --bg:        #0B0B0C;      /* app background, near-black */
  --surface:   #161618;      /* cards, panels */
  --surface-2: #202024;      /* raised rows, inputs */
  --line:      #2E2E33;      /* 1-2px borders, dividers */
  --line-hot:  var(--orange);/* active/selected border */

  /* Text */
  --text:      #F2F2EE;      /* primary, warm off-white */
  --text-dim:  #9A9A94;      /* secondary/labels */
  --text-mute: #5E5E5A;      /* disabled/hints */
  --on-orange: #0B0B0C;      /* text on an orange fill (near-black) */

  /* Functional signals (game state only) */
  --good:  #46D369;          /* correct answer, you win */
  --bad:   #FF4D4D;          /* wrong answer, you lose */
  --info:  #34C6DE;          /* neutral highlight, opponent */

  /* Connect Four discs stay monochrome+orange, on brand */
  --disc-you: var(--orange); /* your discs */
  --disc-opp: #EDEDE6;       /* opponent discs (off-white) */
}
```

Rules of thumb:
- Backgrounds are dark; orange is for the ONE thing you want tapped or noticed.
- Never put dim text on orange. Text on orange is `--on-orange` (near-black).
- Selected/active state = orange 2px border (and/or orange text), not a fill swap,
  except for the primary button which is a solid orange fill.

---

## 2. Typography

Monospace throughout for the "tool/terminal" feel (system fonts, zero download):

```css
--font: ui-monospace, "SF Mono", "Menlo", "Consolas", "Roboto Mono", monospace;
```

- **Wordmark / screen titles:** UPPERCASE, `letter-spacing: 0.12em`, weight 700.
- **Headings (h1/h2):** uppercase, 0.08em tracking.
- **Body / scores / questions:** normal case, weight 400-600.
- **Labels / captions:** uppercase, `--text-dim`, 11-12px, 0.1em tracking.
- Sizes (mobile-first): title 20-24px, question 20px, body 15-16px, button 16-18px,
  caption 12px. Keep line-height ~1.35.
- **Block cursor accent:** append a blinking `▐` (or a 0.6em-wide orange box) after
  the wordmark for the device feel. Animate opacity 1→0 at ~1s steps (step-end),
  respect `prefers-reduced-motion` (no blink).

---

## 3. Shape, border, elevation

Flipper UI is pixel-rectangular. Keep edges sharp and borders chunky.

```css
--radius: 2px;      /* max softness; panels/board cells use 0 */
--border: 2px solid var(--line);
--gap: 8px;
```

- **No drop shadows / no gradients for depth.** Elevation is shown by a lighter
  surface + a border, not blur. (One optional exception: an orange `box-shadow`
  *glow* on the active/pressed CTA, e.g. `0 0 0 2px var(--orange-bright)`.)
- Panels: `--surface` fill, `--border`, `--radius`.
- Focus/selected: swap border to `--orange` (2px). Keep a visible focus ring for
  keyboard users.

### Optional CRT texture (cheap, pure CSS)
A very subtle scanline overlay sells the retro feel without assets. Keep it faint
so it never hurts readability, and disable under reduced-motion is not needed (it's
static). Use at <= 6% opacity:

```css
.crt::after{
  content:""; position:fixed; inset:0; pointer-events:none;
  background:repeating-linear-gradient(180deg,
    rgba(255,255,255,.04) 0 1px, transparent 1px 3px);
}
```

---

## 4. Spacing & layout

- 4px base scale: 4 / 8 / 12 / 16 / 24 / 32.
- Mobile-first single column, `max-width: 480px` centered, `padding: 16px`.
- Sticky top **header bar**: wordmark left, connection dot + nickname right,
  bottom `--border`. Height ~48px.
- Generous vertical rhythm between the header, the "screen" content, and actions.
- Safe areas: pad for notches with `env(safe-area-inset-*)`.

---

## 5. Components (recipes)

**Header bar**
- `HOTSPOT ARCADE` wordmark (uppercase, tracked) + blinking orange block.
- Right: a small round status dot — orange (connected), `--text-mute` (reconnecting),
  `--bad` (disconnected) — plus the player's nick in `--text-dim`.

**Primary button (CTA)** — the main action per screen.
- Solid `--orange` fill, `--on-orange` text, uppercase, weight 700, `--radius`.
- Full width, min-height 52px (big touch target). Pressed: `--orange-dim` fill +
  translateY(1px). Disabled: `--surface-2` fill, `--text-mute`.

**Secondary button** — outline: transparent fill, 2px `--line` border, `--text`.
Active border → `--orange`.

**Panel / card** — `--surface`, `--border`, 12-16px padding.

**List row (players / challenges)** — `--surface-2`, 2px bottom `--line`, 48px min
height, name left, score/badge right, tap target full-width. Selected row: left
3px orange bar or orange border.

**Badge / pill** — small uppercase mono chip; score badges use `--orange` text on
`--surface-2`; rank #1 gets a solid orange pill.

**Trivia answer buttons (A/B/C/D)** — a 1- or 2-column grid of large tiles (min
64px tall). Each tile: letter chip (A-D) left, option text. Default: `--surface-2`
+ `--border`. Your pick (locked): orange border + orange letter chip. On reveal:
correct tile border/letter → `--good`; if your wrong pick → `--bad`; show the count
per option as a small dim number. Disable all after you answer.

**Countdown bar** — thin (6px) full-width track `--surface-2` with an `--orange`
fill that shrinks to 0 over the question `dur` (drive width by `deadline - now`).
Turns `--bad` in the last ~3s. No fill after reveal.

**Connect Four board** — 7×6 grid on `--surface`, cell borders `--line`, cells are
squares (aspect-ratio 1). Empty = `--bg` circle inset; your discs = `--disc-you`;
opponent = `--disc-opp`. Column tap targets span the full column height; disable +
dim columns when it is not your turn. Whose-turn banner uses orange when it's you.

**Toast** — bottom, `--surface-2`, 2px `--orange` left border, auto-dismiss ~2.5s.

**Result screen** — big uppercase `WIN` (`--good`) / `LOSE` (`--bad`) / `DRAW`
(`--text-dim`), then a secondary "Back to lobby" button.

---

## 6. Motion

Snappy and mechanical, never floaty.
- Transitions 90-140ms, `ease-out` (or steps for the cursor). Presses feel instant.
- Disc drop: a short 120-180ms translateY into place is fine; keep it crisp.
- Honor `@media (prefers-reduced-motion: reduce)`: drop the blink and disc drop,
  keep state changes instant.

---

## 7. Do / Don't

- DO keep it monochrome + one orange; DON'T rainbow the UI.
- DO use uppercase mono labels and sharp borders; DON'T add rounded pill-y,
  gradient, glassy "SaaS" styling.
- DO make tap targets >= 48px; DON'T rely on hover (it's a phone).
- DO keep the bundle tiny (this is streamed into ESP RAM); DON'T add web fonts,
  icon fonts, or images. Use CSS shapes / unicode blocks (▐ ● ■ ◍) instead.
- DO test both in a dark room and bright sunlight (bus window): ensure orange-on
  dark and off-white text stay legible.
