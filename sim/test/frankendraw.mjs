// Frankendraw: the exquisite-corpse drawing game. Everyone starts a sheet, the sheets
// rotate one seat per round, and head / torso / legs each come from a different hand.
// Drives a full three-player game and checks the parts that are easy to get wrong:
// the rotation, the per-player visibility gate (a drawer may see ONLY the sliver of the
// panel above theirs, never the rest of it and never another sheet), the sliver being
// exactly what it claims to be, the gallery walk with its live thumbs and net-score
// winner, the panel name labels, the artwork sink, and the per-panel ink budget.
import assert from "node:assert/strict";
import { newEngine, lastToWs, lastBroadcast } from "./harness-lib.mjs";

const FD = 20;
const PIDS = [1, 2, 3];
const NICK = { 1: "ALICE", 2: "BOB", 3: "CARA" };

const view = (items, pid) => lastToWs(items, pid, "frankendraw").msg;
const picture = (items) => lastBroadcast(items, "fdart").msg;
// Quantise a 0..1 wire coordinate the way the engine does, so the test can predict
// exactly which stored value a stroke turns into.
const q = (v) => Math.floor(v * 255 + 0.5);
const quads = (flat) => {
  const o = [];
  for (let i = 0; i < flat.length; i += 4) o.push(flat.slice(i, i + 4));
  return o;
};

// Lobby -> all ready -> countdown -> panel 1. Frankendraw has no content packs.
async function start(nicks) {
  const e = await newEngine();
  e.reset();
  for (const [pid, nick] of Object.entries(nicks)) e.join(Number(pid), nick);
  e.selectGame(FD);
  let out = [];
  for (const pid of Object.keys(nicks)) out = e.input(Number(pid), { t: "ready", ready: true });
  for (let ms = 1000; ms <= 4000; ms += 1000) out = out.concat(e.tick(ms));
  return { e, out };
}

const { e, out: first } = await start(NICK);
let out = first;

const held = []; // held[round-1][pid] = sheet index that player held that round
const lows = {}; // "sheet/panel" -> x of the segment drawn inside the sliver
const highsY = {}; // "sheet/panel" -> y of the segment drawn well above the sliver

for (let round = 1; round <= 3; round++) {
  const map = {};
  for (const pid of PIDS) {
    const m = view(out, pid);
    assert.equal(m.phase, "draw", "round " + round + " is a drawing round");
    assert.equal(m.round, round);
    assert.equal(m.panel, round - 1, "panel index follows the round");
    assert.equal(m.top, (round - 1) * m.band, "the band starts where the panel does");
    assert.equal(m.bot, round * m.band);
    assert.ok(m.cap > 0, "the ink budget is advertised so the client can show it");
    assert.equal(m.used, 0, "a freshly handed-out panel is empty");
    map[pid] = m.sheet;

    if (round === 1) {
      assert.deepEqual(m.ink, [], "nothing to see above the head panel");
    } else {
      // Everything this player may see is the sliver of the panel above, on the sheet
      // now in their hands -- and nothing else.
      const key = m.sheet + "/" + (m.panel - 1);
      const seen = quads(m.ink);
      assert.equal(seen.length, 1, "exactly the one segment drawn inside the sliver");
      const [x0, y0, x1, y1] = seen[0];
      assert.equal(x0, lows[key], "the sliver segment is the previous drawer's");
      assert.equal(x1, lows[key]);
      assert.ok(
        y0 >= m.top - m.over && y1 >= m.top - m.over,
        "no ink from above the sliver line (" + y0 + "," + y1 + " vs " + (m.top - m.over) + ")",
      );
      assert.ok(y0 <= m.top && y1 <= m.top, "the sliver stays in the panel above");
      // Exactly one segment came through and it is the sliver one, so the segment
      // that drawer put at the TOP of the same panel did not.
      assert.notEqual(y0, highsY[key], "the top of the panel above is not leaked");
      // Nor did any other sheet's panel: each sheet's panel carries its own drawer's
      // marker, and only this one's is here.
      for (let other = 0; other < 3; other++) {
        if (other === m.sheet) continue;
        assert.notEqual(x0, lows[other + "/" + (m.panel - 1)], "only the held sheet is serialised");
      }
    }

    // Two segments: one near the top of my band (private), one in the bottom sliver
    // (the next drawer's only clue). x doubles as a per-player marker.
    const mark = pid / 10;
    const yHigh = (m.top + 2) / m.unit;
    const yLow = (m.bot - 1) / m.unit;
    e.input(pid, { t: "stroke", x0: mark, y0: yHigh, x1: mark, y1: yHigh });
    e.input(pid, { t: "stroke", x0: mark, y0: yLow, x1: mark, y1: yLow });
    highsY[m.sheet + "/" + m.panel] = q(yHigh);
    lows[m.sheet + "/" + m.panel] = q(mark);
  }
  held.push(map);
  // Everyone taps Next: the panel ends early rather than running its 75s timer.
  for (const pid of PIDS) out = e.input(pid, { t: "done" });
}

// --- rotation -----------------------------------------------------------------
for (let s = 0; s < 3; s++) {
  const hands = held.map((map) => PIDS.find((p) => map[p] === s));
  assert.equal(new Set(hands).size, 3, "sheet " + s + " passed through three different hands");
}
for (const pid of PIDS) {
  assert.equal(new Set(held.map((m) => m[pid])).size, 3, "a player never redraws a sheet");
}

// --- the gallery walk, its labels, and the artwork sink ------------------------
const whoOf = (s) => held.map((map) => NICK[PIDS.find((p) => map[p] === s)]);

function checkShow(items, n) {
  // The picture is broadcast once per creature; the per-player state push (which fires
  // on every thumb tap) stays small and carries no ink at all.
  const pic = picture(items);
  assert.equal(pic.n, n, "the gallery walks the creatures in order");
  assert.equal(pic.total, 3);
  assert.deepEqual(pic.who, whoOf(n), "each band's label names the player who drew it");
  assert.equal(new Set(pic.who).size, 3, "three different contributors");
  assert.equal(pic.ink.length, 3, "the picture carries all three panels");
  for (const panel of pic.ink) assert.equal(panel.length, 8, "two segments in every panel");
  for (const pid of PIDS) {
    const g = view(items, pid);
    assert.equal(g.phase, "show");
    assert.equal(g.n, n);
    assert.equal(g.final, false);
    assert.equal(g.ink, undefined, "ink rides in fdart, not in every state push");
  }
}

function checkArt(items, n) {
  const art = items.filter((o) => o.to === "uart" && o.kind === "art");
  const begins = art.filter((a) => a.op === 0);
  const ends = art.filter((a) => a.op === 2);
  const strokes = art.filter((a) => a.op === 1);
  assert.equal(begins.length, 1, "the artwork sink fires once per completed sheet");
  assert.equal(ends.length, 1);
  assert.equal(begins[0].json.id, n);
  assert.equal(begins[0].json.game, "frankendraw");
  const saved = [begins[0].json.w0, begins[0].json.w1, begins[0].json.w2];
  assert.deepEqual(saved, whoOf(n), "the saved sheet credits its three drawers");
  assert.equal(strokes.length, 6, "every stored segment is streamed: two per panel");
  for (let p = 0; p < 3; p++) {
    const mine = strokes.filter((s) => s.json.p === p);
    assert.equal(mine.length, 2);
    const xs = mine.map((s) => s.json.x0);
    assert.ok(xs.every((x) => x === lows[n + "/" + p]), "the sink carries this panel's ink");
  }
}

checkShow(out, 0);
checkArt(out, 0);

// Thumbs are live: one tap and the counts move on everybody's screen, not just the
// tapper's. Tapping the same thumb again takes it back.
out = e.input(1, { t: "thumb", sheet: 0, v: 1 });
for (const pid of PIDS) {
  assert.equal(view(out, pid).up, 1, "everyone sees the thumb arrive");
  assert.equal(view(out, pid).down, 0);
}
assert.equal(view(out, 1).mine, 1, "the tapper's own thumb comes back to them");
assert.equal(view(out, 2).mine, 0, "and only to them");
out = e.input(1, { t: "thumb", sheet: 0, v: 1 });
assert.equal(view(out, 1).up, 0, "the same thumb again takes it back");
assert.equal(view(out, 1).mine, 0);
out = e.input(1, { t: "thumb", sheet: 0, v: 1 }); // leave sheet 0 on +1 (net 1, 1 up)
// A thumb for a creature that is not on screen is ignored outright.
let ignored = e.input(2, { t: "thumb", sheet: 2, v: 1 });
assert.equal(lastToWs(ignored, 1, "frankendraw"), undefined, "off-screen thumbs are dropped");

// The walk advances on its own timer.
out = e.tick(10000);
checkShow(out, 1);
checkArt(out, 1);
// Sheet 1: two up, one down -> net 1 as well, but more thumbs-up than sheet 0.
e.input(1, { t: "thumb", sheet: 1, v: 1 });
e.input(2, { t: "thumb", sheet: 1, v: 1 });
out = e.input(3, { t: "thumb", sheet: 1, v: -1 });
assert.equal(view(out, 1).up, 2, "live count follows every tap");
assert.equal(view(out, 1).down, 1);

out = e.tick(16000);
checkShow(out, 2);
checkArt(out, 2);
out = e.input(1, { t: "thumb", sheet: 2, v: -1 }); // sheet 2: net -1
assert.equal(view(out, 1).down, 1);

// --- finale: the winner, shown again ------------------------------------------
out = e.tick(22000);
const fin = view(out, 1);
assert.equal(fin.phase, "show");
assert.equal(fin.final, true, "the walk ends on the winner, shown once more");
assert.equal(fin.n, 1, "net ties break on the most thumbs-up (sheet 1 over sheet 0)");
assert.equal(fin.net, 1);
assert.deepEqual(picture(out).who, whoOf(1), "the finale re-broadcasts the winner's picture");
assert.equal(
  out.filter((o) => o.to === "uart" && o.kind === "art" && o.op === 0).length, 0,
  "and does not save it a second time",
);
// Every sheet with a positive net pays its three contributors; sheet 2 (net -1) pays
// nothing rather than costing anyone points.
const paid = out.filter((o) => o.to === "uart" && o.kind === "score" && o.reason === "frankendraw");
assert.equal(paid.length, 6, "two sheets x three contributors");
for (const s of paid) assert.equal(s.delta, 100, "net 1 x 100 points");

out = e.tick(30000);
const done = view(out, 1);
assert.equal(done.phase, "final");
assert.equal(done.best, 1);
assert.equal(done.net, 1);
assert.deepEqual(done.who, whoOf(1));
assert.ok(done.board.every((p) => p.score === 200), "with three players every sheet is everyone's");

// --- the three-player floor ----------------------------------------------------
// A sheet has to pass through three hands, so two players can never start one.
e.disconnect(3);
e.input(1, { t: "ready", ready: true });
out = e.input(2, { t: "ready", ready: true });
const lob = view(out, 1);
assert.equal(lob.phase, "lobby", "two ready players do not start a game");
assert.equal(lob.need, 3);

// --- the ink budget ------------------------------------------------------------
// A panel holds a fixed number of segments. Past the cap the engine refuses them, and
// the client is told (`used`/`cap`) so its ink bar can stop the pen instead of letting
// strokes vanish unannounced. Undo hands one segment back.
{
  const g = await start({ 1: "ALICE", 2: "BOB", 3: "CARA" });
  const cap = view(g.out, 1).cap;
  const y = (view(g.out, 1).top + 3) / view(g.out, 1).unit;
  for (let i = 0; i < cap + 3; i++) {
    const x = (i % 200) / 255;
    g.e.input(1, { t: "stroke", x0: x, y0: y, x1: x, y1: y });
  }
  let o = g.e.input(1, { t: "undo" });
  assert.equal(view(o, 1).used, cap - 1, "the cap holds and undo gives one segment back");
  o = g.e.input(1, { t: "stroke", x0: 0.5, y0: y, x1: 0.5, y1: y });
  o = g.e.input(1, { t: "undo" });
  assert.equal(view(o, 1).used, cap - 1, "one more segment fits, and comes back off again");
  // The other players drew nothing, so their panels are untouched by all of that.
  assert.equal(view(o, 2).used, 0);
}

// --- reachable from the phone-side game vote -----------------------------------
// gameIdByName() used to stop at the highest id it knew about, which made a game
// numbered above that bound impossible to propose from a phone at all.
{
  const g = await newEngine();
  g.reset();
  g.join(1, "ALICE");
  g.join(2, "BOB");
  g.selectGame(13); // start somewhere else, then propose this game by name
  const o = g.input(1, { t: "proposeGame", game: "frankendraw" });
  const gv = lastToWs(o, 2, "gamevote");
  assert.ok(gv, "a phone can propose this game (its id is inside the name lookup)");
  assert.equal(gv.msg.game, "frankendraw", "proposed by its wire name, not its label");
}

console.log("frankendraw: all checks passed");
