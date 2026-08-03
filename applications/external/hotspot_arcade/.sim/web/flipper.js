// Data-faithful Flipper panel: renders ONLY what crosses the UART, and its controls
// send ONLY the commands the real Flipper sends. Deliberately not a pixel copy of
// the 128x64 screen — reimplementing the scenes would be a second implementation
// of the Flipper UI, the same drift trap the WASM engine exists to avoid.
import { engine, subscribeUart } from "./harness.js";
import { loadGamePacks, LANGS, resolvePacks } from "./trivia-packs.js";

// What the real Flipper streams: every packs/<game>/ directory, tagged with its game
// id. Mirrors ha_content_stream_packs in ha_session.c so the sim exercises all four
// content games, not just trivia.

// Ids copied verbatim from flipper/hotspot-arcade/ha_proto.h (HA_GAME_*).
const GAMES = [
  ["None", 0], ["Trivia", 1], ["Connect Four", 2], ["Tic-Tac-Toe", 3],
  ["Dots & Boxes", 4], ["Draw & Guess", 5], ["Pong", 6], ["Reaction Duel", 7],
  ["Would You Rather", 8], ["Word Scramble", 9], ["Reversi", 10],
  ["Guess the Color", 11], ["Battleship", 12], ["Spectrum", 13],
  ["Kiss Marry Kill", 14],
];

const players = new Map(); // pid -> { nick, score }
const feed = [];

// Nicknames (and anything spliced from engine event/round JSON) are user-supplied and
// reach us unescaped: the phone's maxlength is client-side only, the engine truncates
// without escaping, and ha_sim.cpp's esc() only JSON-escapes quotes/backslashes/control
// chars, not `<`/`>`. Escape at render time so a crafted nickname can't inject markup.
function escapeHtml(s) {
  return String(s).replace(
    /[&<>"']/g,
    (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" })[c],
  );
}

function render() {
  const roster = [...players.entries()]
    .sort((a, b) => b[1].score - a[1].score)
    .map(([pid, p]) => `<tr><td>${pid}</td><td>${escapeHtml(p.nick)}</td><td>${p.score}</td></tr>`)
    .join("");
  document.getElementById("roster").innerHTML =
    roster || `<tr><td colspan="3" class="muted">no players</td></tr>`;
  document.getElementById("feed").innerHTML = feed
    .slice(-40)
    .reverse()
    .map((l) => `<div class="line">${escapeHtml(l)}</div>`)
    .join("");
}

subscribeUart((it) => {
  if (it.kind === "join") players.set(it.pid, { nick: it.nick, score: 0 });
  else if (it.kind === "leave") players.delete(it.pid);
  else if (it.kind === "score") {
    const p = players.get(it.pid);
    if (p) p.score += it.delta;
    feed.push(`SCORE ${it.pid} ${it.delta > 0 ? "+" : ""}${it.delta} (${it.reason})`);
  } else if (it.kind === "event") feed.push(`EVENT ${JSON.stringify(it.json)}`);
  else if (it.kind === "round") feed.push(`ROUND ${JSON.stringify(it.json)}`);
  render();
});

export async function mountFlipper(el) {
  el.innerHTML = `
    <div class="row">
      <select id="game">${GAMES.map(([n, v]) => `<option value="${v}">${n}</option>`).join("")}</select>
      <button id="round">Round end</button>
      <button id="zero">Reset scores</button>
    </div>
    <div class="row"><button id="packs">Load packs</button>
      <select id="lang"><option value="">English</option><option value="pt-br">Portugues (BR)</option></select>
      <span id="packstate" class="muted">no packs loaded</span></div>
    <table class="roster"><thead><tr><th>pid</th><th>nick</th><th>score</th></tr></thead>
      <tbody id="roster"></tbody></table>
    <h3>Event feed</h3>
    <div id="feed" class="feed"></div>`;

  document.getElementById("game").onchange = (e) =>
    engine.selectGame(Number(e.target.value));
  document.getElementById("round").onclick = () => engine.roundEnd();
  document.getElementById("zero").onclick = () => {
    engine.resetScores();
    for (const p of players.values()) p.score = 0;
    render();
  };
  document.getElementById("packs").onclick = async () => {
    engine.contentClear();
    const lang = document.getElementById("lang").value; // "" = English
    let packCount = 0;
    let itemCount = 0;
    // resolvePacks applies the host's per-game English fallback for the chosen language.
    for (const g of resolvePacks(lang)) {
      const loaded = await loadGamePacks(engine, g.game, g.dir, g.names, g.sub);
      packCount += loaded.length;
      itemCount += loaded.reduce((n, p) => n + p.count, 0);
    }
    const label = lang && LANGS[lang] ? lang : "en";
    document.getElementById("packstate").textContent =
      `${packCount} packs, ${itemCount} items (${label})`;
  };

  // Language also drives the phone UI: the ESP echoes it in `welcome`, so new joiners
  // get it. Apply it to already-connected phones directly (a sim shortcut; on hardware
  // a phone picks it up when it joins).
  document.getElementById("lang").onchange = (e) => {
    const lang = e.target.value;
    engine.setLang(lang);
    for (const f of document.querySelectorAll("iframe")) {
      const w = f.contentWindow;
      if (w && w.A && w.A.setLang) w.A.setLang(lang);
    }
  };
  render();
}
