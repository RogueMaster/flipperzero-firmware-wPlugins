// The lobby's testing switch ("minoverride" on the wire) fills the missing seats
// with engine-run bots instead of silencing the minimum check. This walks the
// switch itself and the three games that need it: the seats appear, they ready
// up on their own, a Werewolf round actually deals five roles to a two-person
// room, and -- the regression that motivated the whole feature -- a game-change
// vote in a bot-filled room resolves on the humans alone instead of waiting on
// seats that cannot see the overlay.
import assert from "node:assert/strict";
import { newEngine } from "./harness-lib.mjs";

const TRIVIA = 1, WW = 18, SF = 19;

const log = [];
function absorb(items) { for (const o of items) log.push(o); return items; }
function lastMsg(wsId, type) {
  for (let i = log.length - 1; i >= 0; i--) {
    const o = log[i];
    if (o.to === "ws" && o.id === wsId && o.msg && o.msg.t === type) return o.msg;
  }
  return undefined;
}

const e = await newEngine();
e.reset();
absorb(e.join(1, "ALICE"));
absorb(e.join(2, "BOB"));
absorb(e.selectGame(WW));

// Two humans, no switch: the lobby shows two and the countdown must not arm.
absorb(e.input(1, { t: "ready", ready: true }));
absorb(e.input(2, { t: "ready", ready: true }));
let ms = 1000;
absorb(e.tick(ms));
let v = lastMsg(1, "werewolf");
assert.equal(v.phase, "lobby", "two players stay in the lobby");
assert.equal(v.players.length, 2, "no phantom seats while the switch is off");

// Flip the switch: three bots take the three missing seats at once.
absorb(e.input(1, { t: "minoverride", on: true }));
v = lastMsg(1, "werewolf");
assert.equal(v.players.length, 5, "bots fill up to Werewolf's minimum of five");
const nicks = v.players.map((p) => p.nick);
for (const b of ["BOT-ADA", "BOT-BEN", "BOT-CLU"])
  assert.ok(nicks.includes(b), `${b} took a seat`);

// The bots ready up on their own (paced, so a phase can't resolve instantly),
// the countdown arms and five roles are dealt.
for (; ms <= 8000; ms += 500) absorb(e.tick(ms));
v = lastMsg(1, "werewolf");
assert.equal(v.phase, "play", "the round started once the bots readied up");
const dealtIn = v.players.filter((p) => p.in).length;
assert.equal(dealtIn, 5, "five players were dealt roles, not two");

// Run the clock through a whole night and day: the bots' night moves and day
// votes go through the same validated handlers as a phone's, and no phase
// hangs waiting on a robot.
for (let i = 0; i < 8; i++) { ms += 400000; absorb(e.tick(ms)); for (let s = 0; s < 6; s++) { ms += 700; absorb(e.tick(ms)); } }
v = lastMsg(1, "werewolf");
assert.ok(v, "the game is still being pushed after a full day cycle");

// A game-change vote counts humans only. With three bots seated, ALICE proposes
// and BOB's lone YES must approve it -- if the bots were franchised the tally
// would sit at 1 of 4 and die in the timeout (the ghost-voter shape).
absorb(e.input(1, { t: "proposeGame", game: "trivia" }));
let gv = lastMsg(2, "gamevote");
assert.ok(gv, "the other human got the vote overlay");
assert.equal(gv.others, 1, "the tally is over the one other HUMAN, not the bots");
absorb(e.input(2, { t: "voteGame", ok: true }));
ms += 500; absorb(e.tick(ms));
v = lastMsg(1, "lobby");
assert.equal(v.game, "trivia", "one human YES approved the switch");

// Trivia needs no bots: the seats empty themselves on the next tick.
ms += 500; absorb(e.tick(ms));
v = lastMsg(1, "lobby");
assert.equal(v.players.length, 2, "bots leave a game that has no minimum to fill");

// Proposing the game that is already running is answered, not swallowed.
absorb(e.input(1, { t: "proposeGame", game: "trivia" }));
const toast = lastMsg(1, "toast");
assert.ok(toast && /already|läuft/i.test(toast.msg), "a no-op proposal gets a toast back");

// Spyfall wants three: the same room refills to exactly that.
absorb(e.selectGame(SF));
ms += 500; absorb(e.tick(ms));
v = lastMsg(1, "spyfall");
assert.equal(v.players.length, 3, "Spyfall refills to its own minimum of three");

// Switch off: every bot seat empties, in Spyfall's own lobby too.
absorb(e.input(1, { t: "minoverride", on: false }));
ms += 500; absorb(e.tick(ms));
v = lastMsg(1, "spyfall");
assert.equal(v.players.length, 2, "switching off clears the bot seats");

console.log("bots: all checks passed");
