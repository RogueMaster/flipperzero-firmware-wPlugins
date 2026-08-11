// Fill the Blank: whole-group party game. A rotating Czar shows a prompt card with a
// blank; everyone else plays one answer card from their hand face down. The pile is
// shuffled — with one extra card drawn by the deck itself mixed in — and shown
// anonymously; the Czar picks the winner. Exercises selectGame + ready/vote/play/pick/
// again, the quorum gate (a Czar plus two answers), deal -> play -> judge -> reveal,
// hand refill, Czar rotation, the deck's random card (anonymous before the pick, scores
// nobody when picked), the scoring split (winning author +1 and the Czar +1 for picking a
// real card), a played card staying in the hand marked, and the hidden-information rule
// (no card carries its author's pid or nick before the Czar has picked). Drives the real
// engine headless.
import assert from "node:assert/strict";
import { newEngine, lastToWs } from "./harness-lib.mjs";

const FB = 17;
const HAND = 6;

const PROMPTS = ["Nothing beats _____ on a Monday.", "_____ ruined the family dinner.", "The future is _____."];

/** A fresh 3-player game sitting in round 1's play stage. */
async function startGame() {
  const e = await newEngine();
  e.reset();
  e.join(1, "ALICE");
  e.join(2, "BOB");
  e.selectGame(FB);
  // A pack carries both decks: `P` items are prompt cards (with the ____ blank), `A`
  // items are answer cards. Enough answers for three full hands plus the draw pile.
  e.contentClear();
  e.contentPack(FB, "Test");
  for (const p of PROMPTS) e.contentItem(JSON.stringify({ p }));
  for (let i = 0; i < 30; i++) e.contentItem(JSON.stringify({ a: "answer-" + i }));

  // All three ready -> countdown -> round 1. (Two is now enough to play; that path has
  // its own test below, since it is the one the padding exists for.)
  e.join(3, "CARA");
  e.input(1, { t: "ready", ready: true });
  e.input(2, { t: "ready", ready: true });
  let out = e.input(3, { t: "ready", ready: true });
  for (let ms = 1000; ms <= 6000; ms += 1000) out = out.concat(e.tick(ms));
  return { e, out };
}

// Two players: one Czar, one answer. A judge choosing between a single card is no
// choice at all, so the pile is padded from the deck up to FB_MIN_PILE -- which is what
// makes a two-player game work rather than sit in the lobby forever.
async function twoPlayersPlay() {
  const e = await newEngine();
  e.reset();
  e.join(1, "ALICE");
  e.join(2, "BOB");
  e.selectGame(FB);
  e.contentClear();
  e.contentPack(FB, "Test");
  for (const p of PROMPTS) e.contentItem(JSON.stringify({ p }));
  for (let i = 0; i < 30; i++) e.contentItem(JSON.stringify({ a: "answer-" + i }));

  e.input(1, { t: "ready", ready: true });
  let out = e.input(2, { t: "ready", ready: true });
  for (let ms = 1000; ms <= 6000; ms += 1000) out = out.concat(e.tick(ms));
  assert.equal(lastToWs(out, 1, "fillblank").msg.phase, "play", "two players start a round");

  // The non-Czar answers; the pile must still offer the Czar three cards to pick from.
  const czar = [1, 2].find((p) => lastToWs(out, p, "fillblank").msg.iam);
  const other = czar === 1 ? 2 : 1;
  const hand = lastToWs(out, other, "fillblank").msg.hand;
  assert.ok(hand && hand.length, "the answering player got a hand");
  out = e.input(other, { t: "play", card: 0 });
  for (let ms = 7000; ms <= 12000; ms += 1000) out = out.concat(e.tick(ms));

  const m = lastToWs(out, czar, "fillblank").msg;
  assert.equal(m.stage, "judge", "the Czar is judging");
  assert.equal(m.subs.length, 3, "one player card padded from the deck up to three");
  console.log("fillblank: two players play with a padded pile");
}

const NICK = { 1: "ALICE", 2: "BOB", 3: "CARA" };
const czarOf = (out) => [1, 2, 3].find((p) => lastToWs(out, p, "fillblank").msg.iam);

// ---------------------------------------------------------------- round 1: full flow
{
  const { e, out: start } = await startGame();
  let out = start;

  for (const pid of [1, 2, 3]) {
    const m = lastToWs(out, pid, "fillblank").msg;
    assert.equal(m.phase, "play", "in play after the countdown");
    assert.equal(m.stage, "play", "everyone plays a card first");
    assert.ok(m.prompt && m.prompt.includes("_____"), "a prompt card with a blank");
    assert.equal(m.subs, undefined, "no pile is shown while cards are still being played");
  }
  const czar1 = czarOf(out);
  assert.ok(czar1 >= 1, "one player is the Czar");
  const players = [1, 2, 3].filter((p) => p !== czar1);

  // The Czar holds no playable hand this round; everyone else gets a full one.
  assert.equal(lastToWs(out, czar1, "fillblank").msg.hand, undefined, "the Czar is not dealt a playable hand");
  for (const p of players) {
    const h = lastToWs(out, p, "fillblank").msg.hand;
    assert.equal(h.length, HAND, "a full hand of " + HAND + " answer cards");
    assert.ok(h.every((c) => c), "no empty slot in a fresh hand");
  }

  // Everyone but the Czar plays their slot-0 card. Remember which text each played, so
  // the anonymity assertions below can look for a leak of the mapping.
  const playedBy = {}; // card text -> pid
  const handBefore = {};
  for (const p of players) {
    handBefore[p] = lastToWs(out, p, "fillblank").msg.hand;
    playedBy[handBefore[p][0]] = p;
    out = e.input(p, { t: "play", card: 0 });
  }

  // A played card STAYS in the hand, marked as the one you chose, and further plays are
  // refused outright rather than silently swapping the card.
  for (const p of players) {
    const m = lastToWs(out, p, "fillblank").msg;
    assert.deepEqual(m.hand, handBefore[p], "the played card stays in the hand, in place");
    assert.equal(m.mine, 0, "the engine reports which slot you committed to");
  }
  assert.equal(e.input(players[0], { t: "play", card: 1 }).length, 0,
    "a second play is refused (one card each, no take-backs)");

  // Submissions are closed, the deck's own card is added, and the pile is shuffled.
  for (const pid of [1, 2, 3]) {
    const m = lastToWs(out, pid, "fillblank").msg;
    assert.equal(m.stage, "judge", "judging once everyone has played");
    assert.equal(m.subs.length, 3, "two player cards plus the deck's own card");
    assert.equal(m.played, 2, "the tally counts players, not the deck's card");
    assert.equal(m.total, 2, "two players were expected to play");
    assert.ok(m.subs.every((c) => typeof c === "string"), "a submission is bare card text");
    // The hidden-information rule: nothing in the pile identifies who played what, and
    // nothing marks which card is the deck's.
    const pile = JSON.stringify(m.subs);
    for (const nick of Object.values(NICK))
      assert.ok(!pile.includes(nick), "no nick rides along with a submission");
    assert.equal(m.authors, undefined, "authorship is withheld until the Czar picks");
    assert.equal(m.deckcard, undefined, "the deck's card is not identified before the pick");
    assert.equal(m.pick, undefined, "no winner is named before the Czar picks");
    assert.equal(m.winner, undefined, "no winner is named before the Czar picks");
    assert.equal(m.deckwon, undefined, "no deck result before the Czar picks");
  }

  // The Czar's own view is the same anonymous pile — that is the view the rule protects.
  const pile = lastToWs(out, czar1, "fillblank").msg.subs;
  const deckExtra = pile.filter((c) => !(c in playedBy));
  assert.equal(deckExtra.length, 1, "exactly one card in the pile came from the deck");

  // A non-Czar can't judge: the intent is dropped without touching the round.
  assert.equal(e.input(players[0], { t: "pick", i: 0 }).length, 0, "only the Czar may pick");

  // The Czar picks a real player's card (find its index in the shuffled pile).
  const realIdx = pile.findIndex((c) => c in playedBy);
  const winnerPid = playedBy[pile[realIdx]];
  out = e.input(czar1, { t: "pick", i: realIdx });

  const rev = lastToWs(out, czar1, "fillblank").msg;
  assert.equal(rev.stage, "reveal", "reveal after the pick");
  assert.equal(rev.pick, realIdx, "the reveal marks the winning card");
  assert.equal(rev.winner, NICK[winnerPid], "the winner is named after the pick");
  assert.equal(rev.deckwon, false, "a player's card won, not the deck's");

  // The reveal names EVERY card: authors runs parallel to subs, and the deck's card is
  // identified by index so an author who left ("") can never be mistaken for it.
  assert.equal(rev.authors.length, rev.subs.length, "one author per card");
  assert.equal(typeof rev.deckcard, "number");
  assert.ok(rev.deckcard >= 0 && rev.deckcard < rev.subs.length, "the deck's card is located");
  assert.equal(rev.subs[rev.deckcard], deckExtra[0], "deckcard points at the card nobody played");
  for (let i = 0; i < rev.subs.length; i++) {
    if (i === rev.deckcard) continue;
    assert.equal(rev.authors[i], NICK[playedBy[rev.subs[i]]], "each card is attributed to its player");
  }

  // Scoring: the winning author +1, and the Czar +1 for picking a real player's card.
  const scored = out.filter((o) => o.to === "uart" && o.kind === "score" &&
    String(o.reason).startsWith("fillblank"));
  assert.equal(scored.length, 2, "two score events: the author and the Czar");
  const byPid = Object.fromEntries(scored.map((o) => [o.pid, o]));
  assert.equal(byPid[winnerPid].delta, 1, "the winning card's author scores 1");
  assert.equal(byPid[czar1].delta, 1, "the Czar scores 1 for picking a player's card");
  assert.equal(rev.czarpts, 1, "the reveal reports the Czar's point");
  assert.ok(lastToWs(out, winnerPid, "fillblank").msg.mywin, "the winner is told they won");
  assert.equal(lastToWs(out, czar1, "fillblank").msg.mygain, 1, "the Czar earned a point");
  const board = Object.fromEntries(rev.scores.map((p) => [p.pid, p.score]));
  assert.equal(board[winnerPid], 1);
  assert.equal(board[czar1], 1);
  assert.equal(board[players.find((p) => p !== winnerPid)], 0, "the losing player scores nothing");

  // Reveal ends -> round 2: the Czar rotates, the played card is discarded and redrawn.
  out = e.tick(20000);
  for (const pid of [1, 2, 3]) {
    const m = lastToWs(out, pid, "fillblank").msg;
    assert.equal(m.round, 2, "second round");
    assert.equal(m.stage, "play", "back to playing cards");
    assert.equal(m.mine === undefined || m.mine === -1, true, "nothing played yet in round 2");
  }
  const czar2 = czarOf(out);
  assert.notEqual(czar2, czar1, "the Czar rotates each round");
  for (const p of [1, 2, 3].filter((x) => x !== czar2)) {
    const h = lastToWs(out, p, "fillblank").msg.hand;
    assert.equal(h.filter((c) => c).length, HAND, "hands are topped back up between rounds");
    if (p in handBefore) {
      assert.notEqual(h[0], handBefore[p][0], "the card you played is gone, replaced by a new draw");
    }
  }
}

// ------------------------------------------------- the deck's card wins: nobody scores
{
  const { e, out: start } = await startGame();
  let out = start;
  const czar = czarOf(out);
  const players = [1, 2, 3].filter((p) => p !== czar);
  const played = new Set();
  for (const p of players) {
    played.add(lastToWs(out, p, "fillblank").msg.hand[0]);
    out = e.input(p, { t: "play", card: 0 });
  }
  const pile = lastToWs(out, czar, "fillblank").msg.subs;
  const deckIdx = pile.findIndex((c) => !played.has(c));
  assert.ok(deckIdx >= 0, "the deck's card is in the pile");

  out = e.input(czar, { t: "pick", i: deckIdx });
  const rev = lastToWs(out, czar, "fillblank").msg;
  assert.equal(rev.stage, "reveal");
  assert.equal(rev.deckwon, true, "the deck won the round");
  assert.equal(rev.winner, "", "no player is named as the winner");
  assert.equal(rev.pick, deckIdx, "the winning card is still marked");
  assert.equal(rev.deckcard, deckIdx, "and it is identified as the deck's");
  assert.equal(rev.czarpts, 0, "the Czar earns nothing for being fooled by the deck");
  assert.equal(
    out.filter((o) => o.to === "uart" && o.kind === "score" && String(o.reason).startsWith("fillblank")).length,
    0, "picking the deck's card scores nobody at all");
  for (const pid of [1, 2, 3]) {
    const m = lastToWs(out, pid, "fillblank").msg;
    assert.equal(m.mywin, false, "nobody won");
    assert.equal(m.mygain, 0, "nobody gained");
    assert.ok(m.scores.every((p) => p.score === 0), "the board is untouched");
  }
  // Even so, the reveal still attributes every player card.
  for (let i = 0; i < rev.subs.length; i++) {
    if (i === deckIdx) assert.equal(rev.authors[i], "", "the deck has no nick");
    else assert.ok(Object.values(NICK).includes(rev.authors[i]), "player cards stay attributed");
  }
}

await twoPlayersPlay();

console.log("fillblank: all checks passed");
