// Parses the trivia pack text format so the harness can feed packs to the engine
// the way the Flipper does.
//
// KNOWN DUPLICATION: ha_session.c's trivia_stream_pack (around lines 145-216) parses
// this same format in C. It is the only rule-ish logic implemented twice in this
// project (see the design doc). This function mirrors that C state machine line for
// line, on purpose, rather than taking the obviously-simpler "split on ---" shortcut:
// per packs/README.md line 39, blocks may be separated by blank lines alone,
// with no "---" at all, and the C parser has no notion of "---" as a delimiter — it
// just ignores it as an unrecognized line. A pack with no "---" would parse as one
// giant block under a naive splitter and silently yield zero questions.
//
// Output shape matches what Engine::triviaAddQ actually parses (verified against
// esp32/hotspot-arcade-fw/ha_games.h): options live under "o" and the correct index
// under "c" (NOT "a"/"correct" as an earlier draft of this file assumed).
export function parsePack(text, fallbackName = "") {
  const out = { name: "", questions: [] };

  // Topic name = the first "Pack:" line if present, else the caller's fallback
  // (mirrors trivia_stream_pack falling back to the filename around ha_session.c:170).
  for (const raw of text.split("\n")) {
    const line = raw.trim();
    if (line.startsWith("Pack:")) { out.name = line.slice(5).trim(); break; }
  }
  if (!out.name) out.name = fallbackName;

  // Stream questions the way the C loop does: a "Q:" line is the delimiter itself
  // (flush whatever was accumulated, then start fresh), not "---". Options are
  // indexed by letter (o[t[0]-'A'] in C) so out-of-order A-D lines still map to the
  // right slot, and A-D/Answer: lines are only honored once a "Q:" has been seen
  // (the `inQ` gate) so stray lines before the first question are ignored.
  let q = null;
  let gotOpts = 0;
  let gotAns = false; // mirrors trivia_stream_pack's gotAns gate in ha_session.c
  let inQ = false;

  const flush = () => {
    // Match the C parser: a question with no valid A-D answer, or fewer than
    // four options, is dropped rather than silently defaulted.
    if (inQ && gotOpts >= 4 && gotAns) out.questions.push(q);
  };

  for (const raw of text.split("\n")) {
    const line = raw.trim(); // trim() also strips a trailing \r, so CRLF is tolerated
    if (line.startsWith("Q:")) {
      flush();
      q = { q: line.slice(2).trim(), o: ["", "", "", ""], c: 0 };
      gotOpts = 0;
      gotAns = false;
      inQ = true;
    } else if (inQ) {
      const opt = /^([A-D]):(.*)$/.exec(line);
      if (opt) {
        q.o[opt[1].charCodeAt(0) - 65] = opt[2].trim();
        gotOpts++;
      } else if (line.startsWith("Answer:")) {
        const letter = line.slice(7).trim().toUpperCase().charAt(0);
        const idx = "ABCD".indexOf(letter);
        if (idx >= 0) { q.c = idx; gotAns = true; }
      }
    }
  }
  flush(); // the last block has no following "Q:" to trigger its flush

  return out;
}

/** Load the repo's sample trivia packs through the harness's HTTP server. */
export async function loadSamplePacks(names = ["general", "movies", "science"]) {
  const packs = [];
  for (const n of names) {
    try {
      const res = await fetch(`../../packs/trivia/${n}.txt`);
      if (res.ok) packs.push(parsePack(await res.text(), n));
      else console.warn(`trivia pack "${n}" failed to load: HTTP ${res.status}`);
    } catch (e) { console.warn(`trivia pack "${n}" failed to load:`, e); }
  }
  return packs;
}

// Generic block parser — the faithful mirror of the CURRENT Flipper (ha_session.c's
// content_stream_pack): "Key: value" lines, a "---" or blank line ends a block, a
// "Pack:" line names the pack. Each block becomes an object of the file's own
// lowercased keys, exactly what the engine's per-game loadItem expects. This works
// for every game (trivia {q,a,b,c,d,answer}, wyr {a,b}, scramble/draw {word}) because
// the Flipper never interprets the keys.
//
// A key REPEATED within one block (Spyfall's several "R:" role lines) becomes an array
// here. The C streamer has no such notion — it just emits one JSON pair per line, so
// the object it ships genuinely carries the key more than once — and stringifyItem()
// below reproduces exactly that, which is why this parser must not collapse repeats.
export function parseGenericPack(text, fallbackName = "") {
  let name = fallbackName;
  const items = [];
  let cur = {};
  let any = false;
  const flush = () => { if (any) items.push(cur); cur = {}; any = false; };
  for (const raw of text.split("\n")) {
    const line = raw.trim();
    if (line === "" || line === "---") { flush(); continue; }
    const c = line.indexOf(":");
    if (c < 0) continue;
    const key = line.slice(0, c).trim().toLowerCase();
    const val = line.slice(c + 1).trim();
    if (key === "pack") { if (val) name = val; continue; }
    if (!key) continue;
    if (key in cur) cur[key] = [].concat(cur[key], val);
    else cur[key] = val;
    any = true;
  }
  flush();
  return { name, items };
}

// Serialize one parsed block the way the Flipper sends it: one `"key":"value"` pair per
// source line, in file order, so an array value emits the key repeatedly. JSON.stringify
// can't express that (an object has one slot per key), hence the hand-rolled emitter.
export function stringifyItem(item) {
  const parts = [];
  for (const [k, v] of Object.entries(item)) {
    for (const one of [].concat(v)) parts.push(JSON.stringify(k) + ":" + JSON.stringify(one));
  }
  return "{" + parts.join(",") + "}";
}

// Stream one game's packs into the engine the way the Flipper does: contentPack to
// begin, contentItem per block. Returns [{name, count}] for the panel to report.
export async function loadGamePacks(engine, game, dir, names, sub = "") {
  const loaded = [];
  const base = sub ? `../../packs/${dir}/${sub}` : `../../packs/${dir}`;
  for (const n of names) {
    try {
      const res = await fetch(`${base}/${n}.txt`);
      if (!res.ok) { console.warn(`${dir} pack "${n}": HTTP ${res.status}`); continue; }
      const pk = parseGenericPack(await res.text(), n);
      engine.contentPack(game, pk.name);
      for (const it of pk.items) engine.contentItem(stringifyItem(it));
      loaded.push({ name: pk.name, count: pk.items.length });
    } catch (e) { console.warn(`${dir} pack "${n}":`, e); }
  }
  return loaded;
}

// --- content pack config + language resolution (shared with the sim UI and tests) ---
// PACK_DIRS is the English set (packs/<dir>/*.txt). LANGS maps a language code to its
// packs per game dir; a game a language doesn't cover falls back to English.
export const PACK_DIRS = [
  { game: 1, dir: "trivia", names: ["general", "movies", "science", "geography", "music", "games"] },
  { game: 8, dir: "wyr", names: ["everyday", "spooky", "spicy", "superpowers", "timespace", "absurd"] },
  { game: 9, dir: "scramble", names: ["classic", "animals", "food", "space", "music", "sports"] },
  { game: 5, dir: "draw", names: ["classic", "movies", "food", "nature", "animals", "fantasy"] },
  { game: 13, dir: "spectrum", names: ["everyday", "extremes", "opinions", "tastes"] },
  { game: 14, dir: "kmk", names: ["famous", "fiction", "historical", "mix"] },
  { game: 16, dir: "secrets", names: ["1-normal", "2-slightly-spicy", "3-super-chili"] },
  { game: 17, dir: "fillblank", names: ["1-family", "2-spicy"] },
  { game: 19, dir: "spyfall", names: ["1-everyday", "2-travel", "3-backstage"] },
];

export const LANGS = {
  de: {
    trivia: ["general", "geography", "science", "movies", "music", "games"],
    wyr: ["everyday", "absurd", "spicy", "spooky", "superpowers", "timespace"],
    scramble: ["animals", "food", "space", "sports", "music", "classic"],
    draw: ["animals", "classic", "fantasy", "food", "movies", "nature"],
    spectrum: ["everyday", "extremes", "opinions", "tastes"],
    kmk: ["famous", "fiction", "historical", "mix"],
    secrets: ["1-normal", "2-slightly-spicy", "3-super-chili"],
    fillblank: ["1-family", "2-spicy"],
    spyfall: ["1-everyday", "2-travel", "3-backstage"],
  },
  "pt-br": {
    trivia: ["geral"], wyr: ["cotidiano"], scramble: ["palavras"],
    spectrum: ["opostos"], kmk: ["famosos"], draw: ["coisas"],
  },
};

// For a selected language ("" = English), return {game, dir, names, sub} per game: the
// language's packs where it has them (sub = the language subdir), otherwise the English
// root (sub = ""). This is the host's "one language, English fallback per game" rule.
export function resolvePacks(lang) {
  const trans = (lang && LANGS[lang]) || {};
  return PACK_DIRS.map((g) => {
    const langNames = trans[g.dir];
    return lang && langNames
      ? { game: g.game, dir: g.dir, names: langNames, sub: lang }
      : { game: g.game, dir: g.dir, names: g.names, sub: "" };
  });
}
