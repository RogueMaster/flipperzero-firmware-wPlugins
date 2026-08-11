// Werewolf: hidden-role social deduction. Roles are dealt on the ESP and the
// per-player serializer is the only thing standing between a villager and the
// wolf list -- so most of this file is a standing audit of every payload the
// engine emits, not just a happy-path walk.
//
// Covered: the deal (wolf/seer/doctor counts by table size), the secrecy
// invariant (no role, reading, shield or pack tally in a payload not entitled to
// it), the doctor blocking a kill and the no-repeat restriction, the fixed-length
// night, the live pack tally, the quiet first night at a small table, a day tie
// hanging nobody, the hammer ending a day early, both win conditions, and the
// last werewolf disconnecting.
import assert from "node:assert/strict";
import { newEngine, lastToWs } from "./harness-lib.mjs";

const WW = 18;
const VILLAGER = 1, WOLF = 2, SEER = 3, DOCTOR = 4;
const NICKS = ["ALICE", "BOB", "CARA", "DAN", "EVE", "FRANK", "GINA", "HUGO", "IVY"];

/**
 * Drive one game. wsId doubles as the pid (the engine hands them out in join
 * order). Every drained batch is absorbed: the latest view per player is kept
 * for assertions, and each one is run past the secrecy audit below.
 */
function mkGame(e, n) {
  const g = {
    e, n, ms: 0,
    view: {},                    // pid -> latest {t:"werewolf"} message
    role: {},                    // pid -> role, learned from that player's own myrole
    dead: new Set(),             // pids publicly known to be out (alive:false)
    over: false,
  };

  // `sock` is the wsId the payload went to; `m.you` is that socket's pid. They are
  // the same number in every scenario except a recycled pid, so trust `m.you`.
  function audit(sock, m) {
    const me = (m.you !== undefined) ? m.you : sock;
    if (m.myrole !== undefined && m.myrole !== 0) g.role[me] = m.myrole;
    if (m.phase === "final") g.over = true;
    (m.players || []).forEach(function (p) { if (p.in && !p.alive) g.dead.add(p.pid); });
    if (m.phase !== "play") return;
    // The invariant: a role in someone else's payload is only ever legitimate for
    // a fellow werewolf or for a player who is already dead.
    (m.players || []).forEach(function (p) {
      if (p.role === undefined) return;
      const ok = p.pid === me || g.dead.has(p.pid) ||
        (g.role[me] === WOLF && p.role === WOLF);
      assert.ok(ok, `role leak: pid ${me} (role ${g.role[me]}) saw pid ${p.pid} as role ${p.role}`);
    });
    if (m.check !== undefined) assert.equal(g.role[me], SEER, "only the seer gets a reading");
    for (const k of ["myguard", "lastguard"]) {
      if (m[k] !== undefined) assert.equal(g.role[me], DOCTOR, `only the doctor sees ${k}`);
    }
    for (const k of ["packvotes", "mykill", "packsize"]) {
      if (m[k] !== undefined) assert.equal(g.role[me], WOLF, `only wolves see ${k}`);
    }
  }

  function absorb(items) {
    for (const o of items) {
      if (o.to === "ws" && o.msg && o.msg.t === "werewolf") {
        g.view[o.id] = o.msg;
        audit(o.id, o.msg);
      }
    }
    return items;
  }

  g.tick = (to) => { g.ms = to; return absorb(e.tick(to)); };
  g.join = (wsId, nick) => absorb(e.join(wsId, nick));
  g.send = (pid, obj) => absorb(e.input(pid, obj));
  g.drop = (pid) => absorb(e.disconnect(pid));
  g.pids = () => { const a = []; for (let i = 1; i <= n; i++) a.push(i); return a; };
  g.of = (role) => g.pids().filter((p) => g.role[p] === role);
  g.living = () => g.pids().filter((p) => g.view[p] && g.view[p].alive);
  g.stage = () => g.view[1].stage;
  g.row = (viewer, pid) => g.view[viewer].players.find((x) => x.pid === pid);
  return g;
}

/** Join n players, ready everyone up, and run the countdown into the role reveal. */
async function start(n) {
  const e = await newEngine();
  e.reset();
  for (let i = 1; i <= n; i++) e.join(i, NICKS[i - 1]);
  e.selectGame(WW);
  const g = mkGame(e, n);
  for (let i = 1; i <= n; i++) g.send(i, { t: "ready", ready: true });
  for (let ms = 1000; ms <= 3000; ms += 1000) g.tick(ms);
  return g;
}

/** Advance past the current phase deadline (all timers are generous). */
function skip(g) { return g.tick(g.ms + 400000); }

/** Walk a fresh game to the start of night 1. */
async function toNight(n) {
  const g = await start(n);
  skip(g); // the role-reveal window expires
  assert.equal(g.stage(), "night", "night falls after the role reveal");
  return g;
}

// ---------------------------------------------------------------------------
// 1. Eight players: the deal, secrecy, a fixed-length night, a live pack tally,
//    a tied day that hangs nobody, the doctor's no-repeat rule, a village win
// ---------------------------------------------------------------------------
{
  const g = await start(8);
  for (const p of g.pids()) {
    assert.equal(g.view[p].phase, "play", "dealt in after the countdown");
    assert.equal(g.view[p].stage, "roles", "the private role reveal comes first");
    assert.ok(g.view[p].myrole >= 1 && g.view[p].myrole <= 4, "everyone holds a role");
  }
  // 8 players -> 2 werewolves, 1 seer, 1 doctor, 4 villagers.
  assert.equal(g.of(WOLF).length, 2, "two werewolves at eight players");
  assert.equal(g.of(SEER).length, 1, "exactly one seer");
  assert.equal(g.of(DOCTOR).length, 1, "exactly one doctor from six players up");
  assert.equal(g.of(VILLAGER).length, 4, "the rest are villagers");

  const wolves = g.of(WOLF), seer = g.of(SEER)[0], doc = g.of(DOCTOR)[0];
  const village = g.pids().filter((p) => g.role[p] !== WOLF);

  // Werewolves see each other -- and nobody else.
  for (const w of wolves) {
    const seen = g.view[w].players.filter((p) => p.role !== undefined).map((p) => p.pid).sort();
    assert.deepEqual(seen, wolves.slice().sort(), "a wolf sees exactly the pack");
  }
  // Before anyone has died, a villager's payload carries no role but their own.
  for (const v of village) {
    const seen = g.view[v].players.filter((p) => p.role !== undefined).map((p) => p.pid);
    assert.deepEqual(seen, [v], `pid ${v} sees only their own role`);
  }

  // ---- night 1 ----
  skip(g);
  assert.equal(g.stage(), "night", "night falls");
  assert.equal(g.view[1].nokill, undefined, "eight players hunt on night one");
  const nightDeadline = g.view[1].deadline;
  assert.equal(g.view[1].dur, 60, "the night window is a fixed 60s");
  assert.equal(g.view[wolves[0]].owe, true, "a wolf owes an action");
  assert.equal(g.view[village.find((p) => g.role[p] === VILLAGER)].owe, false,
    "a plain villager owes nothing at night");

  const prey = village.filter((p) => p !== seer && p !== doc)[0];
  const shielded = village.filter((p) => p !== prey && p !== doc)[0];

  // The seer checks a wolf; only the seer is told.
  g.send(seer, { t: "see", n: wolves[0] });
  for (const p of g.pids()) {
    if (p === seer) {
      assert.equal(g.view[p].check.pid, wolves[0], "the seer gets their reading");
      assert.equal(g.view[p].check.wolf, true, "...and it names the wolf");
    } else {
      assert.equal(g.view[p].check, undefined, `pid ${p} must not see the reading`);
    }
  }
  // The doctor shields somebody other than the wolves' target; only they see it.
  g.send(doc, { t: "guard", n: shielded });
  assert.equal(g.view[doc].myguard, shielded, "the doctor sees their own shield");
  for (const p of g.pids()) {
    if (p !== doc) assert.equal(g.view[p].myguard, undefined, `pid ${p} must not see the shield`);
  }

  // A villager cannot fake a night kill, and a wolf cannot eat the pack.
  g.send(prey, { t: "kill", n: wolves[0] });
  g.send(wolves[0], { t: "kill", n: wolves[1] });
  assert.equal(g.view[wolves[0]].packvotes.length, 0, "neither tap was accepted");
  // The pack tally moves the moment one wolf taps -- their only way to converge,
  // since they are sitting in the same room and cannot say a word.
  g.send(wolves[0], { t: "kill", n: prey });
  assert.deepEqual(g.view[wolves[1]].packvotes, [{ by: wolves[0], pid: prey }],
    "the other wolf sees the pick immediately");
  assert.equal(g.view[wolves[1]].packsize, 2, "and how big the pack still is");
  assert.equal(g.view[prey].packvotes, undefined, "the target sees nothing of it");
  g.send(wolves[1], { t: "kill", n: prey });

  // Every night actor has now acted -- and the night still runs its full length,
  // because a short night would tell the room how many specials are alive.
  assert.equal(g.stage(), "night", "the night does not end early");
  assert.equal(g.view[1].deadline, nightDeadline, "and its deadline never moved");
  g.tick(nightDeadline - 1000);
  assert.equal(g.stage(), "night", "still night a second before the deadline");

  skip(g);
  assert.equal(g.stage(), "dawn", "the night ends on its clock");
  assert.equal(g.view[1].victim, prey, "the agreed victim is taken");
  assert.equal(g.view[1].dawnkind, "killed", "and the room is told it was a kill");
  for (const p of g.pids()) {
    const row = g.row(p, prey);
    assert.equal(row.alive, false, "the body is public");
    assert.equal(row.role, g.role[prey], "a dead player's role is revealed to everyone");
  }

  // ---- day 1: a tied vote hangs nobody ----
  skip(g);
  assert.equal(g.stage(), "day", "day breaks");
  assert.equal(g.view[1].voters, 7, "seven still alive");
  assert.equal(g.view[1].needed, 4, "a hammer is a strict majority of them");
  assert.equal(g.view[1].waiting, 7, "nobody has voted yet");
  assert.equal(g.view[1].dur, 60 + 20 * 7, "the day is 60s + 20s per living player");

  const camp = g.living().filter((p) => p !== wolves[0] && p !== wolves[1]); // 5 villagers
  // Three votes each way with one abstention: a dead heat, and short of a hammer.
  const sideA = camp.slice(0, 3), sideB = camp.slice(3).concat([wolves[0]]);
  for (const p of sideA) g.send(p, { t: "accuse", n: wolves[0] });
  for (const p of sideB) g.send(p, { t: "accuse", n: wolves[1] });
  assert.equal(g.stage(), "day", "three votes each is short of the hammer");
  assert.equal(g.view[1].waiting, 1, "one player has not voted");
  skip(g);
  assert.equal(g.stage(), "dusk", "the day ends on its clock");
  assert.equal(g.view[1].lynched, 0, "a tied village hangs nobody");
  assert.equal(g.row(1, wolves[0]).alive, true, "the tied-for-first are both spared");
  assert.equal(g.row(1, wolves[1]).alive, true);

  // ---- night 2: the doctor may not shield the same player twice running ----
  skip(g);
  assert.equal(g.stage(), "night", "night two");
  assert.equal(g.view[doc].lastguard, shielded, "the doctor is reminded who is off-limits");
  g.send(doc, { t: "guard", n: shielded });
  assert.equal(g.view[doc].myguard, 0, "shielding the same player twice is refused");
  const other = g.living().filter((p) => p !== shielded && p !== doc)[0];
  g.send(doc, { t: "guard", n: other });
  assert.equal(g.view[doc].myguard, other, "a different target is fine");

  const nextPrey = g.living().filter(
    (p) => g.role[p] !== WOLF && p !== other && p !== doc && p !== seer)[0];
  for (const w of wolves) g.send(w, { t: "kill", n: nextPrey });
  skip(g); // dawn
  assert.equal(g.view[1].victim, nextPrey, "an unshielded target dies");
  skip(g); // day 2

  // ---- the hammer: a strict majority ends the day the moment it lands ----
  assert.equal(g.stage(), "day");
  const jury = g.living().filter((p) => p !== wolves[0]);
  const need = g.view[1].needed;
  for (let i = 0; i < jury.length && g.stage() === "day"; i++) {
    g.send(jury[i], { t: "accuse", n: wolves[0] });
    if (i + 1 < need) assert.equal(g.stage(), "day", "short of the hammer, the day runs on");
  }
  assert.equal(g.stage(), "dusk", "the hammer ends the day on the spot");
  assert.equal(g.view[1].lynched, wolves[0], "the hammered player is voted out");
  assert.equal(g.row(1, wolves[0]).role, WOLF, "the eliminated player's role is revealed");

  // ---- run it out: the village hammers the second wolf too ----
  for (let guard = 0; guard < 30 && !g.over; guard++) {
    skip(g);
    if (g.over) break;
    if (g.stage() === "day") {
      for (const p of g.living().filter((x) => x !== wolves[1])) {
        g.send(p, { t: "accuse", n: wolves[1] });
        if (g.stage() !== "day") break;
      }
    }
  }
  for (const p of g.pids()) {
    const m = g.view[p];
    assert.equal(m.phase, "final", "the game ends with the last wolf gone");
    assert.equal(m.winner, "villagers", "villagers win");
    assert.ok(m.players.every((x) => !x.in || x.role !== undefined), "every role is revealed at the end");
    assert.ok(Array.isArray(m.log) && m.log.length >= 2, "the final carries a night-by-night log");
  }
  // 1 point per surviving winner: the villagers still standing, and nobody else.
  const board = g.view[1].board;
  const survivors = g.view[1].players
    .filter((x) => x.in && x.alive && x.role !== WOLF).map((x) => x.pid);
  for (const row of board) {
    const want = survivors.includes(row.pid) ? 1 : 0;
    assert.equal(row.score, want, `pid ${row.pid} scored ${row.score}, expected ${want}`);
  }
  assert.ok(survivors.length >= 1, "somebody survived to score");
}

// ---------------------------------------------------------------------------
// 2. The doctor blocks a kill, and an idle pack kills nobody
// ---------------------------------------------------------------------------
{
  const g = await toNight(8);
  const wolves = g.of(WOLF), doc = g.of(DOCTOR)[0];
  const mark = g.living().filter((p) => g.role[p] === VILLAGER)[0];
  for (const w of wolves) g.send(w, { t: "kill", n: mark });
  g.send(doc, { t: "guard", n: mark });
  skip(g);
  assert.equal(g.stage(), "dawn");
  assert.equal(g.view[1].victim, 0, "the shielded target survives");
  assert.equal(g.view[1].dawnkind, "saved", "and the room is told the attack was blocked");
  assert.equal(g.row(1, mark).alive, true, "they are still in the game");
  const bystander = g.living().filter((p) => p !== mark)[0];
  assert.equal(g.row(bystander, mark).role, undefined, "surviving does not reveal their role");
}
{
  const g = await toNight(8);
  skip(g); // nobody acts at all
  assert.equal(g.view[1].victim, 0, "an idle pack takes nobody");
  assert.equal(g.view[1].dawnkind, "quiet", "which reads differently from a blocked attack");
}

// ---------------------------------------------------------------------------
// 3. Five players: no doctor, a first night with no hunt, and a wolf win
// ---------------------------------------------------------------------------
{
  const g = await toNight(5);
  assert.equal(g.of(WOLF).length, 1, "one werewolf at five players");
  assert.equal(g.of(SEER).length, 1, "still a seer");
  assert.equal(g.of(DOCTOR).length, 0, "no doctor at a five-player table");
  assert.equal(g.of(VILLAGER).length, 3);
  const wolf = g.of(WOLF)[0];

  // Night one at a small table is a meeting, not a hunt.
  assert.equal(g.view[1].nokill, true, "the room is told there is no hunt tonight");
  assert.equal(g.view[wolf].owe, false, "so the wolf owes nothing");
  g.send(wolf, { t: "kill", n: g.living().filter((p) => p !== wolf)[0] });
  assert.equal(g.view[wolf].packvotes.length, 0, "a kill tap is refused on the quiet night");
  skip(g);
  assert.equal(g.view[1].victim, 0, "nobody dies on night one");
  assert.equal(g.view[1].dawnkind, "nokill");
  assert.equal(g.living().length, 5, "all five are still in");

  // Someone wanders in mid-game: no role, no vote, just a seat.
  g.join(9, "LATE");
  assert.equal(g.row(1, 6).in, false, "a late joiner is a spectator");
  assert.equal(g.row(1, 6).role, undefined, "and holds no role");

  // From here the village never agrees on anybody, so the pack wins by attrition.
  const stages = new Set();
  for (let guard = 0; guard < 40 && !g.over; guard++) {
    stages.add(g.stage());
    if (g.stage() === "night") {
      const target = g.living().filter((p) => g.role[p] !== WOLF)[0];
      if (target) g.send(wolf, { t: "kill", n: target });
    }
    if (g.over) break;
    skip(g);
  }
  for (const st of ["night", "dawn", "day", "dusk"]) {
    assert.ok(stages.has(st), `the ${st} phase ran`);
  }
  for (const p of [1, 2, 3, 4, 5]) {
    assert.equal(g.view[p].phase, "final", "the game ended");
    assert.equal(g.view[p].winner, "wolves", "werewolves win once they are not outnumbered");
  }
  const board = g.view[wolf].board;
  assert.equal(board.find((r) => r.pid === wolf).score, 1, "the surviving wolf scores 1");
  for (const r of board) {
    if (r.pid !== wolf && r.pid !== 6) assert.equal(r.score, 0, "the losing village scores nothing");
  }
}

// ---------------------------------------------------------------------------
// 4. The deal by table size: a doctor from six up, a hunt from seven up
// ---------------------------------------------------------------------------
{
  const g = await toNight(6);
  assert.equal(g.of(WOLF).length, 1, "one werewolf at six");
  assert.equal(g.of(DOCTOR).length, 1, "a doctor joins the deal at six");
  assert.equal(g.of(SEER).length, 1);
  assert.equal(g.of(VILLAGER).length, 3);
  assert.equal(g.view[1].nokill, true, "six is still a quiet first night");
}
{
  const g = await toNight(7);
  assert.equal(g.of(WOLF).length, 1, "one werewolf at seven");
  assert.equal(g.of(DOCTOR).length, 1);
  assert.equal(g.of(VILLAGER).length, 4);
  assert.equal(g.view[1].nokill, undefined, "seven players hunt from night one");
}

// ---------------------------------------------------------------------------
// 5. The last werewolf walks out mid-game: the village wins by default
// ---------------------------------------------------------------------------
{
  const g = await toNight(5);
  const wolf = g.of(WOLF)[0];
  g.drop(wolf);
  const left = g.pids().filter((p) => p !== wolf);
  for (const p of left) {
    assert.equal(g.view[p].phase, "final", "the game ends the moment the pack is gone");
    assert.equal(g.view[p].winner, "villagers", "villagers win when the last wolf leaves");
  }
  // The vacated pid is genuinely gone, not a ghost still holding a role.
  assert.equal(g.view[left[0]].players.some((x) => x.pid === wolf), false, "the leaver is off the roster");
  for (const r of g.view[left[0]].board) assert.equal(r.score, 1, "every surviving villager scores 1");
}

// ---------------------------------------------------------------------------
// 6. The start gate, driven the way a real session drives it: the HOST picks the
//    game first, then phones join, then they tap ready. A hardware report of
//    "Werewolf does not start" turned out to be a stale served page rather than
//    this gate, but the gate is what a tester sees, so it is pinned here.
// ---------------------------------------------------------------------------
for (const N of [4, 5, 6, 8]) {
  const e = await newEngine();
  e.reset();
  e.selectGame(WW); // host first, exactly like SELECT_GAME arriving over UART
  const g = mkGame(e, N);
  for (let i = 1; i <= N; i++) g.join(i, NICKS[i - 1]);
  assert.equal(g.view[1].phase, "lobby", `N=${N}: joins land in the lobby`);
  assert.equal(g.view[1].enough, N >= 5, `N=${N}: the client is told whether the room is big enough`);
  assert.equal(g.view[1].min, 5, `N=${N}: the minimum is advertised`);

  // Ready up one at a time; the countdown may only arm on the last one.
  for (let i = 1; i <= N; i++) {
    g.send(i, { t: "ready", ready: true });
    const m = g.view[1];
    if (i < N) {
      // The countdown payload drops the roster, so only the lobby can be counted.
      assert.equal(m.phase, "lobby", `N=${N}: not everyone is ready yet`);
      assert.equal(m.players.filter((p) => p.ready).length, i,
        `N=${N}: ${i} ready flags are reflected back`);
    }
  }

  if (N < 5) {
    // Below quorum it must stay put -- and keep saying why.
    for (let ms = 1000; ms <= 20000; ms += 1000) g.tick(ms);
    assert.equal(g.view[1].phase, "lobby", `N=${N}: below quorum the game never starts`);
    assert.equal(g.view[1].enough, false, `N=${N}: and the lobby still says so`);
    continue;
  }

  assert.equal(g.view[1].phase, "countdown", `N=${N}: all ready arms the countdown`);
  for (let ms = 1000; ms <= 3000; ms += 1000) g.tick(ms);
  assert.equal(g.view[1].phase, "play", `N=${N}: the countdown reaches play`);
  assert.equal(g.view[1].stage, "roles", `N=${N}: starting with the private role reveal`);
  for (let i = 1; i <= N; i++) {
    assert.ok(g.view[i].myrole >= 1 && g.view[i].myrole <= 4, `N=${N}: pid ${i} was dealt a role`);
  }
  assert.equal(g.of(WOLF).length >= 1, true, `N=${N}: at least one werewolf`);
  assert.ok(g.of(WOLF).length < g.pids().length - g.of(WOLF).length,
    `N=${N}: the village starts ahead`);
  assert.equal(g.of(SEER).length, 1, `N=${N}: exactly one seer`);
  assert.equal(g.of(DOCTOR).length, N >= 6 ? 1 : 0, `N=${N}: doctor only from six up`);
  assert.equal(g.of(VILLAGER).length >= 1, true, `N=${N}: at least one plain villager`);
  skip(g);
  assert.equal(g.view[1].stage, "night", `N=${N}: and on into the first night`);
}

// Someone walking out during the countdown drops the room below quorum and
// disarms it. (A ready tap cannot: like every other party game, ready is locked
// for the three seconds the countdown is running -- see the phase guard in
// wwReady -- so the leave path is the one that has to work.)
{
  const e = await newEngine();
  e.reset();
  e.selectGame(WW);
  const g = mkGame(e, 5);
  for (let i = 1; i <= 5; i++) g.join(i, NICKS[i - 1]);
  for (let i = 1; i <= 5; i++) g.send(i, { t: "ready", ready: true });
  assert.equal(g.view[1].phase, "countdown", "five ready players arm it");
  g.drop(5);
  assert.equal(g.view[1].phase, "lobby", "a leaver during the countdown disarms it");
  assert.equal(g.view[1].enough, false, "and the lobby says the room is too small again");
  g.join(9, "LATE"); // a new socket, handed the vacated pid 5
  assert.equal(g.view[1].phase, "lobby", "the newcomer has not readied yet");
  assert.equal(g.view[1].enough, true, "but the room is big enough once more");
  assert.equal(g.view[1].players.filter((p) => p.ready).length, 4,
    "the leaver's ready flag did not carry over to whoever took their pid");
  g.send(9, { t: "ready", ready: true });
  assert.equal(g.view[1].phase, "countdown", "and the fifth ready re-arms it");
  for (let ms = 1000; ms <= 4000; ms += 1000) g.tick(ms);
  assert.equal(g.view[1].phase, "play", "the re-armed countdown reaches play");
}

// A fifth player arriving completes the quorum without anyone re-readying.
{
  const e = await newEngine();
  e.reset();
  e.selectGame(WW);
  const g = mkGame(e, 5);
  for (let i = 1; i <= 4; i++) g.join(i, NICKS[i - 1]);
  for (let i = 1; i <= 4; i++) g.send(i, { t: "ready", ready: true });
  assert.equal(g.view[1].phase, "lobby", "four ready players still wait");
  g.join(5, NICKS[4]);
  assert.equal(g.view[1].phase, "lobby", "the newcomer has not readied yet");
  assert.equal(g.view[1].enough, true, "but the room is now big enough");
  g.send(5, { t: "ready", ready: true });
  assert.equal(g.view[1].phase, "countdown", "and the fifth ready arms it");
}

console.log("werewolf: all checks passed");
