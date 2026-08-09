// UTF-8 content safety (enables non-English packs, #9). Content packs are UTF-8, so the
// engine must treat multi-byte letters as whole glyphs, not bytes:
//  - Word Scramble shuffles whole letters (a 2-byte letter must not split into invalid
//    continuation bytes), and a correct guess still scores.
//  - Draw's guesser blank count is per character, not per byte.
// ASCII content is unchanged (every glyph is one byte). Drives the real engine headless.
import assert from "node:assert/strict";
import { newEngine, lastToWs } from "./harness-lib.mjs";

const glyphs = (s) => [...s];
const sorted = (s) => glyphs(s).sort().join("");

// --- Word Scramble: whole-glyph shuffle + guess acceptance ---
{
  const SC = 9, WORD = "MÄDCHEN"; // 7 glyphs, 8 bytes (Ä is 2 bytes)
  const e = await newEngine();
  e.reset(); e.contentClear();
  e.contentPack(SC, "DE"); e.contentItem(JSON.stringify({ word: WORD }));
  e.selectGame(SC);
  e.join(1, "ANA"); e.join(2, "BO");
  e.input(1, { t: "ready", ready: true });
  e.input(2, { t: "ready", ready: true });
  let out = [];
  for (let ms = 1000; ms <= 8000; ms += 1000) out = out.concat(e.tick(ms));
  const round = out.filter((o) => o.to === "ws" && o.msg && o.msg.t === "scramble" && o.msg.scram).pop();
  assert.ok(round, "a scramble round reached a player");
  const scram = round.msg.scram;
  assert.equal(glyphs(scram).length, glyphs(WORD).length, "scramble keeps the glyph count (Ä not split)");
  assert.equal(sorted(scram), sorted(WORD), "scramble is a permutation of the word's glyphs");
  assert.ok(!scram.includes("�"), "no replacement chars (valid UTF-8, no split bytes)");
  const g = e.input(1, { t: "guess", text: WORD });
  const score = lastToWs(g, 1, "scramble").msg.scores.find((p) => p.pid === 1).score;
  assert.ok(score > 0, "a correct non-ASCII guess still scores (got " + score + ")");
}

// --- Draw: blank count is per character ---
{
  const DR = 5, WORD = "CAFÉ"; // 4 glyphs, 5 bytes (É is 2 bytes)
  const e = await newEngine();
  e.reset(); e.contentClear();
  e.contentPack(DR, "T"); e.contentItem(JSON.stringify({ word: WORD }));
  e.selectGame(DR); e.join(1, "ANA"); e.join(2, "BO");
  let out = [];
  for (let ms = 500; ms <= 6000; ms += 500) out = out.concat(e.tick(ms));
  const g = out.filter((o) => o.to === "ws" && o.msg && o.msg.t === "draw" && o.msg.role === "guesser" && "len" in o.msg).pop();
  assert.ok(g, "the guesser received a masked word");
  assert.equal(g.msg.len, glyphs(WORD).length, "blank count is 4 glyphs, not 5 bytes");
}

console.log("utf8: all checks passed");
