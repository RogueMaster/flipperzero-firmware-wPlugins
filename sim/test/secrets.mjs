// Secrets: a whole-group hidden-vote party game. Each round shows a yes/no question;
// players first secretly ANSWER, then secretly PREDICT how many of the N of them said
// yes (0..N), then reveal. Only the group's total yes-count is revealed — the individual
// yes/no answers are never serialized to anyone. Predictions are guesses about the group,
// so at reveal every player's prediction + points appear in "guesses".
// Exercises selectGame + ready/vote/reply/predict/again and the answer->predict->reveal
// flow, asserts the anonymity rule (answers never leak), and that selecting the game pushes
// state to every player even when switching from another game.
import assert from "node:assert/strict";
import { newEngine, lastToWs } from "./harness-lib.mjs";

const SEC = 16;
const e = await newEngine();
e.reset();
e.join(1, "ALICE"); e.join(2, "BOB"); e.join(3, "CARA");

// Routing: the host has a different game active first, then switches to Secrets. Every
// connected player must receive the Secrets push on select (the engine side of "everyone
// follows the host into the new game" — the client's route() in app.js does the view switch).
e.selectGame(8); // Would You Rather
const sel = e.selectGame(SEC);
for (const pid of [1, 2, 3]) {
  const m = lastToWs(sel, pid, "secrets");
  assert.ok(m && m.msg.phase === "lobby",
    "player " + pid + " is pushed into Secrets even coming from another game");
}

// A pack is required (secretsCheckStart no-ops with packCount 0). Load one.
e.contentClear();
e.contentPack(SEC, "Test");
e.contentItem(JSON.stringify({ q: "Do you talk to animals?" }));
e.contentItem(JSON.stringify({ q: "Have you ever cried at a film?" }));

// lobby -> all ready -> countdown -> answer (the answer stage comes FIRST)
e.input(1, { t: "ready", ready: true });
e.input(2, { t: "ready", ready: true });
let out = e.input(3, { t: "ready", ready: true });
for (let ms = 1000; ms <= 4000; ms += 1000) out = out.concat(e.tick(ms));

for (const pid of [1, 2, 3]) {
  const m = lastToWs(out, pid, "secrets");
  assert.equal(m.msg.phase, "answer", "answer phase first");
  assert.equal(m.msg.n, 3, "N = 3 joined players");
  assert.equal(m.msg.myanswer, -1, "own answer unset at round start");
  assert.ok(typeof m.msg.q === "string" && m.msg.q.length, "question text present");
  assert.equal(m.msg.yes, undefined, "the yes-count is never sent before reveal");
  assert.equal(m.msg.guesses, undefined, "no guesses before reveal");
}

// p1 and p2 answer yes; check that player 3 (a not-yet-answered observer) never sees
// their individual answers, nor the running yes-count.
e.input(1, { t: "reply", v: 1 });
out = e.input(2, { t: "reply", v: 1 });
{
  const m3 = lastToWs(out, 3, "secrets");
  assert.equal(m3.msg.phase, "answer", "still answering");
  assert.equal(m3.msg.myanswer, -1, "observer's own answer still unset");
  assert.equal(m3.msg.answers, undefined, "no per-player answer list mid-answer");
  assert.equal(m3.msg.answer, undefined, "no per-player answer array mid-answer");
  assert.equal(m3.msg.yes, undefined, "yes-count stays hidden until reveal");
  // Nothing in the serialized state should betray the two yes answers so far. Strip the
  // observer's own myanswer and the phase token (the answer STAGE is literally named
  // "answer"); no other "answer" field may remain.
  const s = JSON.stringify(m3.msg)
    .replace(/"myanswer":-?\d+/g, "").replace(/"phase":"[a-z]+"/g, "");
  assert.ok(!/answer/i.test(s), "no other player's answer leaks into the observer's state");
}

// p3 answers no -> all answered -> predict stage. yesCount (2) is computed but hidden.
out = e.input(3, { t: "reply", v: 0 });
for (const pid of [1, 2, 3]) {
  const m = lastToWs(out, pid, "secrets");
  assert.equal(m.msg.phase, "predict", "predict stage once everyone answered");
  assert.equal(m.msg.myprediction, -1, "own prediction unset entering predict");
  assert.equal(m.msg.yes, undefined, "yes-count still hidden during predict");
  assert.equal(m.msg.answers, undefined, "answers never serialized in predict");
}

// Everyone predicts: p1=2 (exact), p2=1 (off by one), p3=3 (off by one).
e.input(1, { t: "predict", n: 2 });
e.input(2, { t: "predict", n: 1 });
out = e.input(3, { t: "predict", n: 3 });
for (const pid of [1, 2, 3]) {
  const m = lastToWs(out, pid, "secrets");
  assert.equal(m.msg.phase, "reveal", "reveal after all predicted");
  assert.equal(m.msg.yes, 2, "the group yes-count (2) is revealed to everyone");
  assert.equal(m.msg.n, 3, "N is sent for the 0..N reveal scale");
  // Predictions ARE exposed at reveal (guesses about the group, not personal answers).
  assert.ok(Array.isArray(m.msg.guesses) && m.msg.guesses.length === 3,
    "reveal lists every player's prediction + points");
  for (const g of m.msg.guesses) {
    assert.ok(typeof g.nick === "string" && typeof g.n === "number" && typeof g.pts === "number",
      "each guess has nick/n/pts");
  }
  // Individual yes/no answers must STILL never appear (predictions may; answers may not).
  assert.equal(m.msg.answers, undefined, "reveal never lists individual answers");
  const s = JSON.stringify(m.msg)
    .replace(/"myanswer":-?\d+/g, "").replace(/"phase":"[a-z]+"/g, "");
  assert.ok(!/answer/i.test(s), "no per-player answer field at reveal");
}
// Scoring: an exact prediction earns 1, everything else nothing -- being off by one
// used to earn a point too, which made the reveal fiddly to read for little gain.
assert.equal(lastToWs(out, 1, "secrets").msg.mygain, 1, "exact prediction earns 1");
assert.equal(lastToWs(out, 2, "secrets").msg.mygain, 0, "off by one earns nothing");
assert.equal(lastToWs(out, 3, "secrets").msg.mygain, 0, "off by one earns nothing");
// The exact guesser (predicted 2) is the only one carrying points in the shared list.
const gs = lastToWs(out, 1, "secrets").msg.guesses;
const exact = gs.find((g) => g.n === 2);
assert.ok(exact && exact.pts === 1, "the exact guess is listed with +1");
assert.ok(gs.filter((g) => g.pts > 0).length === 1, "only the exact guess scores");

console.log("secrets: all checks passed");
