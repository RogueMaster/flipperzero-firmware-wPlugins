/* Unified 1v1 duel renderer for Connect 4, Tic-Tac-Toe, and Dots & Boxes.
   All three share the lobby/challenge flow and the {t:"duel"} message; the
   `kind` field ("c4"/"ttt"/"dots") drives the board. The server is the referee;
   we only send move{n} / rematch / leaveGame intents. */
(function () {
  var prev = null;        // previous board signature, to animate/sound new moves
  var prevPhase = "";
  var prevTurnMine = false;

  /* ---- Connect 4 / Tic-Tac-Toe: a simple cell grid ---- */
  function renderGrid(m) {
    var cols = m.cols || (m.kind === "ttt" ? 3 : 7);
    var rows = m.rows || (m.kind === "ttt" ? 3 : 6);
    var n = cols * rows;
    var myTurn = m.turn === m.you;
    var board = $("duel-board");
    board.className = "board" + (m.kind === "ttt" ? " ttt" : "") + (myTurn ? " mine" : "");
    board.style.gridTemplateColumns = "repeat(" + cols + ",1fr)";

    var b = m.board || [];
    var pb = (prev && prev.length === n) ? prev : null;
    var rebuild = board.childElementCount !== n;
    if (rebuild) board.innerHTML = "";
    for (var i = 0; i < n; i++) {
      var cell = rebuild ? document.createElement("div") : board.children[i];
      var v = b[i] || 0;
      var isNew = pb && pb[i] === "0" && v !== 0;
      var own = v === 0 ? "" : (v === m.me ? " you" : " opp");
      cell.className = "cell" + own + (isNew ? " drop" : "");
      if (rebuild) board.appendChild(cell);
    }

    // Tap: connect4 drops in a column, tic-tac-toe claims the exact cell.
    board.onclick = function (e) {
      if (!myTurn) return;
      var idx = Array.prototype.indexOf.call(board.children, e.target);
      if (idx < 0) return;
      if (m.kind === "ttt") {
        if ((b[idx] || 0) !== 0) return;   // occupied
        move(idx);
      } else {
        move(idx % cols);                  // gravity: column only
      }
    };
    hide("duel-score");
  }

  /* ---- Dots & Boxes: a (2h+1) x (2w+1) grid of dots / edges / boxes ---- */
  function renderDots(m) {
    var w = m.w, h = m.h;
    var hedges = m.hedges || [], vedges = m.vedges || [], boxes = m.boxes || [];
    var hoff = (h + 1) * w;               // vertical-edge index offset in move.n
    var myTurn = m.turn === m.you;
    var board = $("duel-board");
    board.className = "dotsgrid" + (myTurn ? " mine" : "");
    // dot rows/cols are fixed and thin; edge + box tracks flex so the whole board
    // fills its (square) container with big, tap-friendly cells.
    board.style.gridTemplateColumns = "12px" + " 1fr 12px".repeat(w);
    board.style.gridTemplateRows = "12px" + " 1fr 12px".repeat(h);
    board.innerHTML = "";

    for (var gr = 0; gr <= 2 * h; gr++) {
      for (var gc = 0; gc <= 2 * w; gc++) {
        var el = document.createElement("div");
        var er = gr & 1, ec = gc & 1;
        if (!er && !ec) {
          el.className = "dot-node";
        } else if (!er && ec) {              // horizontal edge
          var hr = gr / 2, hcc = (gc - 1) / 2, hi = hr * w + hcc;
          var drawn = hedges[hi] === 1;
          el.className = "edge h" + (drawn ? " on" : "");
          if (!drawn) bindEdge(el, hi, myTurn);
        } else if (er && !ec) {              // vertical edge
          var vr = (gr - 1) / 2, vcc = gc / 2, vi = vr * (w + 1) + vcc;
          var vdrawn = vedges[vi] === 1;
          el.className = "edge v" + (vdrawn ? " on" : "");
          if (!vdrawn) bindEdge(el, hoff + vi, myTurn);
        } else {                             // box cell
          var br = (gr - 1) / 2, bcc = (gc - 1) / 2, owner = boxes[br * w + bcc] || 0;
          el.className = "box" + (owner === 0 ? "" : (owner === m.me ? " you" : " opp"));
        }
        board.appendChild(el);
      }
    }

    var sc = $("duel-score");
    sc.innerHTML = '<span class="you">You ' + (m.sme || 0) + "</span>" +
                   '<span class="opp">' + esc(m.opp || "Opp") + " " + (m.sopp || 0) + "</span>";
    show("duel-score");
  }

  /* ---- Reversi / Othello: 8x8, discs, server sends legal moves in `valid` ---- */
  function renderReversi(m) {
    var cols = 8, rows = 8, n = 64;
    var myTurn = m.turn === m.you;
    var valid = m.valid || [];
    var vset = {};
    valid.forEach(function (i) { vset[i] = 1; });
    var board = $("duel-board");
    board.className = "board rev" + (myTurn ? " mine" : "");
    board.style.gridTemplateColumns = "repeat(8,1fr)";

    var b = m.board || [];
    var pb = (prev && prev.length === n) ? prev : null;
    var rebuild = board.childElementCount !== n;
    if (rebuild) board.innerHTML = "";
    for (var i = 0; i < n; i++) {
      var cell = rebuild ? document.createElement("div") : board.children[i];
      var v = b[i] || 0;
      var isNew = pb && pb[i] === "0" && v !== 0;
      var own = v === 0 ? "" : (v === m.me ? " you" : " opp");
      var canPlay = myTurn && vset[i];
      cell.className = "cell" + own + (isNew ? " drop" : "") + (canPlay ? " playable" : "");
      if (rebuild) board.appendChild(cell);
    }

    board.onclick = function (e) {
      if (!myTurn) return;
      var idx = Array.prototype.indexOf.call(board.children, e.target);
      if (idx < 0 || !vset[idx]) return;
      move(idx);
    };

    var sc = $("duel-score");
    sc.innerHTML = '<span class="you">You ' + (m.sme || 0) + "</span>" +
                   '<span class="opp">' + esc(m.opp || "Opp") + " " + (m.sopp || 0) + "</span>";
    show("duel-score");
  }

  function bindEdge(el, n, myTurn) {
    if (!myTurn) return;
    el.classList.add("tap");
    el.addEventListener("click", function () { move(n); });
  }

  function move(n) { A.sfx("drop"); A.vibe(15); send({ t: "move", n: n }); }

  /* ---- Phases ---- */
  function renderPlaying(m) {
    hide("duel-lobby"); hide("duel-over"); show("duel-match"); show("duel-leave");
    var myTurn = m.turn === m.you;
    var turn = $("duel-turn");
    turn.textContent = myTurn ? "Your turn" : (esc(m.opp || "Opponent") + "'s turn");
    turn.className = "turn" + (myTurn ? " you" : "");

    if (m.kind === "dots") renderDots(m);
    else if (m.kind === "reversi") renderReversi(m);
    else renderGrid(m);

    // Cue when the turn flips to us (skip the very first render of a match).
    if (prevPhase === "playing" && myTurn && !prevTurnMine) { A.sfx("tick"); A.vibe(30); }
    prev = m.kind === "dots"
      ? (m.hedges || []).concat(m.vedges || []).map(String)
      : (m.board || []).map(String);
    prevTurnMine = myTurn;
  }

  function renderOver(m) {
    hide("duel-lobby"); hide("duel-match"); show("duel-over"); show("duel-leave");
    var r = $("duel-result");
    var map = { win: ["Win", "win"], lose: ["Lose", "lose"], draw: ["Draw", "draw"] };
    var res = map[m.result] || ["Over", ""];
    r.textContent = res[0];
    r.className = "result " + res[1];
    if (prevPhase !== "over") {
      if (m.result === "win") { A.sfx("win"); A.vibe([40, 60, 40, 60, 120]); }
      else if (m.result === "lose") { A.sfx("lose"); A.vibe(200); }
    }
    prev = null; prevTurnMine = false;
  }

  function renderLobby(m) {
    hide("duel-match"); hide("duel-over"); show("duel-lobby"); hide("duel-leave");
    lobbyView($("duel-incoming"), $("duel-players"), m.challenges);
    prev = null; prevTurnMine = false;
  }

  A.handlers.duel = function (m) {
    route("duel");
    if (A.view !== "duel") return;   // still on landing; render when joined
    var title = GAME_LABEL[({ c4: "connect4", ttt: "tictactoe", dots: "dots", reversi: "reversi" })[m.kind]] || "Duel";
    $("duel-title").textContent = title;
    if (m.phase === "playing") renderPlaying(m);
    else if (m.phase === "over") renderOver(m);
    else renderLobby(m);
    prevPhase = m.phase;
  };

  document.addEventListener("DOMContentLoaded", function () {
    $("duel-leave").addEventListener("click", function () { send({ t: "leaveGame" }); });
    $("duel-back").addEventListener("click", function () { send({ t: "leaveGame" }); });
    $("duel-rematch").addEventListener("click", function () { A.sfx("buzz"); send({ t: "rematch" }); });
  });
})();
