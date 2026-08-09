// Game-change vote: a cross-cutting proposal that lets players switch the active game from
// their phones by majority vote, freezing the active game while it is pending. This is the
// one sanctioned phone->host action, gated behind a strict majority of the OTHER players.
// Covers: propose pauses the game + overlay counts, approve switches, reject/timeout resume,
// the lone-proposer shortcut, the proposer cancelling their own proposal, proposing "none"
// (back to the lobby), and the pause (game intents are ignored during the vote).
import assert from "node:assert/strict";
import { newEngine, lastToWs } from "./harness-lib.mjs";

const SPECTRUM = 13; // HA_GAME_SPECTRUM -- the game the host starts on, to be voted away from

function joinN(e, n) {
  for (let i = 1; i <= n; i++) e.join(i, "P" + i);
}

// --- Approve: 4 players; proposer + 2 of the 3 others = strict majority. ---
{
  const e = await newEngine();
  e.reset();
  joinN(e, 4);
  e.selectGame(SPECTRUM); // host picks Spectrum (authoritative, no vote)

  let out = e.input(1, { t: "proposeGame", game: "wyr" });
  for (const pid of [1, 2, 3, 4]) {
    const gv = lastToWs(out, pid, "gamevote");
    assert.ok(gv, "player " + pid + " gets the vote overlay");
    assert.equal(gv.msg.proposer, "P1", "proposer nick carried");
    assert.equal(gv.msg.avatar, "🙂", "proposer avatar carried (the voters' line leads with it)");
    assert.equal(gv.msg.game, "wyr", "target game name carried");
  }
  const mine = lastToWs(out, 1, "gamevote").msg;
  assert.equal(mine.youproposed, true, "proposer is flagged");
  assert.equal(mine.yes, 1, "the proposer is an implicit yes");
  assert.equal(mine.others, 3, "three other players");
  assert.equal(mine.need, 2, "two of the others are needed");
  const voter = lastToWs(out, 2, "gamevote").msg;
  assert.equal(voter.youproposed, false, "a voter is not the proposer");
  assert.equal(voter.youvoted, false, "a voter has not voted yet");

  // Pause: a game intent is ignored while the vote is pending.
  out = e.input(2, { t: "ready", ready: true });
  assert.equal(lastToWs(out, 2, "spectrum"), undefined, "game intents are frozen during the vote");

  e.input(2, { t: "voteGame", ok: true });
  out = e.input(3, { t: "voteGame", ok: true });
  for (const pid of [1, 2, 3, 4]) {
    const lob = lastToWs(out, pid, "lobby");
    assert.ok(lob && lob.msg.game === "wyr", "player " + pid + " is switched to WYR on approval");
  }
  // The host-facing uart EVENT must carry the numeric game id so the Flipper (which has no
  // name->id map) can update its displayed active game and not revert the vote on a reboot.
  const ev = out
    .filter((o) => o.to === "uart" && o.kind === "event" && o.json && o.json.gamevote === "approved")
    .pop();
  assert.ok(ev, "approve emits a gamevote uart event for the Flipper");
  assert.equal(ev.json.id, 8, "the gamevote event carries the numeric game id (wyr = 8)");
  console.log("gamevote: approve switches the game");
}

// --- Reject: two of the three others vote No -> majority impossible -> stays. ---
{
  const e = await newEngine();
  e.reset();
  joinN(e, 4);
  e.selectGame(SPECTRUM);
  e.input(1, { t: "proposeGame", game: "wyr" });
  e.input(2, { t: "voteGame", ok: false });
  const out = e.input(3, { t: "voteGame", ok: false });
  for (const pid of [1, 2, 3, 4]) {
    const lob = lastToWs(out, pid, "lobby");
    assert.ok(lob && lob.msg.game === "spectrum", "previous game (Spectrum) resumes on reject");
  }
  assert.ok(lastToWs(out, 1, "spectrum"), "the frozen Spectrum game resumes (state pushed again)");
  console.log("gamevote: reject resumes the previous game");
}

// --- Timeout is treated as reject. ---
{
  const e = await newEngine();
  e.reset();
  joinN(e, 4);
  e.selectGame(SPECTRUM);
  e.input(1, { t: "proposeGame", game: "wyr" });
  const out = e.tick(30000); // > GAMEVOTE_SECS (25s)
  for (const pid of [1, 2, 3, 4])
    assert.ok(lastToWs(out, pid, "lobby").msg.game === "spectrum", "timeout resumes the game");
  console.log("gamevote: timeout resumes the previous game");
}

// --- Lone proposer approves immediately (no others to vote). ---
{
  const e = await newEngine();
  e.reset();
  joinN(e, 1);
  e.selectGame(SPECTRUM);
  const out = e.input(1, { t: "proposeGame", game: "wyr" });
  assert.ok(lastToWs(out, 1, "lobby").msg.game === "wyr", "a lone proposer switches at once");
  console.log("gamevote: lone proposer approves immediately");
}

// --- Proposer leaving cancels the vote. ---
{
  const e = await newEngine();
  e.reset();
  joinN(e, 3);
  e.selectGame(SPECTRUM);
  e.input(1, { t: "proposeGame", game: "wyr" });
  const out = e.disconnect(1);
  for (const pid of [2, 3])
    assert.ok(lastToWs(out, pid, "lobby").msg.game === "spectrum", "proposer leaving cancels the vote");
  console.log("gamevote: proposer leaving cancels the vote");
}

// --- Proposer withdraws: their Cancel button sends voteGame{ok:false}, which is the reject
//     path. A voteGame{ok:true} from them stays a no-op (their yes is already implicit). ---
{
  const e = await newEngine();
  e.reset();
  joinN(e, 4);
  e.selectGame(SPECTRUM);
  e.input(1, { t: "proposeGame", game: "wyr" });

  let out = e.input(1, { t: "voteGame", ok: true }); // the proposer's yes changes nothing
  assert.equal(lastToWs(out, 1, "lobby"), undefined, "an OK from the proposer does not resolve it");

  out = e.input(1, { t: "voteGame", ok: false }); // Cancel
  for (const pid of [1, 2, 3, 4])
    assert.ok(lastToWs(out, pid, "lobby").msg.game === "spectrum",
      "player " + pid + " is back on Spectrum after the proposer cancels");
  assert.ok(lastToWs(out, 1, "spectrum"), "the frozen game resumes immediately on cancel");

  // Withdrawn means gone: the same player can open a fresh proposal right away.
  out = e.input(1, { t: "proposeGame", game: "wyr" });
  assert.ok(lastToWs(out, 1, "gamevote"), "a new proposal opens after a cancel");
  console.log("gamevote: the proposer can cancel their own proposal");
}

// --- "Back to Lobby": propose the "none" game, voted like any other change. ---
{
  const e = await newEngine();
  e.reset();
  joinN(e, 3);
  e.selectGame(SPECTRUM);

  let out = e.input(1, { t: "proposeGame", game: "none" });
  const gv = lastToWs(out, 2, "gamevote");
  assert.ok(gv, '"none" is a valid proposal target');
  assert.equal(gv.msg.game, "none", 'the target round-trips as the name "none"');
  assert.equal(gv.msg.need, 2, "both of the two others are needed");

  e.input(2, { t: "voteGame", ok: true });
  out = e.input(3, { t: "voteGame", ok: true });
  for (const pid of [1, 2, 3])
    assert.ok(lastToWs(out, pid, "lobby").msg.game === "none", "approval drops back to the lobby");
  const evNone = out
    .filter((o) => o.to === "uart" && o.kind === "event" && o.json && o.json.gamevote === "approved")
    .pop();
  assert.ok(evNone, "approving 'none' emits a gamevote uart event");
  assert.equal(evNone.json.id, 0, "the event carries id 0 (HA_GAME_NONE) for back-to-lobby");

  // Already there: proposing the active game -- "none" included -- is refused outright.
  out = e.input(1, { t: "proposeGame", game: "none" });
  assert.equal(lastToWs(out, 2, "gamevote"), undefined, "no vote for the game already active");
  out = e.input(1, { t: "proposeGame", game: "nope" });
  assert.equal(lastToWs(out, 2, "gamevote"), undefined, "no vote for an unknown game name");
  console.log("gamevote: back to lobby is proposed and voted like any other game");
}

console.log("gamevote: all checks passed");
