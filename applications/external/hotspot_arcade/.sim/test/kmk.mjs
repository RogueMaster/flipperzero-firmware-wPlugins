// Kiss Marry Kill: whole-group party game. Each round a rotating chooser secretly
// assigns Kiss/Marry/Kill to three people drawn from the pack; everyone else predicts
// the chooser's assignment. Points = matching positions (0/1/3, since matching two
// forces the third); the chooser earns the guessers' average. Exercises selectGame +
// ready/vote/assign/again, the choose->guess->reveal flow, and the hidden-info rule
// (a guesser never sees the chooser's assignment before the reveal). Drives the real
// engine headless.
import assert from "node:assert/strict";
import { newEngine, lastToWs } from "./harness-lib.mjs";

const KMK = 14;
const e = await newEngine();
e.reset();
e.join(1, "ALICE"); e.join(2, "BOB"); e.join(3, "CARA");
e.selectGame(KMK);

// A pack of >=3 names is required (three people are drawn per round).
e.contentClear();
e.contentPack(KMK, "Test");
for (const n of ["Cleopatra", "Darth Vader", "Taylor Swift", "Sherlock Holmes"])
  e.contentItem(JSON.stringify({ name: n }));

// lobby -> all ready -> countdown -> play(choose)
e.input(1, { t: "ready", ready: true });
e.input(2, { t: "ready", ready: true });
let out = e.input(3, { t: "ready", ready: true });
for (let ms = 1000; ms <= 4000; ms += 1000) out = out.concat(e.tick(ms));

// Find the chooser (iam:true) and the two guessers.
let chooser = 0;
for (const pid of [1, 2, 3]) {
  const m = lastToWs(out, pid, "kmk");
  assert.equal(m.msg.phase, "play", "in play after the countdown");
  assert.equal(m.msg.stage, "choose", "choose stage first");
  assert.ok(Array.isArray(m.msg.people) && m.msg.people.length === 3, "three people drawn");
  if (m.msg.iam) chooser = pid;
}
assert.ok(chooser >= 1, "one player is the chooser");
const guessers = [1, 2, 3].filter((p) => p !== chooser);
// Hidden info: a guesser must NOT see the chooser's assignment during the choose stage.
for (const g of guessers) {
  const m = lastToWs(out, g, "kmk");
  assert.equal(m.msg.answer, undefined, "guessers can't see the assignment before the reveal");
}

// Chooser assigns Kiss=person0, Marry=person1, Kill=person2 -> stage flips to guess.
out = e.input(chooser, { t: "assign", kiss: 0, marry: 1, kill: 2 });
for (const g of guessers) {
  const m = lastToWs(out, g, "kmk");
  assert.equal(m.msg.stage, "guess", "guess stage after the chooser decides");
  assert.equal(m.msg.answer, undefined, "still hidden during the guess stage");
}

// One guesser nails it, the other gets one position. Reveal fires when all have guessed.
e.input(guessers[0], { t: "assign", kiss: 0, marry: 1, kill: 2 }); // exact -> 3
out = e.input(guessers[1], { t: "assign", kiss: 1, marry: 0, kill: 2 }); // only Kill matches -> 1
const rev = lastToWs(out, guessers[0], "kmk");
assert.equal(rev.msg.stage, "reveal", "reveal once everyone guessed");
assert.ok(Array.isArray(rev.msg.answer) && rev.msg.answer.length === 3, "reveal exposes the assignment");
assert.ok(Array.isArray(rev.msg.guesses) && rev.msg.guesses.length === 2, "reveal lists both guesses");
const g0 = rev.msg.guesses.find((x) => x.nick === "ALICE" || x.pts === 3) || rev.msg.guesses[0];
const exact = rev.msg.guesses.find((x) => x.pts === 3);
const partial = rev.msg.guesses.find((x) => x.pts === 1);
assert.ok(exact, "the exact guess scores the full 3");
assert.ok(partial, "the one-position guess scores 1");

console.log("kmk: all checks passed");
