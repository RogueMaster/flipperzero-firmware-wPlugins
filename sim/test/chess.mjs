// Chess: 1v1 with clocks, castling, en passant, promotion, threefold/fifty-move claims
// and automatic draws, resignation, draw offers, forfeit and rematch -- the full FIDE
// end-of-game matrix layered on the rules core and match plumbing from Tasks 2-3.
// Drives the real engine headless, exactly like battleship.mjs, plus the
// HA_CHESS_TEST-only ha_chess_load / ha_chess_perft hooks (harness-lib.mjs's
// chessLoad/chessPerft) to reach positions the opening moves can't get to quickly
// (mate/stalemate/draw setups, and perft ground truth against the real move generator).
//
// Sim time only advances via e.tick(ms) (absolute engine millis), so every fresh game
// ticks a nonzero time before accepting the challenge -- otherwise millis() would still
// read 0 when chessStart() stamps lastStamp, which is harmless for correctness but
// would make every subsequent clock computation start from a zero baseline.
import assert from "node:assert/strict";
import { newEngine, lastToWs } from "./harness-lib.mjs";

const CHESS = 15; // HA_GAME_CHESS in ha_proto.h

// file+rank ("e2") -> board index, 0 = a1 .. 63 = h8 (matches chessBoardStr).
function sq(s) {
  return (s.charCodeAt(1) - 49) * 8 + (s.charCodeAt(0) - 97);
}

// Sparse piece placement ({e1:"K", d8:"r", ...}) -> the 64-char board64 string
// ha_chess_load/chessLoadCore expect (uppercase white, '.' empty).
function boardStr(pieces) {
  const b = new Array(64).fill(".");
  for (const [s, p] of Object.entries(pieces)) b[sq(s)] = p;
  return b.join("");
}

const STARTPOS = "RNBQKBNR" + "PPPPPPPP" + ".".repeat(32) + "pppppppp" + "rnbqkbnr";

const e = await newEngine();

// Fresh game: select chess, join ALICE (pid 1) and BOB (pid 2), challenge, tick a
// nonzero time, accept. The challenger always plays white (matchAccept's
// chessStart(&_cm[i], from, pid, from)), so ALICE is white in every fresh game below.
function startGame() {
  e.reset();
  e.selectGame(CHESS);
  e.join(1, "ALICE");
  e.join(2, "BOB");
  e.input(1, { t: "challenge", to: 2 });
  e.tick(1000);
  return e.input(2, { t: "accept", from: 1 });
}

function mv(pid, from, to, promo) {
  return promo === undefined
    ? e.input(pid, { t: "move", from, to })
    : e.input(pid, { t: "move", from, to, promo });
}

// ---- 1. Flow ----------------------------------------------------------------------
{
  const out = startGame();
  const a = lastToWs(out, 1, "chess");
  const b = lastToWs(out, 2, "chess");
  assert.equal(a.msg.phase, "playing", "challenge/accept starts the game");
  assert.equal(b.msg.phase, "playing");
  assert.equal(a.msg.white, true, "the challenger plays white");
  assert.equal(a.msg.moves.length, 20, "white has all 20 opening moves");
  assert.deepEqual(b.msg.moves, [], "it is not black's turn: no moves are sent to them");
  assert.equal(a.msg.board, STARTPOS, "board is the standard starting position");
  assert.equal(a.msg.run, 300000, "the side to move's clock starts at 5:00");
  assert.equal(a.msg.oms, 300000, "the opponent's clock also starts at 5:00");
}

// ---- 2. Illegal moves are silently rejected (no `chess` push at all) --------------
{
  // (a) opponent moves out of turn
  startGame();
  let out = mv(2, sq("e7"), sq("e5")); // black, but it's white's move
  assert.equal(lastToWs(out, 2, "chess"), undefined, "moving out of turn is a no-op");

  // (b) white plays a black piece
  out = mv(1, sq("e7"), sq("e5")); // that square holds a black pawn, not white's
  assert.equal(lastToWs(out, 1, "chess"), undefined, "moving the opponent's piece is a no-op");

  // (c) a king move into check
  startGame();
  e.chessLoad(boardStr({ e1: "K", d8: "r", e8: "k" }), 0, 0, -1, 0, 300000, 300000);
  out = mv(1, sq("e1"), sq("d1")); // Rd8 covers the d-file: Kd1 would be in check
  assert.equal(lastToWs(out, 1, "chess"), undefined, "a move into check is rejected");
  out = mv(1, sq("e1"), sq("f1")); // proves the reject wasn't a dead engine
  const a = lastToWs(out, 1, "chess");
  assert.ok(a, "a legal king move right after is accepted");
  assert.equal(a.msg.board[sq("f1")], "K", "the king actually moved to f1");
}

// ---- 3. Scholar's mate -------------------------------------------------------------
{
  startGame();
  mv(1, sq("e2"), sq("e4"));
  mv(2, sq("e7"), sq("e5"));
  mv(1, sq("f1"), sq("c4"));
  mv(2, sq("b8"), sq("c6"));
  mv(1, sq("d1"), sq("h5"));
  mv(2, sq("g8"), sq("f6")); // the losing blunder: white mates before Nxh5 ever happens
  const out = mv(1, sq("h5"), sq("f7"));
  const a = lastToWs(out, 1, "chess");
  const b = lastToWs(out, 2, "chess");
  assert.equal(a.msg.phase, "over");
  assert.equal(a.msg.result, "win", "white delivered mate");
  assert.equal(a.msg.reason, "mate");
  assert.equal(b.msg.result, "lose");
  assert.equal(b.msg.reason, "mate");
  const score = out.find((o) => o.to === "uart" && o.kind === "score");
  assert.ok(score, "the win scores on the Flipper leaderboard");
  assert.equal(score.pid, 1);
  assert.equal(score.delta, 300);
  assert.equal(score.reason, "chesswin");
  const round = out.find((o) => o.to === "uart" && o.kind === "round");
  assert.equal(round.json.win, 1);
  assert.equal(round.json.lose, 2);
}

// ---- 4. Castling --------------------------------------------------------------------
{
  // (a) O-O becomes available once the king's path clears, and actually moves both pieces
  startGame();
  mv(1, sq("e2"), sq("e4"));
  mv(2, sq("e7"), sq("e5"));
  mv(1, sq("g1"), sq("f3"));
  mv(2, sq("b8"), sq("c6"));
  mv(1, sq("f1"), sq("c4"));
  let out = mv(2, sq("g8"), sq("f6"));
  const castleMove = sq("e1") * 64 + sq("g1");
  let w = lastToWs(out, 1, "chess");
  assert.ok(w.msg.moves.includes(castleMove), "O-O is legal once f1/g1 are clear");
  out = mv(1, sq("e1"), sq("g1"));
  w = lastToWs(out, 1, "chess");
  assert.equal(w.msg.board[sq("g1")], "K", "king castled to g1");
  assert.equal(w.msg.board[sq("f1")], "R", "rook hopped over to f1");
}
{
  // (b) rights loss persists even once the blocking pieces later clear
  startGame();
  mv(1, sq("h2"), sq("h4"));
  mv(2, sq("h7"), sq("h5"));
  mv(1, sq("h1"), sq("h3"));
  mv(2, sq("h8"), sq("h6")); // rook off h1: the O-O right is gone for good
  mv(1, sq("h3"), sq("h1"));
  mv(2, sq("h6"), sq("h8")); // ...even though the rook comes straight back home
  mv(1, sq("g1"), sq("f3")); // clears g1
  mv(2, sq("a7"), sq("a6"));
  mv(1, sq("e2"), sq("e4")); // opens the f1-c4 diagonal for the bishop
  mv(2, sq("a6"), sq("a5"));
  mv(1, sq("f1"), sq("c4")); // clears f1
  const out = mv(2, sq("a5"), sq("a4"));
  const w = lastToWs(out, 1, "chess");
  const castleMove = sq("e1") * 64 + sq("g1");
  assert.ok(!w.msg.moves.includes(castleMove), "O-O stays illegal: the right was lost, not just blocked");
}
{
  // (c) castling through an attacked square is illegal even with the right and a clear path
  startGame();
  const out = e.chessLoad(
    boardStr({ e1: "K", h1: "R", f8: "r", e8: "k" }),
    0, 1 /* white O-O only */, -1, 0, 300000, 300000,
  );
  const w = lastToWs(out, 1, "chess");
  const castleMove = sq("e1") * 64 + sq("g1");
  assert.ok(!w.msg.moves.includes(castleMove), "Rf8 attacks f1, the square the king crosses");
}

// ---- 5. En passant ------------------------------------------------------------------
{
  startGame();
  mv(1, sq("e2"), sq("e4"));
  mv(2, sq("a7"), sq("a6"));
  mv(1, sq("e4"), sq("e5"));
  let out = mv(2, sq("d7"), sq("d5"));
  const epMove = sq("e5") * 64 + sq("d6");
  let w = lastToWs(out, 1, "chess");
  assert.ok(w.msg.moves.includes(epMove), "exd6 e.p. is offered right after the double push");
  out = mv(1, sq("e5"), sq("d6"));
  w = lastToWs(out, 1, "chess");
  assert.equal(w.msg.board[sq("d5")], ".", "the captured black pawn is gone from d5");
  assert.equal(w.msg.board[sq("d6")], "P", "the white pawn landed on d6");
}

// ---- 6. EP repetition subtlety -------------------------------------------------------
// After e4/a6/e5/d5 (position X), exd6 e.p. is legal, so X's hash folds in the ep file
// (FIDE 9.2 compares possible moves, not the bare square). Shuffling the b-knights out
// and back reaches the exact same piece placement with the ep chance gone -- call that
// X'. X' != X, so shuffling it back TWO more times only reaches X's third occurrence of
// X' one full cycle later than a naive (placement-only) hash would claim.
{
  startGame();
  mv(1, sq("e2"), sq("e4"));
  mv(2, sq("a7"), sq("a6"));
  mv(1, sq("e4"), sq("e5"));
  mv(2, sq("d7"), sq("d5")); // position X
  const shuffle = [
    [1, "b1", "c3"], [2, "b8", "c6"], [1, "c3", "b1"], [2, "c6", "b8"],
  ];
  let out;
  for (let cycle = 1; cycle <= 3; cycle++) {
    for (const [pid, from, to] of shuffle) out = mv(pid, sq(from), sq(to));
    const w = lastToWs(out, 1, "chess");
    if (cycle === 2)
      assert.equal(w.msg.claim3, false, "X' (placement of X, ep gone) has occurred only twice");
    if (cycle === 3)
      assert.equal(w.msg.claim3, true, "X' has now occurred a third time");
  }
}

// ---- 7. Promotion --------------------------------------------------------------------
{
  startGame();
  const promoBoard = boardStr({ a7: "P", e1: "K", e8: "k" });
  e.chessLoad(promoBoard, 0, 0, -1, 0, 300000, 300000);
  let out = mv(1, sq("a7"), sq("a8")); // no promo named
  assert.equal(lastToWs(out, 1, "chess"), undefined, "a promotion without `promo` is rejected");
  out = mv(1, sq("a7"), sq("a8"), 5);
  let w = lastToWs(out, 1, "chess");
  assert.equal(w.msg.board[sq("a8")], "Q", "promo:5 promotes to a queen");

  e.chessLoad(promoBoard, 0, 0, -1, 0, 300000, 300000); // reload the same position
  out = mv(1, sq("a7"), sq("a8"), 2);
  w = lastToWs(out, 1, "chess");
  assert.equal(w.msg.board[sq("a8")], "N", "promo:2 promotes to a knight");
}

// ---- 8. Stalemate ---------------------------------------------------------------------
{
  startGame();
  e.chessLoad(boardStr({ g6: "K", f1: "Q", h8: "k" }), 0, 0, -1, 0, 300000, 300000);
  const out = mv(1, sq("f1"), sq("f7"));
  const a = lastToWs(out, 1, "chess");
  const b = lastToWs(out, 2, "chess");
  assert.equal(a.msg.phase, "over");
  assert.equal(a.msg.result, "draw");
  assert.equal(a.msg.reason, "stalemate");
  assert.equal(b.msg.result, "draw");
  const round = out.find((o) => o.to === "uart" && o.kind === "round");
  assert.deepEqual(round.json.draw, [1, 2]);
  assert.ok(!out.some((o) => o.to === "uart" && o.kind === "score"), "no score on a draw");
}

// ---- 9. Dead position -------------------------------------------------------------------
{
  startGame();
  e.chessLoad(boardStr({ b1: "K", c1: "B", d2: "p", a8: "k" }), 0, 0, -1, 0, 300000, 300000);
  const out = mv(1, sq("c1"), sq("d2")); // Bxd2 leaves K+B vs K
  const a = lastToWs(out, 1, "chess");
  assert.equal(a.msg.phase, "over");
  assert.equal(a.msg.result, "draw");
  assert.equal(a.msg.reason, "material");
}

// ---- 10. Threefold claim -----------------------------------------------------------------
{
  startGame();
  const shuffle = [[1, "g1", "f3"], [2, "g8", "f6"], [1, "f3", "g1"], [2, "f6", "g8"]];
  let out;
  for (let cycle = 1; cycle <= 2; cycle++) {
    for (const [pid, from, to] of shuffle) out = mv(pid, sq(from), sq(to));
    const w = lastToWs(out, 1, "chess");
    if (cycle === 1) assert.equal(w.msg.claim3, false, "startpos has occurred twice so far");
    if (cycle === 2) assert.equal(w.msg.claim3, true, "startpos has now occurred a third time");
  }
  out = e.input(1, { t: "claim" });
  const a = lastToWs(out, 1, "chess");
  assert.equal(a.msg.phase, "over");
  assert.equal(a.msg.reason, "rep3");
  assert.equal(a.msg.result, "draw");

  // a claim with nothing to claim is silently ignored
  startGame();
  out = e.input(1, { t: "claim" });
  assert.equal(lastToWs(out, 1, "chess"), undefined, "no repetition or 50-move count yet: no-op");
}

// ---- 11. Fivefold auto -------------------------------------------------------------------
{
  startGame();
  const shuffle = [[1, "g1", "f3"], [2, "g8", "f6"], [1, "f3", "g1"], [2, "f6", "g8"]];
  let out;
  for (let cycle = 1; cycle <= 4; cycle++)
    for (const [pid, from, to] of shuffle) out = mv(pid, sq(from), sq(to));
  const a = lastToWs(out, 1, "chess");
  const b = lastToWs(out, 2, "chess");
  assert.equal(a.msg.phase, "over", "fivefold repetition ends the game automatically");
  assert.equal(a.msg.reason, "rep5");
  assert.equal(a.msg.result, "draw");
  assert.equal(b.msg.result, "draw");
}

// ---- 12. 50/75-move rules, and mate outranking the 150th ply -----------------------------
{
  // (a) 50-move claim
  startGame();
  e.chessLoad(boardStr({ e1: "K", a1: "R", e8: "k" }), 0, 0, -1, 99, 300000, 300000);
  let out = mv(1, sq("a1"), sq("b1")); // reversible: halfmove -> 100
  let w = lastToWs(out, 2, "chess");
  assert.equal(w.msg.claim50, true, "halfmove reached 100: black may claim the 50-move rule");
  out = e.input(2, { t: "claim" });
  w = lastToWs(out, 2, "chess");
  assert.equal(w.msg.phase, "over");
  assert.equal(w.msg.reason, "move50");
  assert.equal(w.msg.result, "draw");
}
{
  // (b) 75-move rule fires automatically, no claim needed
  startGame();
  e.chessLoad(boardStr({ e1: "K", a1: "R", e8: "k" }), 0, 0, -1, 149, 300000, 300000);
  const out = mv(1, sq("a1"), sq("b1")); // reversible: halfmove -> 150
  const w = lastToWs(out, 1, "chess");
  assert.equal(w.msg.phase, "over");
  assert.equal(w.msg.reason, "move75");
  assert.equal(w.msg.result, "draw");
}
{
  // (c) mate wins on the very ply that would otherwise auto-draw at 150
  startGame();
  e.chessLoad(
    boardStr({ a1: "R", g1: "K", f7: "p", g7: "p", h7: "p", g8: "k" }),
    0, 0, -1, 149, 300000, 300000,
  );
  const out = mv(1, sq("a1"), sq("a8")); // back-rank mate, also the 150th reversible ply
  const a = lastToWs(out, 1, "chess");
  const b = lastToWs(out, 2, "chess");
  assert.equal(a.msg.phase, "over");
  assert.equal(a.msg.reason, "mate", "checkmate outranks the automatic 75-move draw");
  assert.equal(a.msg.result, "win");
  assert.equal(b.msg.result, "lose");
  const score = out.find((o) => o.to === "uart" && o.kind === "score");
  assert.ok(score, "the winner still scores, not a move75 draw");
  assert.equal(score.pid, 1);
}

// ---- 13. Flag fall ---------------------------------------------------------------------
{
  // (a) flag fall with mating material left: the opponent wins
  startGame(); // lastStamp = 1000 (startGame ticks 1000 before accepting)
  const out = e.tick(302000); // 301s elapsed: white's 5:00 clock ran out
  const a = lastToWs(out, 1, "chess");
  const b = lastToWs(out, 2, "chess");
  assert.equal(a.msg.phase, "over");
  assert.equal(a.msg.reason, "flag");
  assert.equal(a.msg.result, "lose", "white flagged");
  assert.equal(b.msg.result, "win", "black had mating material left");
}
{
  // (b) flag fall with no mating material left: a draw
  startGame();
  e.chessLoad(boardStr({ e1: "K", d1: "Q", e8: "k" }), 0, 0, -1, 0, 5000, 300000);
  const out = e.tick(1000 + 5001); // past white's 5s clock (load's lastStamp is 1000)
  const a = lastToWs(out, 1, "chess");
  assert.equal(a.msg.phase, "over");
  assert.equal(a.msg.reason, "flagdraw", "a bare king can never be helpmated");
  assert.equal(a.msg.result, "draw");
}
{
  // (c) a move that arrives after a tick already fired the flag changes nothing
  startGame();
  let out = e.tick(1000 + 300001); // chessTick fires the flag before any move is seen
  const a = lastToWs(out, 1, "chess");
  assert.equal(a.msg.phase, "over");
  assert.equal(a.msg.reason, "flag");
  out = mv(1, sq("e2"), sq("e4")); // "late" move: the game is already over
  assert.equal(lastToWs(out, 1, "chess"), undefined, "no further push; the flag result stands");
}

// ---- 14. Draw offer ---------------------------------------------------------------------
{
  // (a) offer -> toast -> agree
  startGame();
  mv(1, sq("e2"), sq("e4")); // makes it black's turn, so the lapse case below is exercisable
  let out = e.input(1, { t: "draw" }); // offering is not turn-gated
  const toast = lastToWs(out, 2, "toast");
  assert.ok(toast, "the opponent gets a toast for the offer");
  let b = lastToWs(out, 2, "chess");
  assert.equal(b.msg.offer, 1, "the offer names the offering pid");
  out = e.input(2, { t: "draw" }); // accept
  const a = lastToWs(out, 1, "chess");
  assert.equal(a.msg.phase, "over");
  assert.equal(a.msg.reason, "agree");
  assert.equal(a.msg.result, "draw");
}
{
  // (b) the offer lapses once a move is played instead of answering it
  startGame();
  mv(1, sq("e2"), sq("e4"));
  e.input(1, { t: "draw" });
  const out = mv(2, sq("e7"), sq("e5")); // black moves instead of answering
  const a = lastToWs(out, 1, "chess");
  const b = lastToWs(out, 2, "chess");
  assert.equal(a.msg.offer, 0, "the lapsed offer clears for both players");
  assert.equal(b.msg.offer, 0);
}

// ---- 15. Resign ---------------------------------------------------------------------------
{
  startGame();
  const out = e.input(1, { t: "resign" });
  const b = lastToWs(out, 2, "chess");
  assert.equal(b.msg.phase, "over");
  assert.equal(b.msg.result, "win");
  assert.equal(b.msg.reason, "resign");
  const score = out.find((o) => o.to === "uart" && o.kind === "score");
  assert.equal(score.pid, 2);
  assert.equal(score.delta, 300);
  assert.equal(score.reason, "chesswin");
}

// ---- 16. Disconnect forfeit ----------------------------------------------------------------
{
  startGame();
  const out = e.disconnect(1);
  const b = lastToWs(out, 2, "chess");
  assert.equal(b.msg.phase, "over");
  assert.equal(b.msg.reason, "left");
  assert.equal(b.msg.result, "win");
}

// ---- 17. Rematch --------------------------------------------------------------------------
{
  startGame();
  e.input(1, { t: "resign" }); // ALICE (white) resigns; BOB wins
  let out = e.input(1, { t: "rematch" });
  out = out.concat(e.input(2, { t: "rematch" })); // idempotent: already restarted, no-op
  const b = lastToWs(out, 2, "chess");
  assert.equal(b.msg.phase, "playing");
  assert.equal(b.msg.white, true, "colors swap: the previous black (BOB) is now white");
  assert.equal(b.msg.moves.length, 20);
  assert.equal(b.msg.board, STARTPOS);
  assert.equal(b.msg.run, 300000);
  assert.equal(b.msg.oms, 300000);
}

// ---- 18. Reaction scoping -----------------------------------------------------------------
{
  startGame();
  e.join(3, "CHARLIE"); // a third player in the lobby, not part of this match
  const out = e.input(1, { t: "react", emoji: "🔥" });
  assert.ok(lastToWs(out, 1, "emoji"), "the sender sees their own reaction echoed back");
  assert.ok(lastToWs(out, 2, "emoji"), "the opponent receives it");
  assert.equal(lastToWs(out, 3, "emoji"), undefined, "a bystander outside the match does not");
}

// ---- 19. Perft (via ha_chess_perft, against the real move generator) ----------------------
{
  assert.equal(e.chessPerft(STARTPOS, 0, 15, -1, 1), 20);
  assert.equal(e.chessPerft(STARTPOS, 0, 15, -1, 2), 400);
  assert.equal(e.chessPerft(STARTPOS, 0, 15, -1, 3), 8902);
  assert.equal(e.chessPerft(STARTPOS, 0, 15, -1, 4), 197281);

  const kiwipete =
    "R...K..R" + "PPPBBPPP" + "..N..Q.p" + ".p..P..." +
    "...PN..." + "bn..pnp." + "p.ppqpb." + "r...k..r";
  assert.equal(e.chessPerft(kiwipete, 0, 15, -1, 1), 48);
  assert.equal(e.chessPerft(kiwipete, 0, 15, -1, 2), 2039);
  assert.equal(e.chessPerft(kiwipete, 0, 15, -1, 3), 97862);
}

console.log("chess: all checks passed");
