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
//
// manifest.json can't be byte-compared for exactly the same reason: it carries a crc32
// OF THE GZ, so it moves with the builder's zlib even when the content is identical.
// Comparing it verbatim failed every PR opened against a bundle built elsewhere, whatever
// the PR touched. Check the STRUCTURE instead (crc stripped) plus that the committed crc
// really describes the committed gz -- the same rule build.yml's "bundled-assets" job
// already uses, which this script was never brought in line with.
const crc32 = (b) => {
  let c = 0xffffffff;
  for (let i = 0; i < b.length; i++) {
    c ^= b[i];
    for (let k = 0; k < 8; k++) c = c & 1 ? (c >>> 1) ^ 0xedb88320 : c >>> 1;
  }
  return (c ^ 0xffffffff) >>> 0;
};
// JSON round-trip, so this also normalises whitespace and CRLF between the two copies.
const stripCrc = (s) => JSON.stringify(JSON.parse(s).map(({ crc, ...rest }) => rest));
const crcOf = (s) => (JSON.parse(s).find((a) => a.path === "/") || {}).crc;

let bundleOk = false, bundleDetail = "";
try {
  const committedGz = execSync("git show HEAD:web/dist/index.html.gz", { cwd: REPO });
  const committedHtml = gunzipSync(committedGz).toString();
  const committedManifest = sh("git show HEAD:web/dist/manifest.json");
  sh("node web/build.mjs"); // overwrites web/dist in the working tree
  const builtHtml = gunzipSync(readFileSync(REPO + "web/dist/index.html.gz")).toString();
  const structureOk = stripCrc(committedManifest) === stripCrc(read("/web/dist/manifest.json"));
  const want = crc32(committedGz);
  const crcOk = crcOf(committedManifest) === want;
  bundleOk = committedHtml === builtHtml && structureOk && crcOk;
  bundleDetail = bundleOk ? "up to date"
    : committedHtml !== builtHtml ? "the built bundle differs from web/dist — run `node web/build.mjs` and commit"
      : !structureOk ? "web/dist/manifest.json is stale — run `node web/build.mjs` and commit"
        : `web/dist/manifest.json crc ${crcOf(committedManifest)} does not describe web/dist/index.html.gz (${want}) — run \`node web/build.mjs\` and commit`;
} catch (e) {
  bundleDetail = "could not check the web bundle: " + e.message.split("\n")[0];
}
H(bundleOk, "web/dist matches `node web/build.mjs` (decompressed)", bundleDetail);

// ---- HARD: the fap's bundled web copy matches web/dist ----
// flipper/hotspot-arcade/assets/web/ is the copy the Flipper actually streams to the ESP
// and serves to phones -- a SEPARATE committed build output from web/dist. When it went
// stale, a newly added game looked completely broken on hardware: the served page had no
// screen to route to and no handler for the game's state pushes, so phones just sat in
// the lobby forever. Mirrors the build.yml "bundled-assets" job so it fails here first.
//
// web/dist was rebuilt in the working tree by the check above, so its manifest now holds
// THIS machine's crc; compare structure and check the bundled crc against the bundled gz.
let webAssetOk = false, webAssetDetail = "";
try {
  const dist = gunzipSync(readFileSync(REPO + "/web/dist/index.html.gz")).toString();
  const bundledGz = readFileSync(REPO + "/flipper/hotspot-arcade/assets/web/index.html.gz");
  const bundled = gunzipSync(bundledGz).toString();
  const bundledManifest = read("/flipper/hotspot-arcade/assets/web/manifest.json");
  const structureSame = stripCrc(read("/web/dist/manifest.json")) === stripCrc(bundledManifest);
  const want = crc32(bundledGz);
  const crcOk = crcOf(bundledManifest) === want;
  webAssetOk = dist === bundled && structureSame && crcOk;
  webAssetDetail = webAssetOk ? "up to date"
    : dist !== bundled
      ? "assets/web/index.html.gz is stale — run `tools/build-fap.sh` (or copy web/dist/*.gz over) and commit"
      : !structureSame
        ? "assets/web/manifest.json is stale — run `tools/build-fap.sh` and commit"
        : `assets/web/manifest.json crc ${crcOf(bundledManifest)} does not describe assets/web/index.html.gz (${want}) — run \`tools/build-fap.sh\` and commit`;
} catch (e) {
  webAssetDetail = "could not check the bundled web asset: " + e.message.split("\n")[0];
}
H(webAssetOk, "flipper assets/web matches web/dist (decompressed)", webAssetDetail);

// ---- HARD: i18n catalog integrity ----
// Every t("key") the client calls must resolve in the English catalog — a missing key
// leaks the raw key string to players. pt-BR (or any non-en) gaps are advisory: they
// fall back to English by design, so they're reported in the detail, not failed.
// (Only direct t("literal") calls are checked; keys passed through a variable — e.g.
// kmk.js's LBL[] array — are not, but they're literals in the source and rarely drift.)
let i18nOk = true, i18nDetail = "";
try {
  const cat = read("/web/core/i18n.js");
  const parts = cat.split('"pt-br"');
  const keysOf = (s) => new Set([...s.matchAll(/"([a-z][a-z0-9]*\.[a-z0-9_]+)":/g)].map((m) => m[1]));
  const en = keysOf(parts[0]);
  const pt = keysOf('"pt-br"' + (parts[1] || ""));
  const refs = new Set();
  for (const f of sh("git ls-files web/games web/core").split("\n")) {
    if (!/\.js$/.test(f) || /i18n\.js$/.test(f)) continue;
    for (const m of read("/" + f).matchAll(/(?<![A-Za-z0-9_$])t\(\s*"([^"]+)"/g)) {
      if (/^[a-z]+\./.test(m[1])) refs.add(m[1]); // namespaced keys only
    }
  }
  const missingEn = [...refs].filter((k) => !en.has(k));
  const missingPt = [...en].filter((k) => !pt.has(k));
  i18nOk = missingEn.length === 0;
  i18nDetail = !i18nOk
    ? `missing en keys: ${missingEn.join(", ")}`
    : `${en.size} keys` + (missingPt.length
      ? `, pt-br missing ${missingPt.length} (falls back to en): ${missingPt.slice(0, 6).join(", ")}${missingPt.length > 6 ? "…" : ""}`
      : ", pt-br in sync");
} catch (e) {
  i18nDetail = "could not check the i18n catalog: " + e.message.split("\n")[0];
}
H(i18nOk, "i18n catalog: every client t() key resolves (en)", i18nDetail);

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
