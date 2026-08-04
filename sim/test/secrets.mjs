// Secrets: a whole-group hidden-vote party game. Each round shows a yes/no question;
// players secretly predict how many of the N of them will say yes (0..N), then secretly
// answer. Only the group's total yes-count is revealed — never who answered what.
// Exercises selectGame + ready/vote/predict/reply/again and the predict->answer->reveal
// flow, and asserts the anonymity rule: no player ever receives another player's
// individual prediction or answer, and the yes-count is hidden until reveal.
import assert from "node:assert/strict";
import { newEngine, lastToWs } from "./harness-lib.mjs";

const SEC = 16;
const e = await newEngine();
e.reset();
e.join(1, "ALICE"); e.join(2, "BOB"); e.join(3, "CARA");
e.selectGame(SEC);
// A pack is required (secretsCheckStart no-ops with packCount 0). Load one.
e.contentClear();
e.contentPack(SEC, "Test");
e.contentItem(JSON.stringify({ q: "Do you talk to animals?" }));
e.contentItem(JSON.stringify({ q: "Have you ever cried at a film?" }));

// lobby -> all ready -> countdown -> predict
e.input(1, { t: "ready", ready: true });
e.input(2, { t: "ready", ready: true });
let out = e.input(3, { t: "ready", ready: true });
for (let ms = 1000; ms <= 4000; ms += 1000) out = out.concat(e.tick(ms));

for (const pid of [1, 2, 3]) {
  const m = lastToWs(out, pid, "secrets");
  assert.equal(m.msg.phase, "predict", "predict phase first");
  assert.equal(m.msg.n, 3, "N = 3 joined players");
  assert.equal(m.msg.myprediction, -1, "own prediction unset at round start");
  assert.ok(typeof m.msg.q === "string" && m.msg.q.length, "question text present");
  assert.equal(m.msg.yes, undefined, "the yes-count is never sent before reveal");
}

// Everyone predicts: p1=2 (will be exact), p2=1 (off by one), p3=3 (off by one).
e.input(1, { t: "predict", n: 2 });
e.input(2, { t: "predict", n: 1 });
out = e.input(3, { t: "predict", n: 3 });
{
  const m3 = lastToWs(out, 3, "secrets");
  assert.equal(m3.msg.phase, "answer", "answer stage once everyone predicted");
  assert.equal(m3.msg.myprediction, 3, "player 3 sees only its own prediction");
  // Anonymity: no other player's prediction is serialized anywhere in the payload.
  assert.equal(m3.msg.predictions, undefined, "no per-player prediction list");
  assert.equal(m3.msg.predict, undefined, "no per-player prediction array");
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
  // Nothing in the whole serialized message should betray the two yes answers so far:
  // the only answer field is the observer's own (-1).
  const s = JSON.stringify(m3.msg);
  assert.ok(!/"a(nswer)?[0-9]?":\s*1/.test(s.replace(/"myanswer":-1/, "")),
    "no other player's yes/no leaks into the observer's state");
}

// p3 answers no -> yesCount = 2, reveal fires.
out = e.input(3, { t: "reply", v: 0 });
for (const pid of [1, 2, 3]) {
  const m = lastToWs(out, pid, "secrets");
  assert.equal(m.msg.phase, "reveal", "reveal after all answered");
  assert.equal(m.msg.yes, 2, "the group yes-count (2) is revealed to everyone");
  assert.equal(m.msg.answers, undefined, "reveal still never lists individual answers");
}
// Scoring: exact prediction -> +3, off by one -> +1, else 0.
assert.equal(lastToWs(out, 1, "secrets").msg.mygain, 3, "exact prediction earns 3");
assert.equal(lastToWs(out, 2, "secrets").msg.mygain, 1, "off-by-one earns 1");
assert.equal(lastToWs(out, 3, "secrets").msg.mygain, 1, "off-by-one earns 1");

console.log("secrets: all checks passed");
