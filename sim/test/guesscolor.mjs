// Guess the Color: whole-group round game. A random target swatch, everyone
// submits an R/G/B guess, and points = closeness (Euclidean RGB distance) + a
// speed bonus. Exercises selectGame + the ready/guess/again intents, the reveal
// scoring (closest wins, with speed as the differentiator), and the 5-round
// run to a final podium. Drives the real engine headless.
import assert from "node:assert/strict";
import { newEngine, lastToWs } from "./harness-lib.mjs";

const GC = 11;
const e = await newEngine();
e.reset();
e.join(1, "ALICE");
e.join(2, "BOB");
e.selectGame(GC);

// lobby -> both ready -> countdown (3s) -> play
e.input(1, { t: "ready", ready: true });
e.input(2, { t: "ready", ready: true });
let out = [];
for (let ms = 1000; ms <= 4000; ms += 1000) out = out.concat(e.tick(ms));

const play = lastToWs(out, 1, "gc");
assert.equal(play.msg.phase, "play", "in play after the countdown");
assert.match(play.msg.color, /^#[0-9A-F]{6}$/, "play carries the target as a hex swatch");
const hex = play.msg.color;
const [tr, tg, tb] = [1, 3, 5].map((i) => parseInt(hex.slice(i, i + 2), 16));

// Alice nails it exactly (distance 0); Bob guesses the opposite corner (far).
e.tick(5000);
const far = (v) => (v < 128 ? 255 : 0);
e.input(1, { t: "guess", r: tr, g: tg, b: tb });
const rev = e.input(2, { t: "guess", r: far(tr), g: far(tg), b: far(tb) });

const rA = lastToWs(rev, 1, "gc");
const rB = lastToWs(rev, 2, "gc");
assert.equal(rA.msg.phase, "reveal", "reveal once everyone has guessed");
assert.equal(rA.msg.r, tr, "reveal exposes the true RGB");
assert.equal(rA.msg.winner, "ALICE", "the closest guess wins the round");
assert.equal(rA.msg.iwon, true);
assert.equal(rA.msg.your.dist, 0, "an exact guess is distance 0");
assert.ok(rA.msg.your.points > 200 && rA.msg.your.points <= 300,
  "exact match earns 200 closeness + a partial speed bonus (got " + rA.msg.your.points + ")");
assert.ok(rB.msg.your.points < rA.msg.your.points, "the far guess scores less");
const score = (m, pid) => m.scores.find((p) => p.pid === pid).score;
assert.ok(score(rA.msg, 1) > score(rA.msg, 2), "Alice leads the board");

// A guess after the round is locked is ignored: no state change, no push.
const late = e.input(1, { t: "guess", r: 0, g: 0, b: 0 });
assert.equal(lastToWs(late, 1, "gc"), undefined, "a late guess is silently ignored");

// Play out the rest; every round Alice guesses exact, Bob far. Reach the podium.
let t = 6000;
for (let guard = 0; guard < 80; guard++) {
  out = e.tick(t);
  t += 1000;
  const st = lastToWs(out, 1, "gc");
  if (!st) continue;
  if (st.msg.phase === "final") break;
  if (st.msg.phase === "play" && !st.msg.submitted) {
    const h = st.msg.color;
    const [R, G, B] = [1, 3, 5].map((i) => parseInt(h.slice(i, i + 2), 16));
    e.input(1, { t: "guess", r: R, g: G, b: B });
    e.input(2, { t: "guess", r: far(R), g: far(G), b: far(B) });
  }
}
const fin = lastToWs(out, 1, "gc");
assert.equal(fin.msg.phase, "final", "reaches the final podium after 5 rounds");
assert.ok(Array.isArray(fin.msg.board) && fin.msg.board.length === 2, "final carries the leaderboard");
assert.equal(fin.msg.board[0].nick, "ALICE", "Alice tops the podium");

// "Play again" from the final clears back to the lobby.
const again = e.input(1, { t: "again" });
assert.equal(lastToWs(again, 1, "gc").msg.phase, "lobby", "play again returns to the lobby");

console.log("guesscolor: all checks passed");
