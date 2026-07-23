// Reactions go to whoever shares your screen. Three players: 1 and 2 in a Connect
// Four match, 3 left in the lobby. A reaction from inside the match must reach only
// the two players in it, and a reaction from the lobby must not leak into the match.
//
// Before this rule existed the engine broadcast every reaction to everyone, so both
// assertions below would fail — which is the point of testing it here rather than
// with three phones and a dev board.
import assert from "node:assert/strict";
import { newEngine } from "./harness-lib.mjs";

const HA_GAME_CONNECT4 = 2;

/** pids that received a {t:"emoji"} in this batch, via either sink. */
function emojiRecipients(items) {
  const pids = new Set();
  for (const it of items) {
    if (it.to === "ws" && it.msg && it.msg.t === "emoji") pids.add(it.id);
    if (it.to === "all" && it.msg && it.msg.t === "emoji") pids.add("ALL");
  }
  return pids;
}

const e = await newEngine();
e.reset();
e.selectGame(HA_GAME_CONNECT4);
e.join(1, "ana");
e.join(2, "bo");
e.join(3, "cy");

// 1 and 2 pair off; 3 stays in the lobby.
e.input(1, { t: "challenge", to: 2 });
e.input(2, { t: "accept", from: 1 });

// --- a reaction from inside the match ---------------------------------------
const fromMatch = e.input(1, { t: "react", emoji: "🔥" });
const matchGot = emojiRecipients(fromMatch);
assert.ok(!matchGot.has("ALL"), "reactions must not be broadcast any more");
assert.deepEqual(
  [...matchGot].sort(),
  [1, 2],
  "a reaction inside a match reaches exactly its two players",
);

// --- a reaction from the lobby ----------------------------------------------
const fromLobby = e.input(3, { t: "react", emoji: "🎉" });
const lobbyGot = emojiRecipients(fromLobby);
assert.ok(!lobbyGot.has("ALL"), "reactions must not be broadcast any more");
assert.deepEqual(
  [...lobbyGot].sort(),
  [3],
  "a lobby reaction stays in the lobby (player 3 is alone there)",
);

// --- the sender is always included, or you can't see your own reaction -------
assert.ok(matchGot.has(1), "sender receives their own reaction (the client waits for the echo)");

// --- the payload names the sender -------------------------------------------
const one = fromMatch.find((o) => o.to === "ws" && o.msg.t === "emoji");
assert.equal(one.msg.nick, "ANA", "reaction carries the sender's nickname");
assert.ok(one.msg.avatar, "reaction carries the sender's avatar");

console.log("reactions: OK");
