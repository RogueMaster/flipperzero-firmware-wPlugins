// Battleship: 1v1 with a placement phase, hidden fleets, and shoot-again-on-hit.
// Exercises the shared challenge/accept flow, manual placement (valid + rejected),
// firing (hit keeps the turn, miss passes it), the sunk toast, the win, forfeit on
// leave, rematch, and -- most importantly -- that a player's view never exposes an
// un-hit enemy ship cell (hidden information). Drives the real engine headless.
import assert from "node:assert/strict";
import { newEngine, lastToWs } from "./harness-lib.mjs";

const BS = 12;
// All five ships laid out horizontally, one per row (fixed ship order 5,4,3,3,2).
const LAYOUT = "0,0,0;1,0,0;2,0,0;3,0,0;4,0,0";
const SHIP_CELLS = [0, 1, 2, 3, 4, 10, 11, 12, 13, 20, 21, 22, 30, 31, 32, 40, 41]; // 17
const EMPTY_A = 99, EMPTY_B = 50; // cells with no ship in the LAYOUT

const e = await newEngine();
e.reset();
e.join(1, "ALICE");
e.join(2, "BOB");
e.selectGame(BS);

// challenge -> accept -> placement
e.input(1, { t: "challenge", to: 2 });
let out = e.input(2, { t: "accept", from: 1 });
assert.equal(lastToWs(out, 1, "bs").msg.phase, "place", "match starts in placement");

// invalid layout (ships 0 and 1 overlap at row 0) is rejected: no state push
let bad = e.input(1, { t: "place", ships: "0,0,0;0,0,0;2,0,0;3,0,0;4,0,0" });
assert.equal(lastToWs(bad, 1, "bs"), undefined, "invalid placement is rejected, no change");

// valid placements
let a = lastToWs(e.input(1, { t: "place", ships: LAYOUT }), 1, "bs");
assert.equal(a.msg.ready, true, "valid placement readies you");
out = e.input(2, { t: "place", ships: LAYOUT });
a = lastToWs(out, 1, "bs");
assert.equal(a.msg.phase, "fire", "both placed -> firing");
assert.equal(a.msg.yourTurn, true, "challenger (Alice) fires first");

// a reaction inside the match reaches both players (onReact must know battleship matches)
let rx = e.input(1, { t: "react", emoji: "🔥" });
assert.ok(lastToWs(rx, 1, "emoji"), "your own reaction echoes back to you in a match");
assert.ok(lastToWs(rx, 2, "emoji"), "your reaction reaches your opponent in a match");

// a HIT keeps the turn (shoot again)
out = e.input(1, { t: "fire", n: SHIP_CELLS[0] });
a = lastToWs(out, 1, "bs");
assert.equal(a.msg.track[SHIP_CELLS[0]], 2, "hit shows on the tracking grid");
assert.equal(a.msg.yourTurn, true, "a hit keeps your turn");

// HIDDEN INFO: an un-hit enemy ship cell must NOT leak in Alice's view
assert.equal(a.msg.track[1], 0, "an un-hit enemy ship cell reads as un-shot (hidden)");
assert.equal(a.msg.oppFleet, undefined, "the enemy fleet is not revealed during play");
const revealed = a.msg.track.filter((v) => v !== 0).length;
assert.equal(revealed, 1, "only the cells you've actually shot are non-zero");

// a MISS passes the turn; Bob misses back to return it to Alice
out = e.input(1, { t: "fire", n: EMPTY_A });
a = lastToWs(out, 1, "bs");
assert.equal(a.msg.track[EMPTY_A], 1, "miss shows on the tracking grid");
assert.equal(a.msg.yourTurn, false, "a miss passes the turn");
let b = lastToWs(out, 2, "bs");
assert.equal(b.msg.yourTurn, true, "it's Bob's turn");
assert.equal(b.msg.mine[SHIP_CELLS[0]], 3, "Bob sees the incoming hit on his own fleet");
e.input(2, { t: "fire", n: EMPTY_B }); // Bob misses, turn back to Alice

// Alice sinks the rest. All remaining ship cells are hits, so she keeps firing.
let sankToast = false;
for (let i = 1; i < SHIP_CELLS.length; i++) {
  out = e.input(1, { t: "fire", n: SHIP_CELLS[i] });
  if (lastToWs(out, 1, "toast")) sankToast = true; // a ship went down at some point
}
assert.ok(sankToast, "sinking a ship sends a toast");

a = lastToWs(out, 1, "bs");
b = lastToWs(out, 2, "bs");
assert.equal(a.msg.phase, "over", "17 hits ends the game");
assert.equal(a.msg.result, "win", "Alice wins");
assert.equal(b.msg.result, "lose", "Bob loses");
assert.ok(Array.isArray(a.msg.oppFleet), "the enemy fleet is revealed at game over");
assert.equal(a.msg.oppShips, 0, "all enemy ships are sunk");

// rematch -> back to placement, first move alternated (Bob fires first now)
out = e.input(1, { t: "rematch" });
a = lastToWs(out, 1, "bs");
assert.equal(a.msg.phase, "place", "rematch returns to placement");
e.input(1, { t: "place", ships: LAYOUT });
out = e.input(2, { t: "place", ships: LAYOUT });
b = lastToWs(out, 2, "bs");
assert.equal(b.msg.yourTurn, true, "rematch alternates who fires first (Bob now)");

// forfeit: leaving mid-match hands the win to the opponent
out = e.input(1, { t: "leaveGame" });
b = lastToWs(out, 2, "bs");
assert.equal(b.msg.phase, "over", "opponent leaving ends the match");
assert.equal(b.msg.result, "win", "you win when your opponent forfeits");

console.log("battleship: all checks passed");
