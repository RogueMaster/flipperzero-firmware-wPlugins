// Cross-game switching over the shared game-state union (the v1.8.0 RAM refactor).
//
// Every game's runtime state now overlaps in ONE union: only the active game's state is
// ever live, so a game switch must (a) zero the whole union, (b) re-default only the
// incoming game, and (c) never let a handler read another game's aliased bytes. Content
// packs were lifted OUT of that union (they hold Strings and are streamed for every game up
// front), so they must survive every switch.
//
// The per-game tests each drive ONE game and so never exercise the switch. This test cycles
// through all of them under ASan/UBSan and checks that: the lobby follows each switch, the
// packs survive, a live 1v1 match is cleared on switch, a disconnect is safe under any active
// game, and the scoreboard resets. A stale union byte surfaces here as a UBSan "load of value
// N, not valid for bool" or an ASan overflow; an aliasing read surfaces as a wrong assertion.
import assert from "node:assert/strict";
import { newEngine, lastToWs } from "./harness-lib.mjs";

const G = {
  TRIVIA: 1, CONNECT4: 2, TICTACTOE: 3, DOTS: 4, DRAW: 5, PONG: 6, REACT: 7, WYR: 8,
  SCRAMBLE: 9, REVERSI: 10, GUESSCOLOR: 11, BATTLESHIP: 12, SPECTRUM: 13, KMK: 14,
  CHESS: 15, SECRETS: 16, FILLBLANK: 17, WEREWOLF: 18, SPYFALL: 19, FRANKENDRAW: 20,
};
const NAME = {
  [G.TRIVIA]: "trivia", [G.CONNECT4]: "connect4", [G.TICTACTOE]: "tictactoe",
  [G.DOTS]: "dots", [G.DRAW]: "draw", [G.PONG]: "pong", [G.REACT]: "react",
  [G.WYR]: "wyr", [G.SCRAMBLE]: "scramble", [G.REVERSI]: "reversi", [G.GUESSCOLOR]: "gc",
  [G.BATTLESHIP]: "bs", [G.SPECTRUM]: "spectrum", [G.KMK]: "kmk", [G.CHESS]: "chess",
  [G.SECRETS]: "secrets", [G.FILLBLANK]: "fillblank", [G.WEREWOLF]: "werewolf",
  [G.SPYFALL]: "spyfall", [G.FRANKENDRAW]: "frankendraw",
};

const e = await newEngine();
e.reset();

// Load every content pack up front, exactly as the Flipper streams them for a session --
// all nine content games at once, regardless of which game gets picked.
e.contentClear();
e.contentPack(G.TRIVIA, "GK");
e.contentItem(JSON.stringify({ q: "Capital of France?", a: "Paris", b: "London", c: "Berlin", d: "Madrid", answer: "A" }));
e.contentItem(JSON.stringify({ q: "Largest ocean?", a: "Atlantic", b: "Indian", c: "Pacific", d: "Arctic", answer: "C" }));
e.contentPack(G.WYR, "WYR");
for (let i = 0; i < 8; i++) e.contentItem(JSON.stringify({ a: "A" + i, b: "B" + i }));
e.contentPack(G.SCRAMBLE, "Words");
for (const w of ["planet", "guitar", "window", "rocket"]) e.contentItem(JSON.stringify({ word: w }));
e.contentPack(G.DRAW, "Draw");
for (const w of ["cat", "house", "tree"]) e.contentItem(JSON.stringify({ word: w }));
e.contentPack(G.SPECTRUM, "Spec");
for (const [l, r] of [["Cold", "Hot"], ["Cheap", "Expensive"]]) e.contentItem(JSON.stringify({ left: l, right: r }));
e.contentPack(G.KMK, "People");
for (const n of ["Ana", "Bo", "Cy", "Di", "Ed"]) e.contentItem(JSON.stringify({ name: n }));
e.contentPack(G.SECRETS, "Q");
for (const q of ["Talk to animals?", "Cried at a film?"]) e.contentItem(JSON.stringify({ q }));
e.contentPack(G.FILLBLANK, "FB");
for (const p of ["_____ ruined dinner.", "The future is _____."]) e.contentItem(JSON.stringify({ p }));
for (let i = 0; i < 30; i++) e.contentItem(JSON.stringify({ a: "ans-" + i }));
e.contentPack(G.SPYFALL, "Places");
for (const it of [
  '{"loc":"Beach","r":"Lifeguard","r":"Surfer","r":"Vendor"}',
  '{"loc":"Hospital","r":"Surgeon","r":"Nurse","r":"Patient"}',
]) e.contentItem(it);

// Four players cover every game's minimum (frankendraw/spyfall want 3+, 1v1 games want 2).
["ALICE", "BOB", "CARA", "DUKE"].forEach((n, i) => e.join(i + 1, n));

const lobbyOf = (out) => {
  const lob = lastToWs(out, 1, "lobby");
  assert.ok(lob, "a lobby is broadcast");
  return lob.msg;
};

// 1) Cycle through EVERY game. Each switch runs gsZero + challengesClear + the incoming
//    game's clear over the shared union. The order deliberately jumps between union members
//    of very different sizes (trivia -> frankendraw -> a 1v1 match array -> ...).
const ORDER = [
  G.TRIVIA, G.WYR, G.CHESS, G.FILLBLANK, G.SPYFALL, G.FRANKENDRAW, G.CONNECT4,
  G.BATTLESHIP, G.PONG, G.SCRAMBLE, G.SPECTRUM, G.KMK, G.SECRETS, G.GUESSCOLOR,
  G.REACT, G.WEREWOLF, G.DOTS, G.REVERSI, G.TICTACTOE, G.DRAW, G.TRIVIA,
];
for (const id of ORDER) {
  const out = e.selectGame(id);
  const lob = lobbyOf(out);
  assert.equal(lob.game, NAME[id], "lobby.game follows the switch to " + NAME[id]);
  assert.equal(lob.players.length, 4, "all four players survive the switch to " + NAME[id]);
  // Poke the fresh lobby: ready everyone and tick, so the game's serializer/checkStart runs
  // over freshly-zeroed union memory (a leftover byte from the previous game surfaces here).
  for (let p = 1; p <= 4; p++) e.input(p, { t: "ready", ready: true });
  e.tick(1000);
}

// 2) A live 1v1 match then a switch away. Connect-4 challenge+accept fills _m (union) and a
//    _c challenge slot (outside the union). Switching to trivia must clear both; a stale _m
//    match read as trivia's bytes would corrupt the room.
e.selectGame(G.CONNECT4);
e.input(1, { t: "challenge", to: 2 });
e.input(2, { t: "accept", from: 1 }); // _m[0] now a live match
let out = e.selectGame(G.TRIVIA);
assert.equal(lobbyOf(out).game, "trivia", "switching off a live match returns a clean trivia lobby");

// 3) A disconnect while trivia is active runs anyOnLeave -> every 1v1 match finder plus
//    fdOnLeave, each over union memory that now holds trivia. The _active guards must stop
//    them reading trivia's bytes as matches/sheets (else UBSan/ASan fires here).
out = e.disconnect(3);
assert.ok(lastToWs(out, 1, "lobby"), "a disconnect during trivia keeps the room alive");
e.join(3, "CARA"); // rejoin for the score check below

// 4) Packs live OUTSIDE the union, so they must survive all the cycling above. Re-select each
//    pack-driven game and assert its lobby still lists the pack it was given -- one game per
//    pack struct type: WyrPack (wyr, spectrum), WordPack (scramble, kmk, secrets),
//    FillBlankPack (fillblank), SpyPack (spyfall).
for (const [id, tag, name] of [
  [G.WYR, "wyr", "WYR"], [G.SPECTRUM, "spectrum", "Spec"],
  [G.SCRAMBLE, "scramble", "Words"], [G.KMK, "kmk", "People"],
  [G.SECRETS, "secrets", "Q"], [G.FILLBLANK, "fillblank", "FB"],
  [G.SPYFALL, "spyfall", "Places"],
]) {
  const m = lastToWs(e.selectGame(id), 1, tag).msg;
  assert.equal(m.phase, "lobby", tag + " returns to its lobby on select");
  assert.ok(Array.isArray(m.packs) && m.packs.length === 1, tag + " still has its one pack after the cycling");
  assert.equal(m.packs[0].name, name, tag + " pack survived the union switches intact");
}

// A lifted pack must still be playable, not just present: drive WYR through its countdown and
// confirm a real A/B prompt round opens (wyrCheckStart no-ops if _wyrPacks was lost).
e.selectGame(G.WYR);
for (let p = 1; p <= 4; p++) e.input(p, { t: "ready", ready: true });
let now = 0, vote = [];
for (let s = 0; s < 4; s++) { now += 1000; vote = e.tick(now); }
assert.equal(lastToWs(vote, 1, "wyr").msg.phase, "vote", "WYR still starts a round -> its pack is intact and usable");

// 5) Switching games resets the scoreboard (selectGame -> resetScoresAll). Score P1 in trivia,
//    confirm it went positive, then switch and confirm every score is back to zero.
e.selectGame(G.TRIVIA);
for (let p = 1; p <= 4; p++) e.input(p, { t: "ready", ready: true });
let q = [];
for (let ms = 1000; ms <= 8000; ms += 1000) q = q.concat(e.tick(ms));
assert.equal(lastToWs(q, 1, "trivia").msg.phase, "question", "trivia reaches a question");
e.input(1, { t: "answer", c: 0 }); // P1: correct (option A -> index 0)
e.input(2, { t: "answer", c: 1 });
e.input(3, { t: "answer", c: 1 });
let scored = e.input(4, { t: "answer", c: 1 }); // last answer triggers reveal + scoring
const p1 = lobbyOf(scored).players.find((p) => p.pid === 1);
assert.ok(p1 && p1.score > 0, "P1 scored a correct trivia answer");
const after = lobbyOf(e.selectGame(G.GUESSCOLOR));
assert.ok(after.players.every((p) => p.score === 0), "switching games resets every score to zero");

console.log("multigame: all checks passed");
