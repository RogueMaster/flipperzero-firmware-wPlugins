// Cross-game cumulative scoring ("opponents beaten").
//
// Every game scores on its own scale -- a trivia session runs to five figures, a werewolf
// win pays 1 -- so `score` can never be ranked across games. `total` is the second number
// that can: at a finish, each player gains one point per player they finished ABOVE, so a
// game contributes by how many people you beat at it and needs no per-game tuning.
//
// What this pins down: the award itself and its tie rule, that a game switch clears `score`
// but never `total`, that sitting on a final screen does not pay twice, that a game which
// bails out before it starts pays nobody, that a 1v1 pays exactly one opponent, and that a
// phone which drops and comes back keeps the evening it earned.
import assert from "node:assert/strict";
import { newEngine, lastToWs, macKey } from "./harness-lib.mjs";

const G = { TRIVIA: 1, CONNECT4: 2, GUESSCOLOR: 11, WYR: 8 };

// ANA plays the whole test, but her socket changes when she reconnects, so every read
// names the socket it is looking through.
const lobbyOf = (items, ws = 1) => lastToWs(items, ws, "lobby").msg;
const rosterOf = (items, ws = 1) => {
  const m = {};
  for (const p of lobbyOf(items, ws).players) m[p.pid] = p;
  return m;
};
/**
 * Read the current roster without changing anything.
 *
 * Ticks alone are not enough: no game's tick has a phase-4 branch, so a room sitting on a
 * final emits nothing at all. A silent re-hello (no `named`, so it cannot rename) rebinds
 * the same socket and calls pushAll(), which is a state push with no state change.
 */
const snapshot = (e, ws = 1) => rosterOf(e.join(ws, "ANA"), ws);

/** Run the clock forward, collecting everything the engine emits. */
const run = (e, from, to, step = 500) => {
  let out = [];
  for (let ms = from; ms <= to; ms += step) out = out.concat(e.tick(ms));
  return out;
};

const e = await newEngine();
e.reset();

// Four phones on four distinct devices, so the reconnect check at the end has a key to
// come back on.
const DEV = [0, macKey(2, 0, 0, 0, 0, 1), macKey(2, 0, 0, 0, 0, 2),
  macKey(2, 0, 0, 0, 0, 3), macKey(2, 0, 0, 0, 0, 4)];
for (let p = 1; p <= 4; p++) e.setDevice(p, DEV[p]);
e.join(1, "ANA");
e.join(2, "BEN");
e.join(3, "CAL");
e.join(4, "DOT");

// ---------------------------------------------------------------- 1) the award itself
// Two questions, and an answer pattern that ranks the room 1 > 2 > 3 = 4:
// ANA takes both, BEN takes the second, CAL and DOT take neither and tie on zero.
e.triviaClear();
e.triviaAddTopic("GK");
// Shape matters: the engine reads the options from "o" and the correct INDEX from "c".
e.triviaAddQ(JSON.stringify({ q: "Capital of France?", o: ["Paris", "London", "Berlin", "Madrid"], c: 0 }));
e.triviaAddQ(JSON.stringify({ q: "Largest ocean?", o: ["Atlantic", "Indian", "Pacific", "Arctic"], c: 2 }));
e.selectGame(G.TRIVIA);
for (let p = 1; p <= 4; p++) e.input(p, { t: "ready", ready: true });

let out = run(e, 500, 6000);
assert.equal(lastToWs(out, 1, "trivia").msg.phase, "question", "trivia reaches its first question");
e.input(1, { t: "answer", c: 0 }); // ANA correct
e.input(2, { t: "answer", c: 1 });
e.input(3, { t: "answer", c: 1 });
e.input(4, { t: "answer", c: 1 });

out = run(e, 6500, 14000);
assert.equal(lastToWs(out, 1, "trivia").msg.phase, "question", "trivia reaches its second question");
e.input(1, { t: "answer", c: 2 }); // ANA correct
e.input(2, { t: "answer", c: 2 }); // BEN correct
e.input(3, { t: "answer", c: 0 });
e.input(4, { t: "answer", c: 0 });

out = run(e, 14500, 24000);
const fin = lastToWs(out, 1, "trivia").msg;
assert.equal(fin.phase, "final", "trivia reaches its final");

let roster = rosterOf(out);
assert.ok(roster[1].score > roster[2].score, "ANA outscored BEN inside the game");
assert.ok(roster[2].score > 0 && roster[3].score === 0 && roster[4].score === 0,
  "BEN scored, CAL and DOT did not");

assert.equal(roster[1].total, 3, "1st of four beat three players");
assert.equal(roster[2].total, 2, "2nd of four beat two");
// The tie rule, and the reason the award counts STRICTLY-lower scores: CAL and DOT are
// level on zero, so neither finished above the other and neither gains.
assert.equal(roster[3].total, 0, "tied at the bottom: nobody beaten");
assert.equal(roster[4].total, 0, "tied at the bottom: nobody beaten");

// The final board carries the total too, so the podium can show it.
assert.equal(fin.board.find((p) => p.pid === 1).total, 3, "the final board carries totals");

// The host hears the same numbers. TOTAL is sent ABSOLUTE, unlike the SCORE delta stream,
// so the Flipper holds a copy it cannot drift out of step with.
const totalFrames = out.filter((o) => o.to === "uart" && o.kind === "total");
assert.ok(totalFrames.length, "a finish reports the new totals to the host");
assert.equal(totalFrames.filter((f) => f.pid === 1).pop().total, 3,
  "the host is told an absolute total, not a delta");

// ------------------------------------------------- 2) a final screen pays out only once
// The final phase is re-pushed on every tick while the room sits on the podium, and
// triviaBoard() is rebuilt each time. Awarding at the transition (not in the JSON builder)
// is what makes that safe, so hold here a long time and confirm nothing moves.
run(e, 24500, 60000);
roster = snapshot(e);
assert.equal(roster[1].total, 3, "sitting on the final does not pay again");
assert.equal(roster[2].total, 2, "sitting on the final does not pay again");

// --------------------------------------------- 3) a switch clears score, never the total
const afterSwitch = rosterOf(e.selectGame(G.GUESSCOLOR));
assert.ok(Object.values(afterSwitch).every((p) => p.score === 0),
  "switching games still resets every per-game score");
assert.equal(afterSwitch[1].total, 3, "the cross-game total survives the switch");
assert.equal(afterSwitch[2].total, 2, "the cross-game total survives the switch");

// ------------------------------------------------------- 4) a game that never starts pays
// WYR with no pack loaded bails straight to its final without playing a round. Nobody
// beat anybody, so nobody may gain -- this is the case the `pt.round > 0` guards exist for.
e.contentClear();
e.selectGame(G.WYR);
for (let p = 1; p <= 4; p++) e.input(p, { t: "ready", ready: true });
run(e, 60500, 70000);
roster = snapshot(e);
assert.equal(roster[1].total, 3, "a game that bailed before round 1 awards nothing");
assert.equal(roster[2].total, 2, "a game that bailed before round 1 awards nothing");

// ------------------------------------------------------------ 5) a 1v1 pays one opponent
// Four in a column for ANA against BEN. A duel is two players, so beating one is worth
// exactly one -- it must NOT rank the whole room, because _p[].score is global and other
// matches share it.
e.selectGame(G.CONNECT4);
e.input(1, { t: "challenge", to: 2 });
e.input(2, { t: "accept", from: 1 });
let duel = [];
for (let i = 0; i < 4; i++) {
  duel = duel.concat(e.input(1, { t: "move", n: 0 }));
  if (i < 3) duel = duel.concat(e.input(2, { t: "move", n: 1 }));
}
roster = rosterOf(duel);
assert.equal(roster[1].total, 4, "winning a 1v1 beats exactly one opponent");
assert.equal(roster[2].total, 2, "losing a 1v1 gains nothing");
assert.equal(roster[3].total, 0, "a duel never touches players at other boards");
assert.equal(roster[4].total, 0, "a duel never touches players at other boards");

// ------------------------------------------------------- 6) a reconnect keeps the evening
// The whole point of the total is that it is the evening's tally, so a WiFi blip or a
// locked screen must not cost it. Drop ANA and bring the same device back.
e.disconnect(1);
e.setDevice(5, DEV[1]); // same phone, new socket
const back = e.join(5, "ANA");
const rejoined = lastToWs(back, 5, "lobby").msg.players.find((p) => p.nick === "ANA");
assert.ok(rejoined, "ANA is back in the room");
assert.equal(rejoined.total, 4, "a reconnecting phone keeps its cross-game total");

// ------------------------------------------------------------------ 7) the host can clear
const cleared = rosterOf(e.resetScores(), 5); // ANA is on socket 5 since the reconnect
assert.ok(Object.values(cleared).every((p) => p.total === 0),
  "the host's reset clears the evening, not just the current game");
assert.ok(Object.values(cleared).every((p) => p.score === 0),
  "the host's reset clears per-game scores too");

console.log("totals: all checks passed");
