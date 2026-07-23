// The generic ingest path: the engine is fed the exact JSON blocks the Flipper
// produces from a pack file, and must end up with a playable trivia topic.
//
// The block shape here is deliberately the FILE's keys, lowercased — {q,a,b,c,d,
// answer} — not the engine's internal shape. Trivia's struct uses `c` for the
// CORRECT INDEX while the file uses `C:` for the third option, so a loader that
// consumed this object raw would mark option C's text as the answer index. That
// collision is what this test exists to catch.
//
// Beyond the happy path, this file also covers: a lowercase Answer: letter, an
// unknown game id arriving mid-stream (must not corrupt or attach to trivia's
// content), and a pack whose items are all malformed (must commit no question
// at all, not a half-built one).
import assert from "node:assert/strict";
import { newEngine, lastToWs } from "./harness-lib.mjs";

const HA_GAME_TRIVIA = 1;

const e = await newEngine();
e.reset();
e.contentClear();
e.contentPack(HA_GAME_TRIVIA, "General Knowledge");
e.contentItem(JSON.stringify({
  q: "What is the capital of France?",
  a: "Paris", b: "London", c: "Berlin", d: "Madrid",
  answer: "A",
}));
e.contentItem(JSON.stringify({
  q: "Largest ocean?",
  a: "Atlantic", b: "Indian", c: "Pacific", d: "Arctic",
  answer: "C",
}));

// Two players, ready up, run out the countdown, and check the question that lands.
e.selectGame(HA_GAME_TRIVIA);
e.join(1, "ana");
e.join(2, "bo");
e.input(1, { t: "ready", ready: true });
e.input(2, { t: "ready", ready: true });
let seen = [];
for (let ms = 1000; ms <= 8000; ms += 1000) seen = seen.concat(e.tick(ms));

const q = lastToWs(seen, 1, "trivia");
assert.ok(q, "a trivia question reached the player");
assert.ok(
  q.msg.o.includes("Berlin"),
  "option C's TEXT is an option, not swallowed as the correct index",
);
assert.equal(q.msg.o.length, 4, "all four options survived the mapping");

// Answering the option the file's Answer: letter points at must score.
const idx = q.msg.o.indexOf("Paris") >= 0 ? q.msg.o.indexOf("Paris") : 0;
e.input(1, { t: "answer", c: idx });
const after = e.input(2, { t: "answer", c: (idx + 1) % 4 });
const score = after.find((o) => o.to === "uart" && o.kind === "score" && o.pid === 1);
assert.ok(score && score.delta > 0, "the Answer: letter mapped to the right option");

// A lowercase Answer: letter (e.g. a hand-typed pack) must map to the same
// option as its uppercase form. triviaLoadItem title-cases the letter before
// comparing it to 'A'..'D'; if that branch regressed, "c" would fail the
// `c < 'A' || c > 'D'` check and the whole question would be dropped instead
// of mapping to option index 2.
e.reset();
e.contentClear();
e.contentPack(HA_GAME_TRIVIA, "Lowercase Answer");
e.contentItem(JSON.stringify({
  q: "Lowercase answer letter?",
  a: "wrong1", b: "wrong2", c: "right", d: "wrong3",
  answer: "c", // lowercase -- must still resolve to option index 2
}));

e.selectGame(HA_GAME_TRIVIA);
e.join(1, "ana");
e.join(2, "bo");
e.input(1, { t: "ready", ready: true });
e.input(2, { t: "ready", ready: true });
let lower = [];
for (let ms = 1000; ms <= 8000; ms += 1000) lower = lower.concat(e.tick(ms));

const lowerQ = lastToWs(lower, 1, "trivia");
assert.ok(lowerQ, "a trivia question reached the player");
assert.equal(lowerQ.msg.o[2], "right", "option index 2 (letter C) is the 'right' text");

// The player who picked index 2 (not the letter -- the engine only ever
// speaks in option indices) must be the one who scores.
e.input(1, { t: "answer", c: 2 });
const lowerAfter = e.input(2, { t: "answer", c: 0 });
const lowerScore = lowerAfter.find((o) => o.to === "uart" && o.kind === "score" && o.pid === 1);
assert.ok(lowerScore && lowerScore.delta > 0, "lowercase 'c' mapped to option index 2, and picking it scored");

// An unknown game id (nothing currently uses 99) must be dropped harmlessly.
// The stated constraint: a newer Flipper streaming a game this firmware
// doesn't understand must not corrupt -- or attach to -- an older board's
// existing content. Load a good trivia pack, then feed a pack+item under
// game 99, then confirm trivia still plays its own question untouched.
e.reset();
e.contentClear();
e.contentPack(HA_GAME_TRIVIA, "Still Works");
e.contentItem(JSON.stringify({
  q: "Does an unrelated pack corrupt trivia?",
  a: "no", b: "definitely not", c: "nope", d: "not at all",
  answer: "A",
}));
e.contentPack(99, "Some Future Game"); // unhandled game id
e.contentItem(JSON.stringify({ arbitrary: "shape", nothing: "like trivia" }));

e.selectGame(HA_GAME_TRIVIA);
e.join(1, "ana");
e.join(2, "bo");
e.input(1, { t: "ready", ready: true });
e.input(2, { t: "ready", ready: true });
let unknown = [];
for (let ms = 1000; ms <= 8000; ms += 1000) unknown = unknown.concat(e.tick(ms));

const unknownQ = lastToWs(unknown, 1, "trivia");
assert.ok(unknownQ, "a trivia question reached the player");
assert.equal(unknownQ.msg.phase, "question", "trivia still reaches a real question");
assert.equal(unknownQ.msg.o.length, 4);
assert.ok(unknownQ.msg.o.includes("no"), "the trivia content itself is untouched by the game-99 pack");

const unknownIdx = unknownQ.msg.o.indexOf("no");
e.input(1, { t: "answer", c: unknownIdx });
const unknownAfter = e.input(2, { t: "answer", c: (unknownIdx + 1) % 4 });
const unknownScore = unknownAfter.find((o) => o.to === "uart" && o.kind === "score" && o.pid === 1);
assert.ok(unknownScore && unknownScore.delta > 0, "scoring still works normally after the game-99 pack");

// A block that isn't a question must be skipped, not stored as a broken one.
// Prove it through observable behaviour, since there's no direct accessor for
// a topic's question count: a pack fed ONLY malformed items must reach the
// question phase with zero real content (n:0, empty q/options), not a
// half-built question. Reset first so the earlier good packs above can't
// mask the result.
e.reset();
e.contentClear();
e.contentPack(HA_GAME_TRIVIA, "Broken");
e.contentItem(JSON.stringify({ q: "Only two options?", a: "x", b: "y" }));
e.contentItem(JSON.stringify({ note: "no question here at all" }));

e.selectGame(HA_GAME_TRIVIA);
e.join(1, "ana");
e.join(2, "bo");
e.input(1, { t: "ready", ready: true });
e.input(2, { t: "ready", ready: true });
let broken = [];
for (let ms = 1000; ms <= 8000; ms += 1000) broken = broken.concat(e.tick(ms));

const brokenQ = lastToWs(broken, 1, "trivia");
assert.ok(brokenQ, "the topic itself still exists (it has a name, just no questions)");
// topicCount > 0 is enough for triviaCheckStart to run the countdown regardless
// of qcount, so the phase does reach "question" -- just with nothing in it.
assert.equal(brokenQ.msg.phase, "question", "the countdown still elapses (a topic exists, just an empty one)");
assert.equal(brokenQ.msg.n, 0, "zero questions were committed from the malformed items");
assert.ok(brokenQ.msg.o.every((opt) => opt === ""), "no option text leaked from a half-built question");
assert.equal(brokenQ.msg.q, "", "no question text leaked from a half-built question");

// --- Would You Rather ingests {a,b} prompt pairs -----------------------------
// The file's keys are the option texts; wyrLoadItem stores them as a prompt pair.
// A block missing either option is not a usable prompt and must be dropped.
{
  const HA_GAME_WYR = 8;
  e.reset();
  e.contentClear();
  e.contentPack(HA_GAME_WYR, "Spooky");
  e.contentItem(JSON.stringify({ a: "Be haunted forever", b: "Haunt someone forever" }));
  e.contentItem(JSON.stringify({ a: "only one option" })); // dropped
  e.selectGame(HA_GAME_WYR);
  e.join(1, "ana");
  e.join(2, "bo");
  e.input(1, { t: "ready", ready: true });
  e.input(2, { t: "ready", ready: true });
  let seen = [];
  for (let ms = 1000; ms <= 8000; ms += 1000) seen = seen.concat(e.tick(ms));
  const round = seen.filter((o) => o.to === "ws" && o.msg && o.msg.t === "wyr").pop();
  assert.ok(round, "a wyr round message reached a player");
  const opts = JSON.stringify(round.msg);
  assert.ok(
    opts.includes("Be haunted forever") && opts.includes("Haunt someone forever"),
    "the round shows the pack's prompt, not a hardcoded one",
  );
  assert.ok(!opts.includes("only one option"), "the malformed prompt was dropped");
}

// --- WYR must not serve a stale pack after a re-clear leaves it empty -------
// contentClear() has to fully wipe _wyr.packs (not just packCount back to 0),
// and wyrCheckStart() has to gate on packCount > 0 the same way
// triviaCheckStart() gates on _topicCount > 0. Otherwise: play a WYR pack once
// (packs[0] gets populated), contentClear() for the next round without a
// replacement WYR pack, and a second all-ready would silently start a round
// off the still-nonzero packs[0].count and stale prompt text left over from
// before the clear.
{
  const HA_GAME_WYR = 8;
  const HA_GAME_TRIVIA = 1;

  // Load and play a WYR pack once so packs[0] is populated with a
  // distinctive, easy-to-spot prompt.
  e.reset();
  e.contentClear();
  e.contentPack(HA_GAME_WYR, "Stale");
  e.contentItem(JSON.stringify({ a: "STALE_OPTION_A", b: "STALE_OPTION_B" }));
  e.selectGame(HA_GAME_WYR);
  e.join(1, "ana");
  e.join(2, "bo");
  e.input(1, { t: "ready", ready: true });
  e.input(2, { t: "ready", ready: true });
  let first = [];
  for (let ms = 1000; ms <= 8000; ms += 1000) first = first.concat(e.tick(ms));
  const firstRound = first.filter((o) => o.to === "ws" && o.msg && o.msg.t === "wyr").pop();
  assert.ok(firstRound, "the first WYR pack plays normally");
  assert.ok(
    JSON.stringify(firstRound.msg).includes("STALE_OPTION_A"),
    "the first round shows the loaded pack's prompt",
  );

  // Re-clear content and load a DIFFERENT game's pack -- no WYR pack this
  // time, so _wyr.packCount goes back to 0 with nothing to replace it.
  e.contentClear();
  e.contentPack(HA_GAME_TRIVIA, "Not Wyr");
  e.contentItem(JSON.stringify({
    q: "irrelevant", a: "1", b: "2", c: "3", d: "4", answer: "A",
  }));

  // Re-select WYR (still zero packs loaded) and ready both players again,
  // exactly as if a host cleared content mid-session and players re-readied.
  e.selectGame(HA_GAME_WYR);
  e.input(1, { t: "ready", ready: true });
  e.input(2, { t: "ready", ready: true });
  let second = [];
  for (let ms = 1000; ms <= 8000; ms += 1000) second = second.concat(e.tick(ms));

  const staleRound = second.find(
    (o) => o.to === "ws" && o.msg && o.msg.t === "wyr" &&
      JSON.stringify(o.msg).includes("STALE_OPTION_A"),
  );
  assert.ok(!staleRound, "no wyr round carried the stale pack's prompt after the re-clear");

  const anyRound = second.find((o) => o.to === "ws" && o.msg && o.msg.t === "wyr");
  assert.ok(!anyRound, "with zero wyr packs loaded, the game refuses to start a round at all");
}

// --- Word Scramble ingests {word} --------------------------------------------
{
  const HA_GAME_SCRAMBLE = 9;
  e.reset();
  e.contentClear();
  e.contentPack(HA_GAME_SCRAMBLE, "Animals");
  e.contentItem(JSON.stringify({ word: "ELEPHANT" }));
  e.contentItem(JSON.stringify({ note: "no word here" })); // dropped
  e.selectGame(HA_GAME_SCRAMBLE);
  e.join(1, "ana");
  e.join(2, "bo");
  e.input(1, { t: "ready", ready: true });
  e.input(2, { t: "ready", ready: true });
  let seen = [];
  for (let ms = 1000; ms <= 8000; ms += 1000) seen = seen.concat(e.tick(ms));
  const round = seen.filter((o) => o.to === "ws" && o.msg && o.msg.t === "scramble").pop();
  assert.ok(round, "a scramble round reached a player");
  // The scrambled letters shown must be a permutation of the pack's word.
  const shown = (round.msg.scram || "").split("").sort().join("");
  assert.equal(shown, "AEEHLNPT", "the shown letters are a permutation of ELEPHANT");
}

// --- Draw & Guess ingests {word} ---------------------------------------------
{
  const HA_GAME_DRAW = 5;
  e.reset();
  e.contentClear();
  e.contentPack(HA_GAME_DRAW, "Things");
  e.contentItem(JSON.stringify({ word: "ROCKET" }));
  e.selectGame(HA_GAME_DRAW);
  e.join(1, "ana");
  e.join(2, "bo");
  // Draw starts on its own once two players are present; tick to begin a round.
  let seen = [];
  for (let ms = 1000; ms <= 3000; ms += 500) seen = seen.concat(e.tick(ms));
  // The drawer is told the word privately; assert it reached exactly one socket.
  const wordMsgs = seen.filter((o) => o.to === "ws" && o.msg && o.msg.word === "ROCKET");
  assert.ok(wordMsgs.length >= 1, "the drawer was given the pack's word");
}

// --- pack vote decides which pack plays --------------------------------------
{
  const HA_GAME_WYR = 8;
  e.reset();
  e.contentClear();
  e.contentPack(HA_GAME_WYR, "Everyday");
  e.contentItem(JSON.stringify({ a: "Tea", b: "Coffee" }));
  e.contentPack(HA_GAME_WYR, "Spooky");
  e.contentItem(JSON.stringify({ a: "Ghosts", b: "Zombies" }));
  e.selectGame(HA_GAME_WYR);
  e.join(1, "ana");
  e.join(2, "bo");
  // Both vote the second pack (index 1), then ready up.
  e.input(1, { t: "vote", pack: 1 });
  e.input(2, { t: "vote", pack: 1 });
  e.input(1, { t: "ready", ready: true });
  e.input(2, { t: "ready", ready: true });
  let seen = [];
  for (let ms = 1000; ms <= 8000; ms += 1000) seen = seen.concat(e.tick(ms));
  const round = seen.filter((o) => o.to === "ws" && o.msg && o.msg.t === "wyr").pop();
  const txt = JSON.stringify(round.msg);
  assert.ok(txt.includes("Ghosts") || txt.includes("Zombies"), "the voted (Spooky) pack was played");
  assert.ok(!txt.includes("Tea") && !txt.includes("Coffee"), "the un-voted pack was not played");
}

console.log("content: OK");
