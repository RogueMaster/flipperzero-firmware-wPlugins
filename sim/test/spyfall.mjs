// Spyfall: everyone shares a location and a role there except one spy, who is told
// neither. The round is driven by players PRESSING buttons -- "I know the location"
// (spy only) and "I know the spy" (everyone, spy included) -- and the six-minute clock
// is only the fallback: running it out starts a round-robin nomination the table still
// has to win. This test drives the real engine through each of those paths and pins:
//   * the card stage: the clock does not start until every phone has tapped OK;
//   * exactly one spy per round, and the spy rotates between rounds;
//   * the location NEVER reaches the spy before the reveal, in any stage (the only
//     location text in their payload is the pack's full candidate list, identical every
//     round, which therefore carries nothing);
//   * a role only ever reaches its own holder;
//   * "I know the spy": right ends the round, wrong locks that player out for the round
//     while play carries on;
//   * "I know the location": right and wrong both end the round;
//   * the round-robin nomination: a poll short of the threshold passes the turn on, a
//     poll that clears it settles the round either way, and the spy wins outright once
//     every seat has nominated in vain;
//   * scoring: every outcome is worth exactly 1 point.
import assert from "node:assert/strict";
import { newEngine, lastToWs } from "./harness-lib.mjs";

const SF = 19;
// Blocks exactly as the Flipper streams them: one JSON pair per "Key: value" line, so
// the several "R:" role lines arrive as the SAME key repeated in one object.
const ITEMS = [
  '{"loc":"Beach","r":"Lifeguard","r":"Surfer","r":"Ice cream vendor"}',
  '{"loc":"Hospital","r":"Surgeon","r":"Nurse","r":"Patient"}',
  '{"loc":"Airport","r":"Pilot","r":"Passenger","r":"Security officer"}',
  '{"loc":"Circus","r":"Clown","r":"Acrobat","r":"Ringmaster"}',
];
const ALL_LOCS = ["Beach", "Hospital", "Airport", "Circus"];

// One table, wound forward to the card stage of round 1. `t` is the shared clock: the
// engine only advances time on tick(), so asserting against it is exact.
async function table(n) {
  const pids = [];
  for (let p = 1; p <= n; p++) pids.push(p);
  const e = await newEngine();
  e.reset();
  for (const p of pids) e.join(p, "P" + p);
  e.selectGame(SF);
  e.contentClear();
  e.contentPack(SF, "Test");
  for (const it of ITEMS) e.contentItem(it);
  for (const p of pids) e.input(p, { t: "ready", ready: true });
  const g = {
    e, pids, t: 0,
    out: [],
    tick(ms) { g.t = ms; g.out = e.tick(ms); return g.out; },
    tickTo(deadline) { return g.tick(deadline); },
    in(pid, obj) { g.out = e.input(pid, obj); return g.out; },
    views() {
      const v = {};
      for (const p of pids) {
        const m = lastToWs(g.out, p, "spyfall");
        if (m) v[p] = m.msg;
      }
      return v;
    },
    // Everyone taps OK; the six minutes start on the last tap, not at round start.
    readCards() {
      const before = g.t;
      const v = g.views();
      for (const p of pids) assert.equal(v[p].stage, "card", "the card comes first");
      assert.equal(v[pids[0]].total, pids.length, "the card stage counts the table");
      assert.equal(v[pids[0]].seen, 0, "nobody has acknowledged yet");
      const info = inspect(v);
      for (let i = 0; i < pids.length - 1; i++) {
        g.in(pids[i], { t: "seen" });
        const w = g.views();
        assert.equal(w[pids[0]].stage, "card", "still reading: the clock has not started");
        assert.equal(w[pids[i]].myseen, true, "your own tap is echoed back");
        assert.equal(w[pids[0]].seen, i + 1, "acknowledgement counter");
      }
      g.in(pids[pids.length - 1], { t: "seen" });
      const w = g.views();
      for (const p of pids) assert.equal(w[p].stage, "talk", "last OK opens the questioning");
      assert.equal(w[pids[0]].deadline, before + 360000,
        "the 6-minute clock starts when the cards are away, not at round start");
      return info;
    },
  };
  g.tick(3000);
  return g;
}

// The round's secret, read from a non-spy (who is allowed to know it), plus every
// hidden-information assertion. Safe to call in any stage before the reveal.
function inspect(v) {
  const pids = Object.keys(v).map(Number);
  const spies = pids.filter((p) => v[p].spy);
  assert.equal(spies.length, 1, "exactly one spy per round");
  const spy = spies[0];
  const others = pids.filter((p) => p !== spy);
  const loc = v[others[0]].loc;
  assert.ok(ALL_LOCS.includes(loc), "non-spies are told the location");
  for (const p of others) assert.equal(v[p].loc, loc, "the table shares one location");

  // --- the hidden-information rule ---
  const sm = v[spy];
  assert.equal(sm.loc, undefined, "the spy is never sent the location");
  assert.equal(sm.role, undefined, "the spy holds no role");
  assert.deepEqual(sm.locs, ALL_LOCS, "the spy gets the pack's full candidate list");
  // Strip that list and the location must not appear anywhere else in the payload.
  const rest = JSON.stringify({ ...sm, locs: undefined });
  assert.ok(!rest.includes(loc), "no other field leaks the location to the spy: " + rest);
  assert.equal(sm.roles, undefined, "the role table only exists on the reveal");

  // --- roles are private ---
  const roles = {};
  for (const p of others) {
    assert.equal(typeof v[p].role, "string", "a non-spy is told their own role");
    assert.ok(v[p].role.length > 0);
    roles[p] = v[p].role;
    assert.equal(v[p].roles, undefined, "the role table only exists on the reveal");
    assert.equal(v[p].locs, undefined, "only the spy needs the candidate list");
  }
  for (const p of others) {
    for (const q of others) {
      if (p === q || roles[p] === roles[q]) continue;
      assert.ok(!JSON.stringify(v[p]).includes(roles[q]),
        "player " + p + " must not see player " + q + "'s role");
    }
  }
  return { spy, others, loc, roles };
}

// Reveal shared checks: outcome, who scored, and that the reveal is public.
function checkReveal(g, spy, loc, outcome, gain) {
  const v = g.views();
  for (const p of g.pids) {
    assert.equal(v[p].stage, "reveal", "the round reveals (wanted " + outcome + ")");
    assert.equal(v[p].outcome, outcome, "outcome");
    assert.equal(v[p].loc, loc, "the location is public on the reveal");
    assert.equal(v[p].spyPid, spy, "the spy is named on the reveal");
    assert.equal(v[p].roles.length, g.pids.length, "every seat's role is listed");
    assert.equal(v[p].roles.filter((r) => r.spy).length, 1, "one spy in the role table");
    assert.equal(v[p].mygain, gain[p] || 0, "points for " + p);
  }
  const board = v[g.pids[0]].scores;
  for (const p of g.pids) {
    assert.equal(board.find((x) => x.pid === p).score, gain[p] || 0, "score for " + p);
  }
  return v;
}

// Walk the talk clock out into the hush, then into the first nomination turn.
function toNominations(g) {
  const v0 = g.views();
  g.tickTo(v0[g.pids[0]].deadline);
  let v = g.views();
  for (const p of g.pids) {
    assert.equal(v[p].stage, "nominate", "the clock running out starts nominations");
    assert.equal(v[p].nomStage, "hush", "'Time's up. Stop discussing!' first");
    assert.equal(v[p].need, g.pids.length - 1, "the threshold is the non-spy count");
  }
  g.tickTo(v[g.pids[0]].deadline);
  v = g.views();
  for (const p of g.pids) assert.equal(v[p].nomStage, "pick", "then somebody nominates");
  return v;
}

// ---- 1: card stage, clock start, "I know the spy" landing on the spy ---------------
{
  const g = await table(3);
  const { spy, others, loc } = g.readCards();
  let v = g.views();
  inspect(v); // still hidden in the talk stage
  assert.equal(v[spy].spent, false, "nobody has spent their accusation yet");
  assert.equal(v[spy].cands.length, 3, "the whole table is accusable");
  g.in(others[0], { t: "accuse", pid: spy });
  const gain = {};
  for (const p of others) gain[p] = 1;
  checkReveal(g, spy, loc, "caught", gain);

  // The spy rotates: let the reveal lapse and check round 2 has a different spy.
  g.tickTo(g.views()[spy].deadline);
  v = g.views();
  for (const p of g.pids) assert.equal(v[p].round, 2, "on to round 2");
  const spy2 = g.pids.filter((p) => v[p].spy);
  assert.equal(spy2.length, 1, "one spy in round 2");
  assert.notEqual(spy2[0], spy, "the spy rotates between rounds");
  // Round 2 deals a fresh card stage, and the one-press lockout is per ROUND: the
  // player who spent theirs in round 1 gets it back here.
  g.readCards();
  const v2 = g.views();
  for (const p of g.pids) {
    assert.equal(v2[p].spent, false, "the accusation lockout resets each round");
    assert.equal(v2[p].misses.length, 0, "and so does the miss board");
  }
}

// ---- 2: a wrong accusation locks that player out, and play carries on --------------
{
  const g = await table(3);
  const { spy, others, loc } = g.readCards();
  // The spy presses it too -- deliberately allowed, as cover. It can never be right,
  // so it always costs them their own press.
  g.in(spy, { t: "accuse", pid: others[0] });
  let v = g.views();
  assert.equal(v[spy].stage, "talk", "a wrong accusation does not end the round");
  assert.equal(v[spy].spent, true, "the spy burnt their own press bluffing");
  assert.equal(v[others[0]].spent, false, "nobody else is affected");
  assert.equal(v[others[0]].misses.length, 1, "the miss is shown to the whole table");
  assert.equal(v[others[0]].misses[0].by, "P" + spy);
  assert.equal(v[others[0]].misses[0].of, "P" + others[0]);
  inspect(v); // and it still leaks nothing

  // others[0] misses too, then may not press again -- not even correctly.
  g.in(others[0], { t: "accuse", pid: others[1] });
  v = g.views();
  assert.equal(v[others[0]].spent, true, "a wrong accusation spends your press");
  assert.equal(v[others[0]].misses.length, 2, "two misses on the board");
  g.in(others[0], { t: "accuse", pid: spy });
  assert.equal(Object.keys(g.views()).length, 0,
    "a locked-out player's press is dropped outright, even a correct one");

  // The player who still has their press lands it.
  g.in(others[1], { t: "accuse", pid: spy });
  const gain = {};
  for (const p of others) gain[p] = 1;
  v = checkReveal(g, spy, loc, "caught", gain);
  assert.equal(v[others[1]].misses.length, 2, "the two earlier misses are still on record");
}

// ---- 3: the spy calls the location, correctly --------------------------------------
{
  const g = await table(3);
  const { spy, loc } = g.readCards();
  const idx = g.views()[spy].locs.indexOf(loc);
  assert.ok(idx >= 0);
  g.in(spy, { t: "solve", loc: idx });
  const gain = {};
  gain[spy] = 1;
  const v = checkReveal(g, spy, loc, "solved", gain);
  assert.equal(v[spy].called, loc, "the reveal says what the spy called");
}

// ---- 4: ...and incorrectly --------------------------------------------------------
{
  const g = await table(3);
  const { spy, others, loc } = g.readCards();
  const wrongIdx = g.views()[spy].locs.findIndex((n) => n !== loc);
  const wrongName = g.views()[spy].locs[wrongIdx];
  g.in(spy, { t: "solve", loc: wrongIdx });
  const gain = {};
  for (const p of others) gain[p] = 1;
  const v = checkReveal(g, spy, loc, "failed", gain);
  assert.equal(v[spy].called, wrongName, "the reveal says what the spy called");
}

// ---- 5: nominations -- a poll short of the threshold passes the turn on, the next
//         one clears it and names the spy ------------------------------------------
{
  const g = await table(4); // need = 3 agreements, so a lone ally is not enough
  const { spy, others, loc } = g.readCards();
  let v = toNominations(g);
  const first = v[g.pids[0]].nominator;
  assert.equal(first, g.pids[0], "the round-robin starts at the first seat");
  for (const p of g.pids) {
    assert.equal(v[p].nomMe, p === first, "only one seat is asked at a time");
    assert.equal(v[p].nominatorNick, "P" + first, "the whose-turn indicator");
  }
  // A nomination nobody else has to accept.
  const innocent = others.find((p) => p !== first);
  g.in(first, { t: "nominate", pid: innocent });
  v = g.views();
  for (const p of g.pids) {
    assert.equal(v[p].nomStage, "poll", "the table is polled on the nomination");
    assert.equal(v[p].nominee, innocent);
    assert.equal(v[p].agreed, 1, "the nominator is in on their own nomination");
  }
  assert.equal(v[first].myagree, 1, "and is not asked again");
  const rest = g.pids.filter((p) => p !== first);
  g.in(rest[0], { t: "agree", in: true });
  g.in(rest[1], { t: "agree", in: false });
  g.in(rest[2], { t: "agree", in: false });
  v = g.views();
  for (const p of g.pids) {
    assert.equal(v[p].nomStage, "pick", "2 of 3 needed: the turn passes on");
    assert.equal(v[p].nominator, g.pids[1], "to the next seat");
  }
  // Second seat nominates the spy and the table backs it.
  const second = v[g.pids[0]].nominator;
  assert.notEqual(second, first);
  g.in(second, { t: "nominate", pid: spy === second ? others[0] : spy });
  if (spy === second) {
    // The spy drew the second turn; their nomination of an innocent must not stick
    // either, so back it fully and check an innocent condemned scores the spy.
    for (const p of g.pids.filter((x) => x !== second)) g.in(p, { t: "agree", in: true });
    const gain = {};
    gain[spy] = 1;
    const rv = checkReveal(g, spy, loc, "escaped", gain);
    assert.equal(rv[spy].blamedNick, "P" + others[0], "the reveal names who was condemned");
  } else {
    for (const p of g.pids.filter((x) => x !== second)) g.in(p, { t: "agree", in: true });
    const gain = {};
    for (const p of others) gain[p] = 1;
    const rv = checkReveal(g, spy, loc, "caught", gain);
    assert.equal(rv[spy].blamedNick, "P" + spy, "the reveal names who was condemned");
  }
}

// ---- 6: everyone nominates in vain -> the spy wins outright ------------------------
{
  const g = await table(3);
  const { spy, loc } = g.readCards();
  let v = toNominations(g);
  const seen = [];
  for (let turn = 0; turn < 3; turn++) {
    v = g.views();
    assert.equal(v[g.pids[0]].nomStage, "pick", "turn " + turn + " asks for a nomination");
    const nom = v[g.pids[0]].nominator;
    assert.ok(!seen.includes(nom), "each seat nominates exactly once");
    seen.push(nom);
    const target = g.pids.find((p) => p !== nom);
    g.in(nom, { t: "nominate", pid: target });
    // Everybody else refuses, so it never reaches the threshold.
    for (const p of g.pids.filter((x) => x !== nom)) g.in(p, { t: "agree", in: false });
  }
  assert.equal(seen.length, 3, "all three seats had a turn");
  const gain = {};
  gain[spy] = 1;
  const rv = checkReveal(g, spy, loc, "escaped", gain);
  assert.equal(rv[spy].blamedNick, undefined, "nobody was pinned, so nobody is named");
}

// ---- 7: a silent room cannot stall -- every wait has a deadline --------------------
{
  const g = await table(3);
  // Nobody taps OK: the card stage times out into the questioning by itself.
  let v = g.views();
  assert.equal(v[1].stage, "card");
  g.tickTo(v[1].deadline);
  v = g.views();
  for (const p of g.pids) assert.equal(v[p].stage, "talk", "the card stage times out");
  const spy = g.pids.find((p) => v[p].spy);
  const loc = v[g.pids.find((p) => !v[p].spy)].loc;
  // Nobody presses anything, nobody nominates, nobody answers a poll: the whole
  // endgame still resolves on timers alone, and the spy wins it.
  toNominations(g);
  for (let turn = 0; turn < 3; turn++) {
    v = g.views();
    if (v[g.pids[0]].stage === "reveal") break;
    g.tickTo(v[g.pids[0]].deadline); // nomination turn expires
  }
  const gain = {};
  gain[spy] = 1;
  checkReveal(g, spy, loc, "escaped", gain);
}

// ---- 8: the quorum, and the spy walking out ---------------------------------------
{
  const e = await newEngine();
  e.reset();
  for (const p of [1, 2, 3]) e.join(p, "P" + p);
  e.selectGame(SF);
  e.contentClear();
  e.contentPack(SF, "Test");
  for (const it of ITEMS) e.contentItem(it);
  e.disconnect(3);
  e.input(1, { t: "ready", ready: true });
  let out = e.input(2, { t: "ready", ready: true });
  assert.equal(lastToWs(out, 1, "spyfall").msg.phase, "lobby", "two players can't start");
  assert.equal(lastToWs(out, 1, "spyfall").msg.need, 3, "the lobby advertises the quorum");
}
{
  const g = await table(3);
  const { spy, others } = g.readCards();
  g.out = g.e.disconnect(spy);
  for (const p of others) {
    const m = lastToWs(g.out, p, "spyfall");
    assert.equal(m.msg.stage, "reveal", "the round ends when the spy walks out");
    assert.equal(m.msg.outcome, "aborted", "an abandoned round has no result");
    assert.equal(m.msg.mygain, 0, "nobody scores off an abandoned round");
    for (const row of m.msg.scores) assert.equal(row.score, 0, "no score moved");
  }
}

console.log("spyfall: all checks passed");
