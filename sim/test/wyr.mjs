// Would You Rather: a whole-group live A/B poll, no scoring. Exercises the full
// lobby -> countdown -> vote/reveal x6 -> final flow, and in particular the
// per-round split history the engine latches at each reveal and hands to the
// client in the final payload (the phone can't reconstruct it — a player who
// joined late never saw the earlier rounds).
import assert from "node:assert/strict";
import { newEngine, lastToWs } from "./harness-lib.mjs";

const WYR = 8;
const N = 5; // five players -> the agreement axis is 60% / 80% / 100%
const e = await newEngine();
e.reset();
const NICKS = ["ALICE", "BOB", "CARA", "DUKE", "EVE"];
NICKS.forEach((n, i) => e.join(i + 1, n));
e.selectGame(WYR);
// A pack is required (wyrCheckStart no-ops with packCount 0). Load one.
e.contentClear();
e.contentPack(WYR, "Test");
for (let i = 0; i < 8; i++) e.contentItem(JSON.stringify({ a: "A" + i, b: "B" + i }));

// lobby -> all ready -> countdown -> round 1 vote
for (let i = 1; i <= N; i++) e.input(i, { t: "ready", ready: true });
let now = 0, out = [];
for (let s = 0; s < 3; s++) { now += 1000; out = e.tick(now); }
assert.equal(lastToWs(out, 1, "wyr").msg.phase, "vote", "round 1 opens for voting after the countdown");

// Six rounds. Each entry is how many of the five vote A (the rest vote B); the
// last round is left unvoted so the vote window times out with a 0/0 split.
const A_VOTES = [5, 4, 3, 5, 3, null];
for (let r = 0; r < A_VOTES.length; r++) {
  const a = A_VOTES[r];
  if (a === null) {
    now += 21000; // WYR_VOTE_SECS = 20 -> reveal on timeout with nothing recorded
    e.tick(now);
    now += 6000; // WYR_REVEAL_MS = 5000
  } else {
    for (let i = 1; i <= N; i++) out = e.input(i, { t: "answer", c: i <= a ? 0 : 1 });
    const m = lastToWs(out, 1, "wyr").msg;
    assert.equal(m.phase, "reveal", "round " + (r + 1) + " reveals once everyone voted");
    assert.deepEqual(m.counts, [a, N - a], "round " + (r + 1) + " tallies the split live");
    now += 6000;
  }
  out = e.tick(now);
}

// Final: the whole game's history plus the voter count the client buckets into.
const fin = lastToWs(out, 1, "wyr").msg;
assert.equal(fin.phase, "final", "six rounds then the final screen");
assert.equal(fin.voters, N, "final carries the voter count");
assert.deepEqual(
  fin.rounds,
  [{ a: 5, b: 0 }, { a: 4, b: 1 }, { a: 3, b: 2 }, { a: 5, b: 0 }, { a: 3, b: 2 }, { a: 0, b: 0 }],
  "final carries every round's A/B split, including the unvoted 0/0 one",
);
// Agreement is always the majority share, so it never dips below 50%: this game
// reads 100 / 80 / 60 / 100 / 60 (the 0/0 round is skipped), mean 80%.
const pcts = fin.rounds.filter((r) => r.a + r.b > 0)
  .map((r) => Math.round((Math.max(r.a, r.b) / (r.a + r.b)) * 100));
assert.deepEqual(pcts, [100, 80, 60, 100, 60], "per-round agreement is the majority share");
assert.equal(pcts.reduce((s, p) => s + p, 0) / pcts.length, 80, "mean agreement is 80%");

// A phone that joins at the final screen gets the same history — the reason the
// engine records it at all.
const late = lastToWs(e.join(9, "ZED"), 9, "wyr").msg;
assert.equal(late.phase, "final", "a late joiner lands on the final screen");
assert.deepEqual(late.rounds, fin.rounds, "a late joiner sees the full round history");

// Play again wipes it.
const again = lastToWs(e.input(1, { t: "again" }), 1, "wyr").msg;
assert.equal(again.phase, "lobby", "again returns everyone to the lobby");

console.log("wyr: all checks passed");
