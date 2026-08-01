// Spectrum: wavelength-style party game. One player is the psychic each round: they
// see a hidden 0..100 target between two words and type a clue; everyone else slides
// to guess. Points by closeness; the psychic scores by how well the group guessed.
// Exercises selectGame + ready/vote/clue/slide/again and the clue->guess->reveal flow.
import assert from "node:assert/strict";
import { newEngine, lastToWs } from "./harness-lib.mjs";

const SP = 13;
const e = await newEngine();
e.reset();
e.join(1, "ALICE"); e.join(2, "BOB"); e.join(3, "CARA");
e.selectGame(SP);
// A pack is required (spectrumCheckStart no-ops with packCount 0). Load one.
e.contentClear();
e.contentPack(SP, "Test");
e.contentItem(JSON.stringify({ left: "Cold", right: "Hot" }));
e.contentItem(JSON.stringify({ left: "Cheap", right: "Expensive" }));

// lobby -> all ready -> countdown -> play(clue)
e.input(1, { t: "ready", ready: true });
e.input(2, { t: "ready", ready: true });
let out = e.input(3, { t: "ready", ready: true });
for (let ms = 1000; ms <= 4000; ms += 1000) out = out.concat(e.tick(ms));

// Find the psychic (the player whose message has iam:true).
let psychic = 0, target = 0;
for (const pid of [1, 2, 3]) {
  const m = lastToWs(out, pid, "spectrum");
  assert.equal(m.msg.phase, "play", "in play after countdown");
  if (m.msg.iam) { psychic = pid; target = m.msg.target; }
}
assert.ok(psychic >= 1, "one player is the psychic");
assert.ok(target >= 5 && target <= 95, "psychic sees the hidden target (got " + target + ")");
// Non-psychics must NOT see the target during the clue stage (hidden info).
const guessers = [1, 2, 3].filter((p) => p !== psychic);
for (const g of guessers) {
  const m = lastToWs(out, g, "spectrum");
  assert.equal(m.msg.target, undefined, "guessers can't see the target during clue stage");
  assert.equal(m.msg.stage, "clue", "clue stage first");
}

// Psychic types a clue -> stage flips to guess, clue is now visible to all.
out = e.input(psychic, { t: "clue", text: "lukewarm" });
for (const g of guessers) {
  const m = lastToWs(out, g, "spectrum");
  assert.equal(m.msg.stage, "guess", "guess stage after the clue");
  assert.equal(m.msg.clue, "lukewarm", "clue is broadcast to guessers");
}

// One guesser nails the target, the other is far. Reveal fires when all have guessed.
e.input(guessers[0], { t: "slide", n: target });
out = e.input(guessers[1], { t: "slide", n: (target + 60) % 100 });
const rev = lastToWs(out, psychic, "spectrum");
assert.equal(rev.msg.stage, "reveal", "reveal once everyone guessed");
assert.equal(rev.msg.target, target, "reveal exposes the target to everyone");
assert.ok(Array.isArray(rev.msg.guesses) && rev.msg.guesses.length === 2, "reveal lists both guesses");
const near = rev.msg.guesses.find((x) => x.g === target);
assert.ok(near && near.pts >= 4, "an exact guess earns the bullseye (got " + (near && near.pts) + ")");

console.log("spectrum: all checks passed");
