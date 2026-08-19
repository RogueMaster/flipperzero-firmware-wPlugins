# Gallery GIFs

How the animated screenshots in the README (`docs/img/web-<game>.gif`) are made.
They come from the **simulator**, which runs the real ESP engine (WASM) and the real
phone client — so a GIF captured there is pixel-identical to what a phone shows,
minus the phone bezel.

## The spec

| Property | Value |
|---|---|
| Width | 300 px (height lands at ~649 px, the phone panel's aspect) |
| Frame rate | 5 fps |
| Length | 2–5 s (10–27 frames) |
| File size | keep under ~300 KB |
| Framing | the full phone client viewport (one phone panel, header included) |
| Content | a short **story**: setup → action → payoff, ending on the reveal/winner |

A GIF is a sequence of game *states*, not a screen recording. You screenshot each key
state once, then repeat frames to control how long each state holds (the payoff holds
longest, ~1.5 s).

## Capture

1. Build and serve:

   ```sh
   cd web && node build.mjs      # fresh phone client
   cd .. && sim/engine/build.sh  # plain build (skip --asan; it's slow in a browser)
   sim/serve.sh                  # -> http://localhost:8123/sim/web/
   ```

2. Stage the table. Add phones (**+ phone**), give each a clean nickname before
   tapping Play (ALICE, BOB, CARA, ... reads much better than saved TIGER76-style
   names), hit **Load packs**, then pick the game in the Flipper panel.

   > Pick the game *after* loading packs — a pack list loaded later doesn't refresh
   > an already-open lobby. Re-pick the game if the pack vote shows up empty.

3. Play to each key state and screenshot **one consistent phone panel** at every
   state. Any capture works; two that give exact pixels with no cropping:
   - DevTools → right-click the phone `<iframe>` element → *Capture node screenshot*.
   - Scripted: a headless browser screenshotting `#phones .phone:nth-child(N) iframe`
     (each panel exposes the client's `A` object, and `A.ws.send(...)` accepts every
     intent — `sim/test/<game>.mjs` documents each game's intent shapes).

4. Watch the text in every frame. The sim renders the real client, so a broken
   string in a frame is a real client bug (this workflow caught a double-encoded
   em dash in the Werewolf reveal).

### Staging notes per game

- **Werewolf** needs 8 players for a night-1 hunt (with fewer, the first night is a
  quiet meet-the-pack). The night runs a fixed 60 s; the day vote ends the moment a
  majority lands. Best arc: the wolf's own screen, night pick → day vote → dusk reveal.
- **Spyfall** needs packs. Best arc: the spy's card → the questioning options → the
  reveal after the spy calls the location.
- **Fill the Blank** rounds auto-advance on a timer — move briskly or the frames span
  different rounds/czars. Best arc: a hand → card played → the reveal naming every card.
- **Draw a Monster** is easiest with exactly 3 players (3 sheets to fill). Everyone
  tapping **Next** advances the round early, so you control the pace. Best arc: blank
  head canvas → head → the sliver handoff → the assembled creature in the gallery.
- **Secrets** needs a pack vote before it will start. The reveal is only
  `SECRETS_REVEAL_MS` (5 s) and the answer/predict windows are 30 s safety timers, so
  drive answer → predict → reveal in one uninterrupted pass; stop to read a file in
  between and the round times out with everyone defaulted to "no". Step the prediction
  with `#sec-plus` / read `#sec-num` — a lookup by the button's "+" text finds the wrong
  element. Best arc: lobby pack vote → the yes/no question → the stepper → the reveal
  with the histogram of who guessed what.
- **Reaction Duel** is the one game whose payoff frame is hard to catch, because green
  lasts 6 s at most and the round settles the moment every player has tapped. Trick:
  when green lands, tap from every phone EXCEPT the one you are screenshotting. A winner
  now exists, so the no-winner timeout never fires, and the round cannot settle until
  your phone taps — the green pad holds indefinitely. Tap it yourself afterwards. For a
  believable reaction time in the reveal, script the taps in-page (a CLI round trip puts
  seconds on the clock) and burst-screenshot across a whole session to catch one.
- **The 1v1 board duels** (Connect Four, Tic-Tac-Toe, Dots & Boxes) start with
  `{t:"challenge",to:pid}` then `{t:"accept",from:pid}`, and moves are `{t:"move",n}`.
  The win screen REPLACES the board with WIN/REMATCH, so there is no frame showing the
  winning line — let the last board frame carry the near-win and hold the WIN screen
  after it.
- **Dots & Boxes** edges: `0..29` horizontal (`r*5+c`, r 0-5), `30..59` vertical
  (`30 + r*6+c`, r 0-4). Sending the same edge from BOTH duellists needs no turn
  tracking: the engine ignores the player whose turn it isn't, and an already-drawn edge
  is a no-op.
- **Word Scramble** needs a pack vote too. The input is `#scr-input` and the intent is
  `{t:"guess",text:"WORD"}`. Best arc: the jumbled letters → the word typed in → the
  ANSWER reveal.

## Assemble

Duplicate each state's PNG to set its hold time, then encode with a palette (keeps
the flat UI colors crisp and the file small):

```sh
i=0
dup(){ for _ in $(seq 1 $2); do printf -v n "seq%03d.png" $i; cp "$1" "$n"; i=$((i+1)); done; }
dup state1.png 4     # opening
dup state2.png 3     # middle states, 2-4 frames each
dup state3.png 7     # payoff holds longest
ffmpeg -framerate 5 -i seq%03d.png \
  -vf "scale=300:-1:flags=lanczos,split[s0][s1];[s0]palettegen=stats_mode=diff[p];[s1][p]paletteuse=dither=bayer:bayer_scale=3" \
  -loop 0 web-<game>.gif
```

## Ship

Drop the file in `docs/img/` and add it to the matching gallery row in the README
(5 images per row, `width="19%"`, an `alt` that narrates the arc).
