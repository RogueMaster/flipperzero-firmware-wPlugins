# Hotspot Arcade - phone web client

The phone-facing client for Hotspot Arcade: an offline multiplayer game system
served from an ESP32-S2 WiFi board. Phones join an open WiFi AP, a captive page
hands off to this app in the real browser, and they play over a WebSocket to the
ESP. No internet, no CDNs, no frameworks.

## Structure

The source is split for readability; the build inlines everything into one file.

```
src/index.html      HTML shell with /*__CSS__*/ and /*__JS__*/ placeholders
core/style.css      all styles (Flipper Zero design system, see DESIGN.md)
core/app.js         WebSocket, router, landing + lobby, reconnect, shared `A` state
games/trivia.js     trivia screen  (registers A.handlers.trivia)
games/connect4.js   connect 4 screen (registers A.handlers.c4)
build.mjs           inlines + minifies -> dist/, gzips, writes manifest, size gate
mock-server.mjs     zero-dep local server for eyeballing the UI in a browser
```

Modules share a single global object `A` (state + `A.handlers` map) instead of a
bundler. `core/app.js` must be concatenated first; the game modules attach to it.

## Build

```
npm run build        # or: node build.mjs
```

Produces:

```
dist/index.html      single self-contained file (inlined CSS + JS)
dist/index.html.gz   gzip -9, what the ESP actually streams
dist/manifest.json   [{"path":"/","file":"index.html.gz","mime":"text/html","gzip":true}]
```

The build prints raw + gzipped sizes and **exits non-zero if the gzipped bundle
exceeds 60 KB** (soft target 30 KB). Current bundle: ~7 KB gzipped.

## Visual style

The UI follows the Flipper Zero design system documented in `DESIGN.md`:
monochrome near-black surfaces with a single hot orange (#FF8200) accent,
monospace uppercase "tool" typography, sharp 2px borders (no rounded/gradient/
glassy styling), a blinking orange block cursor in the wordmark, a faint CSS
scanline overlay, and big touch targets. Connect Four discs stay on-brand: yours
are orange, the opponent's are off-white. All offline: no web fonts or images,
only CSS shapes and unicode blocks. Motion honors `prefers-reduced-motion`.

## Local testing

```
npm run build
npm run mock         # serves the built page + a mock WS at http://localhost:8080
```

Open `http://localhost:8080` in a browser (resize to a phone width). The WS URL
derives from `location.host`, so the same bundle talks to the mock locally and to
`ws://192.168.4.1/ws` on the ESP with no code change. The mock scripts a demo
sequence (lobby -> trivia question/reveal -> Connect 4 challenge/match) so every
screen renders; it is a demo, not the referee, and does not implement real game
rules. Set `PORT` to change the port: `PORT=8091 npm run mock`.

## Protocol

Client -> server: `hello`, `answer`, `challenge`, `accept`, `cancel`, `move`,
`leaveGame`, `ping`. Server -> client: `welcome`, `lobby`, `trivia`, `c4`,
`toast`, `pong`. The ESP is authoritative; the client sends intents and renders
server state. The trivia countdown is cosmetic: the client learns the server
clock offset from the first timed message (`deadline` is a server `millis()`
value) and animates a bar toward the deadline. The server is the real referee.

## Captive-portal handoff (limitation)

When a phone joins the AP, iOS/Android open a captive **mini-browser** (CNA on
iOS, the "Sign in to network" assistant on Android). That mini-browser is
sandboxed: WebSocket support is unreliable and it may close on backgrounding. The
landing screen therefore shows a prominent **"Open in browser"** link to
`http://192.168.4.1/`, which most captive assistants honor by handing off to the
real system browser (Safari/Chrome), where the WebSocket works reliably.

This is best-effort. There is no cross-platform API to force the handoff, so the
page also keeps a persistent hint ("If this page closes, rejoin the WiFi and open
192.168.4.1 in your browser.") for the case where a user lands back on the AP.
The nickname is persisted to `localStorage` so reopening in the real browser
rejoins automatically.

## No em-dashes

User-facing copy uses commas or periods, never em-dashes.
