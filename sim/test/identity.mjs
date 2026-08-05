// One phone = one player. A phone can reach the portal from more than one browser
// context at a time -- iOS pops a captive mini-browser whose storage is separate
// from Safari's, and a second tab is a third context -- and a phone that drops
// (screen lock, WiFi off) leaves a socket the ESP only notices minutes later, when
// TCP times out. Both used to mint a fresh player, so one phone showed up in the
// lobby two or three times.
//
// The engine keys identity on the device instead of on the socket: the firmware
// resolves each connection to the station's MAC and hands the engine an opaque device
// key, which every browser context on that phone shares. This test covers the three
// rules that follow (rebind, distinct devices, stale disconnect) plus the
// unknown-device fallback.
//
// Note the two numbering schemes below: SOCK_* are wsIds (connections), pids are the
// engine's player ids handed out in join order. They are deliberately not aligned
// here, because the whole point is that one player can outlive one connection.
import assert from "node:assert/strict";
import { newEngine, macKey, lastToWs } from "./harness-lib.mjs";

const HA_GAME_C4 = 2; // HA_GAME_CONNECT4 in ha_proto.h

const PHONE_A = macKey(0x02, 0, 0, 0, 0xA1, 0x01); // ANA's phone
const PHONE_B = macKey(0x02, 0, 0, 0, 0xB0, 0x02); // BO's phone
const PHONE_C = macKey(0x02, 0, 0, 0, 0xC1, 0x03); // CY's phone

const SOCK_ANA = 1; // ANA's first browser (whichever one she pressed Play in)
const SOCK_BO = 2;
const SOCK_ANA_CAPTIVE = 3; // the captive mini-browser on ANA's phone, later
const SOCK_CY = 4;

const PID_ANA = 1, PID_BO = 2, PID_CY = 3;

/** The roster from the most recent lobby push (pushAll unicasts it per socket). */
function roster(items) {
  const lob = items.filter((o) => o.to === "ws" && o.msg && o.msg.t === "lobby").pop();
  return lob ? lob.msg.players : [];
}
const scoreOf = (items, pid) => (roster(items).find((p) => p.pid === pid) || {}).score;

const e = await newEngine();
e.reset();
e.setDevice(SOCK_ANA, PHONE_A);
e.setDevice(SOCK_ANA_CAPTIVE, PHONE_A); // same phone as SOCK_ANA
e.setDevice(SOCK_BO, PHONE_B);
e.setDevice(SOCK_CY, PHONE_C);

e.selectGame(HA_GAME_C4);
e.join(SOCK_ANA, "ana");
e.join(SOCK_BO, "bo");

// Give ANA a score to protect: a vertical four in column 0 (see duel.mjs).
let won = [];
e.input(SOCK_ANA, { t: "challenge", to: PID_BO });
e.input(SOCK_BO, { t: "accept", from: PID_ANA });
for (let i = 0; i < 4; i++) {
  won = won.concat(e.input(SOCK_ANA, { t: "move", n: 0 }));
  if (i < 3) won = won.concat(e.input(SOCK_BO, { t: "move", n: 1 }));
}
const earned = scoreOf(won, PID_ANA);
assert.ok(earned > 0, "setup: winning the duel scored ANA some points");
assert.equal(roster(won).length, 2, "setup: two phones, two players");

// --- (a) a second context on the same phone rebinds, it does not duplicate ------
// The captive mini-browser has its own empty storage, so it says hello with a name
// of its own. It must land on ANA's player anyway.
const second = e.join(SOCK_ANA_CAPTIVE, "ghost");

const w = lastToWs(second, SOCK_ANA_CAPTIVE, "welcome");
assert.ok(w, "the second context gets a welcome on its own socket");
assert.equal(w.msg.pid, PID_ANA, "it is handed the pid the phone already had");
assert.equal(w.msg.nick, "ANA", "and the nick, so the new context shows the same name");
assert.ok(w.msg.avatar, "and the avatar, so it shows the same identity everywhere");
assert.equal(
  roster(second).length,
  2,
  "the roster still holds two players -- one phone did not become two",
);
assert.equal(scoreOf(second, PID_ANA), earned, "the score survives the rebind");
assert.ok(
  !second.some((o) => o.to === "uart" && o.kind === "join"),
  "a rebind is not a new player, so the Flipper is not told anyone joined",
);
const logged = second.find((o) => o.to === "log");
assert.ok(logged, "the consolidation is traced to the serial console");
assert.equal(logged.kind, "consolidated", "and is traced as a consolidation, not a join");
assert.equal(logged.pid, PID_ANA, "naming the player the socket was folded into");
assert.equal(logged.device, PHONE_A, "and the device it came from");

// State now goes to the new socket, and the old context can no longer act.
const moved = e.input(SOCK_ANA_CAPTIVE, { t: "react", emoji: "🔥" });
assert.ok(
  moved.some(
    (o) => o.to === "ws" && o.id === SOCK_ANA_CAPTIVE && o.msg.t === "emoji" && o.msg.nick === "ANA",
  ),
  "the new socket now speaks for the player",
);
assert.deepEqual(
  e.input(SOCK_ANA, { t: "react", emoji: "🎉" }),
  [],
  "the superseded socket owns nobody and is ignored",
);

// --- (b) two different phones are still two players ----------------------------
const third = e.join(SOCK_CY, "cy");
assert.equal(roster(third).length, 3, "a different device is a different player");
assert.equal(lastToWs(third, SOCK_CY, "welcome").msg.pid, PID_CY, "and gets its own pid");
assert.equal(lastToWs(third, SOCK_CY, "welcome").msg.nick, "CY", "under its own name");
assert.deepEqual(
  third.filter((o) => o.to === "log").map((o) => [o.kind, o.pid, o.device]),
  [["join", PID_CY, PHONE_C]],
  "and is traced as a plain join, not a consolidation",
);

// --- (c) the old socket's late close must not remove the rebound player --------
// This is the ghost-player half of the bug: iOS reports the dead socket long after
// the phone has already come back on a new one.
const closed = e.disconnect(SOCK_ANA);
assert.ok(
  !closed.some((o) => o.to === "uart" && o.kind === "leave"),
  "a stale socket closing does not report a leave",
);
// `leaveGame` from a player who is in no match is a no-op that still pushes the
// lobby -- the cheapest way to ask the engine for a fresh roster snapshot.
const after = e.input(SOCK_CY, { t: "leaveGame" });
assert.ok(roster(after).some((p) => p.pid === PID_ANA), "the player is still in the lobby");
assert.equal(scoreOf(after, PID_ANA), earned, "with their score intact");

// The live socket closing does still remove them, exactly as before.
const reallyGone = e.disconnect(SOCK_ANA_CAPTIVE);
assert.ok(
  reallyGone.some((o) => o.to === "uart" && o.kind === "leave" && o.pid === PID_ANA),
  "the player's current socket closing still removes them",
);
assert.ok(!roster(reallyGone).some((p) => p.pid === PID_ANA), "and drops them from the roster");

// --- (d) an unknown device falls back to one player per connection -------------
// The firmware reports 0 when it cannot identify the phone behind a connection.
// Those clients must not all collapse into a single player.
const e2 = await newEngine();
e2.reset();
e2.setDevice(1, 0);
e2.setDevice(2, 0);
e2.join(1, "ana");
const anon = e2.join(2, "bo");
assert.equal(roster(anon).length, 2, "an unknown device keeps the old per-connection behaviour");
assert.equal(lastToWs(anon, 2, "welcome").msg.nick, "BO", "and each keeps its own name");

console.log("identity: OK");
