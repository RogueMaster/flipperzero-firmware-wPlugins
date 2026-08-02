// Content language selection. The host picks one language and each game streams
// packs/<game>/<lang>/, falling back to English per game where a language has none.
// Covers the resolver's selection + fallback rule, and an end-to-end check that a
// pt-BR pack actually flows Portuguese content through the real engine.
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { newEngine } from "./harness-lib.mjs";
import { PACK_DIRS, LANGS, resolvePacks, parseGenericPack } from "../web/trivia-packs.js";

// --- selection + fallback ---
{
  // English (""): every game streams from its root, no subdir.
  const en = resolvePacks("");
  assert.equal(en.length, PACK_DIRS.length, "resolves every game");
  assert.ok(en.every((g) => g.sub === ""), "English uses the pack root (no language subdir)");

  // pt-BR: each game it covers streams from the pt-br subdir with the translated names.
  const pt = Object.fromEntries(resolvePacks("pt-br").map((g) => [g.dir, g]));
  for (const dir of Object.keys(LANGS["pt-br"])) {
    assert.equal(pt[dir].sub, "pt-br", dir + " streams from the pt-br subdir");
    assert.deepEqual(pt[dir].names, LANGS["pt-br"][dir], dir + " uses the translated pack names");
  }

  // Fallback: a language with no packs for a game (here, an entirely unknown language)
  // falls back to the English root for every such game.
  const unknown = resolvePacks("xx");
  assert.ok(unknown.every((g) => g.sub === ""), "an unknown language falls back to English per game");
  assert.deepEqual(unknown.map((g) => g.names), PACK_DIRS.map((g) => g.names), "fallback uses the English names");
}

// --- end to end: pt-BR content flows through the engine ---
{
  const e = await newEngine();
  e.reset(); e.contentClear();
  // Load the pt-BR trivia pack the way the host streams it: read file, parse, contentPack/Item.
  const text = readFileSync(new URL("../../packs/trivia/pt-br/geral.txt", import.meta.url), "utf8");
  const pk = parseGenericPack(text, "geral");
  assert.ok(pk.items.length >= 4, "the pt-BR trivia pack parsed some questions");
  e.contentPack(1, pk.name);
  for (const it of pk.items) e.contentItem(JSON.stringify(it));
  e.selectGame(1);
  e.join(1, "ANA"); e.join(2, "BO");
  e.input(1, { t: "ready", ready: true });
  e.input(2, { t: "ready", ready: true });
  let out = [];
  for (let ms = 1000; ms <= 8000; ms += 1000) out = out.concat(e.tick(ms));
  const q = out.filter((o) => o.to === "ws" && o.msg && o.msg.t === "trivia" && o.msg.q).pop();
  assert.ok(q, "a trivia question reached a player");
  const blob = JSON.stringify(q.msg);
  assert.ok(/[áâãàéêíóôõúüç]/i.test(blob) || /\b(qual|quem|quantos|capital)\b/i.test(blob),
    "the streamed trivia is Portuguese (got: " + q.msg.q + ")");
}

console.log("lang: all checks passed");
