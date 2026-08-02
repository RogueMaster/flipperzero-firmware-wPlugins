#!/usr/bin/env node
// PR checklist reviewer. Runs the deterministic checks from CONTRIBUTING.md and prints a
// Markdown report to stdout. Exits non-zero if a HARD check fails (proto drift, stale
// bundle); "adding a game" items are advisory (a warning, never a hard fail — the
// maintainer decides what a given PR actually needs).
//
//   node tools/pr-check.mjs            # diff against origin/master
//   BASE=origin/main node tools/pr-check.mjs
//
// CI passes BASE; locally it defaults to origin/master (falls back to master).
import { execSync } from "node:child_process";
import { readFileSync, existsSync } from "node:fs";
import { gunzipSync } from "node:zlib";

const REPO = new URL("..", import.meta.url).pathname;
const sh = (c) => execSync(c, { cwd: REPO, encoding: "utf8" }).trim();
const shOk = (c) => { try { sh(c); return true; } catch { return false; } };
const read = (p) => (existsSync(REPO + p) ? readFileSync(REPO + p, "utf8") : "");

// Resolve the base ref to diff the PR against. BASE is interpolated into git commands,
// so restrict it to safe ref characters (no shell metacharacters).
let BASE = process.env.BASE || "origin/master";
if (!/^[\w./@+-]+$/.test(BASE)) { console.error("Invalid BASE ref"); process.exit(2); }
if (!shOk(`git rev-parse --verify ${BASE}`)) BASE = shOk("git rev-parse --verify master") ? "master" : "";
const changed = BASE ? sh(`git diff --name-only ${BASE}...HEAD`).split("\n").filter(Boolean) : [];
const touched = (re) => changed.some((f) => re.test(f));

const hard = []; // { ok, label, detail }
const soft = [];
const H = (ok, label, detail = "") => hard.push({ ok, label, detail });
const S = (ok, label, detail = "") => soft.push({ ok, label, detail });

// ---- HARD: ha_proto.h consistency (the two headers must agree) ----
const espProto = read("/esp32/hotspot-arcade-fw/ha_proto.h");
const flpProto = read("/flipper/hotspot-arcade/ha_proto.h");
const fwOf = (s) => (s.match(/#define\s+HA_FW_VERSION\s+(\d+)/) || [])[1];
const gamesOf = (s) => {
  const m = {};
  for (const line of s.split("\n")) {
    const g = line.match(/HA_GAME_(\w+)\s*=?\s*(\d+)/);
    if (g && g[1] !== "NONE") m[g[1]] = g[2];
  }
  return m;
};
const espFw = fwOf(espProto), flpFw = fwOf(flpProto);
H(espFw && espFw === flpFw, "HA_FW_VERSION matches in both ha_proto.h",
  espFw === flpFw ? `v${espFw}` : `esp=${espFw} flipper=${flpFw}`);
const espGames = gamesOf(espProto), flpGames = gamesOf(flpProto);
const gamesEqual = JSON.stringify(espGames) === JSON.stringify(flpGames);
H(gamesEqual, "HA_GAME_* ids match in both ha_proto.h",
  gamesEqual ? `${Object.keys(espGames).length} games` : "id lists differ between the two headers");

// ---- HARD: web bundle is rebuilt & committed ----
// Compare the DECOMPRESSED bundle, not the raw .gz: gzip container bytes vary by the
// platform's zlib, so a raw-byte check false-fails when the committer built on a
// different OS. The decompressed HTML is what actually differs when the bundle is stale.
let bundleOk = false, bundleDetail = "";
try {
  const committedHtml = gunzipSync(execSync("git show HEAD:web/dist/index.html.gz", { cwd: REPO })).toString();
  sh("node web/build.mjs"); // overwrites web/dist in the working tree
  const builtHtml = gunzipSync(readFileSync(REPO + "web/dist/index.html.gz")).toString();
  const manifestStale = sh("git status --porcelain -- web/dist/manifest.json").length > 0;
  bundleOk = committedHtml === builtHtml && !manifestStale;
  bundleDetail = bundleOk ? "up to date"
    : committedHtml !== builtHtml ? "the built bundle differs from web/dist — run `node web/build.mjs` and commit"
      : "web/dist/manifest.json is stale — run `node web/build.mjs` and commit";
} catch (e) {
  bundleDetail = "could not check the web bundle: " + e.message.split("\n")[0];
}
H(bundleOk, "web/dist matches `node web/build.mjs` (decompressed)", bundleDetail);

// ---- Detect a new game (a HA_GAME_* id present in HEAD but not in BASE) ----
let baseGames = {};
if (BASE) {
  try { baseGames = gamesOf(sh(`git show ${BASE}:esp32/hotspot-arcade-fw/ha_proto.h`)); } catch { /* new file */ }
}
const newGames = Object.keys(espGames).filter((g) => !(g in baseGames));
const addingGame = newGames.length > 0;

if (addingGame) {
  // HARD: a new game must bump the firmware version.
  const baseFw = BASE ? fwOf(sh(`git show ${BASE}:esp32/hotspot-arcade-fw/ha_proto.h`) || "") : null;
  H(!baseFw || Number(espFw) > Number(baseFw), "HA_FW_VERSION bumped for the new game",
    baseFw ? `${baseFw} -> ${espFw}` : "");

  // SOFT: the wiring + docs checklist (presence in the PR diff).
  S(touched(/ha_games\.h$/), "engine wired in ha_games.h");
  S(touched(/scene_game_select\.c$/), "Flipper game picker (game_select.c)");
  S(touched(/scene_lobby\.c$/), "Flipper lobby name (lobby.c)");
  S(touched(/web\/games\/.+\.js$/), "phone client module (web/games/*.js)");
  S(touched(/web\/build\.mjs$/), "client registered in web/build.mjs");
  S(touched(/web\/core\/app\.js$/), "screen/label registered in web/core/app.js");
  S(touched(/web\/src\/index\.html$/), "screen markup in web/src/index.html");
  S(touched(/web\/core\/style\.css$/), "styles in web/core/style.css");
  S(touched(/sim\/web\/flipper\.js$/), "simulator GAMES list (sim/web/flipper.js)");
  S(touched(/sim\/test\/.+\.mjs$/), "a headless test under sim/test/");
  S(touched(/docs\/PROTOCOL\.md$/), "protocol section (docs/PROTOCOL.md)");
  S(touched(/^README\.md$/), "README updated (count/gallery)");
  S(touched(/CHANGELOG\.md$/), "CHANGELOG entry");
  S(touched(/catalog\/DESCRIPTION\.md$/), "catalog DESCRIPTION.md");
  S(touched(/docs\/img\/.+\.(gif|png)$/), "a screenshot or GIF (docs/img/)");
}

// ---- Render ----
const mark = (ok) => (ok ? "✅" : "❌");
const wmark = (ok) => (ok ? "✅" : "⚠️");
let out = "## PR checklist\n\n";
if (!BASE) out += "> Could not resolve a base ref; diff-based checks were skipped.\n\n";
out += "**Required**\n\n";
for (const c of hard) out += `- ${mark(c.ok)} ${c.label}${c.detail ? ` — ${c.detail}` : ""}\n`;
if (addingGame) {
  out += `\n**Adding a game** (detected new id: ${newGames.map((g) => "`HA_GAME_" + g + "`").join(", ")}) — advisory\n\n`;
  for (const c of soft) out += `- ${wmark(c.ok)} ${c.label}\n`;
  out += "\n<sub>Advisory items are reminders, not blockers. Tick anything that genuinely doesn't apply.</sub>\n";
} else {
  out += "\n<sub>No new game id detected — game-wiring checklist skipped.</sub>\n";
}

const hardFail = hard.some((c) => !c.ok);
out += `\n---\n${hardFail ? "❌ Required checks failed." : "✅ Required checks passed."}`;
console.log(out);
process.exit(hardFail ? 1 : 0);
