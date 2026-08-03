/* Chess — 1v1, full FIDE rules refereed by the ESP. The server is the sole
   authority: it sends the whole board every push and the legal moves for
   whichever side is to move, so this file never runs chess logic of its own
   — it only renders `board`/`moves` and forwards two-tap intents. No
   optimistic updates: a tapped move is not reflected locally until the next
   server state confirms it. */
(function () {
  var lastMsg = null;       // latest chess message, read by the clock tick + click handler
  var selFrom = -1;         // selected origin square (0..63), -1 = none selected
  var movesFrom = {};       // origin square -> [target squares], rebuilt every render
  var pendingPromo = null;  // {from,to} awaiting a promotion pick, or null
  var clockTimer = null;    // 200ms clock tick, running only while phase is "playing"
  var prevPhase = "";
  var prevYourTurn = false;
  var prevMyCheck = false;  // last "your king is in check" state, for the check sfx edge
  var rematchTimer = null;
  var resignArmed = false;
  var resignTimer = null;

  function sub(name) {
    ["lobby", "play", "over"].forEach(function (id) {
      $("chess-" + id).classList.toggle("hide", id !== name);
    });
  }

  /* Orientation: each player sees their own back rank at the bottom. Displayed
     cell i (0 = top-left, row-major) maps to board square sq (0 = a1, rank-major
     to h8). */
  function squareAt(i, white) {
    return white ? (7 - Math.floor(i / 8)) * 8 + (i % 8) : Math.floor(i / 8) * 8 + (7 - (i % 8));
  }

  function buildMovesFrom(moves) {
    var out = {};
    (moves || []).forEach(function (v) {
      var f = Math.floor(v / 64), to = v % 64;
      (out[f] || (out[f] = [])).push(to);
    });
    return out;
  }

  function kingSquare(board, white) { return board.indexOf(white ? "K" : "k"); }

  // Filled glyph set for both colors (the hollow ♙♘♗♖♕♔ codepoints render as
  // un-stylable emoji on some Android fonts); color comes from the w/b class.
  var GLYPH = { P: "♟", N: "♞", B: "♝", R: "♜", Q: "♛", K: "♚" };

  function renderBoard(m) {
    var board = m.board;
    var chkSq = m.check ? kingSquare(board, m.wtm) : -1;
    var lastFrom = m.last >= 0 ? Math.floor(m.last / 64) : -1;
    var lastTo = m.last >= 0 ? m.last % 64 : -1;
    var el = $("chess-board");
    var rebuild = el.childElementCount !== 64;
    if (rebuild) el.innerHTML = "";
    for (var i = 0; i < 64; i++) {
      var cell = rebuild ? document.createElement("div") : el.children[i];
      var sq = squareAt(i, m.white);
      var rank = sq >> 3, file = sq & 7;
      var cls = "ch-cell " + ((rank + file) % 2 === 0 ? "dk" : "lt");
      if (sq === selFrom) cls += " sel";
      if (selFrom !== -1 && movesFrom[selFrom] && movesFrom[selFrom].indexOf(sq) >= 0) cls += " tgt";
      if (sq === lastFrom || sq === lastTo) cls += " last";
      if (sq === chkSq) cls += " chk";
      var ch = board.charAt(sq);
      if (ch !== ".") cls += ch === ch.toUpperCase() ? " w" : " b";
      cell.className = cls;
      cell.textContent = ch === "." ? "" : (GLYPH[ch.toUpperCase()] || "");
      if (rebuild) el.appendChild(cell);
    }
    el.onclick = m.yourTurn ? function (e) {
      var idx = Array.prototype.indexOf.call(el.children, e.target);
      if (idx < 0) return;
      onCellTap(m, squareAt(idx, m.white));
    } : null;
  }

  function onCellTap(m, sq) {
    if (selFrom === -1) {
      if (movesFrom[sq]) { selFrom = sq; A.vibe(8); renderBoard(m); }
      return;
    }
    if (sq === selFrom) { selFrom = -1; renderBoard(m); return; }
    if (movesFrom[selFrom] && movesFrom[selFrom].indexOf(sq) >= 0) {
      var from = selFrom, to = sq;
      var piece = m.board.charAt(from);
      var needsPromo = (piece === "P" && to >= 56) || (piece === "p" && to <= 7);
      selFrom = -1;
      if (needsPromo) { pendingPromo = { from: from, to: to }; renderBoard(m); showPromo(); }
      else { sendMove(from, to, 0); renderBoard(m); }
      return;
    }
    if (movesFrom[sq]) { selFrom = sq; A.vibe(8); renderBoard(m); return; }
    selFrom = -1;
    renderBoard(m);
  }

  function sendMove(from, to, promo) {
    A.sfx("drop"); A.vibe(15);
    var msg = { t: "move", from: from, to: to };
    if (promo) msg.promo = promo;
    send(msg);
  }

  function showPromo() { $("chess-promo").classList.remove("hide"); }
  // Always leaves the board in a fresh, consistent state (selFrom is already -1 by
  // the time this runs, from either caller) -- both the promo-pick and the
  // backdrop-cancel path route through here, so neither can leave a stale
  // sel/tgt highlight from the square that triggered the promotion overlay.
  function hidePromo() {
    $("chess-promo").classList.add("hide");
    pendingPromo = null;
    if (lastMsg) renderBoard(lastMsg);
  }

  /* ---- clocks ---- */
  function fmtClock(ms) {
    var s = Math.max(0, Math.ceil(ms / 1000));
    var mm = Math.floor(s / 60), ss = s % 60;
    return mm + ":" + (ss < 10 ? "0" : "") + ss;
  }
  // `liveMs` is the running side's current remaining time (computed from the
  // deadline while ticking); omitted, it falls back to the static m.run, which
  // is what the over screen must always use (see the module comment on quirks).
  function paintClocks(m, liveMs) {
    var running = liveMs == null ? m.run : liveMs;
    var paused = m.oms;
    var mine = m.wtm === m.white ? running : paused;
    var theirs = m.wtm === m.white ? paused : running;
    var myEl = $("chess-my-clock"), oppEl = $("chess-opp-clock");
    myEl.textContent = fmtClock(mine);
    oppEl.textContent = fmtClock(theirs);
    myEl.classList.toggle("hot", mine < 30000);
    oppEl.classList.toggle("hot", theirs < 30000);
  }
  function stopClockTimer() { if (clockTimer) { clearInterval(clockTimer); clockTimer = null; } }
  function startClockTimer() {
    stopClockTimer();
    clockTimer = setInterval(function () {
      // Only the running side moves; the over screen stops this timer entirely
      // (see renderOver) so it never fights the frozen m.run/m.oms values.
      if (!lastMsg || lastMsg.phase !== "playing") return;
      paintClocks(lastMsg, Math.max(0, lastMsg.deadline - serverNow()));
    }, 200);
  }

  /* ---- phases ---- */
  function renderPlay(m) {
    sub("play");
    if (m.run >= 1000) noteDeadline(m.deadline, m.run);
    movesFrom = buildMovesFrom(m.moves);
    if (selFrom !== -1 && !movesFrom[selFrom]) selFrom = -1;
    renderBoard(m);
    paintClocks(m);

    var turnEl = $("chess-turn");
    turnEl.textContent = m.yourTurn ? t("common.your_turn") : t("common.opp_turn", { nick: m.opp || t("common.opponent") });
    turnEl.className = "turn" + (m.yourTurn ? " you" : "");
    var myCheck = !!(m.check && m.yourTurn);
    $("chess-check").classList.toggle("hide", !myCheck);
    if (myCheck && !prevMyCheck) A.sfx("check");
    prevMyCheck = myCheck;

    var drawBtn = $("chess-draw");
    if (m.offer === m.you) { drawBtn.disabled = true; drawBtn.textContent = t("chess.offer_sent"); }
    else if (m.offer) { drawBtn.disabled = false; drawBtn.textContent = t("chess.accept_draw"); }
    else { drawBtn.disabled = false; drawBtn.textContent = t("chess.offer_draw"); }
    $("chess-claim").classList.toggle("hide", !(m.claim3 || m.claim50));

    if (m.yourTurn && !prevYourTurn) { A.sfx("tick"); A.vibe(30); }
    prevYourTurn = m.yourTurn;
  }

  function renderOver(m) {
    sub("over");
    if (m.run >= 1000) noteDeadline(m.deadline, m.run);
    // Quirk: the server keeps recomputing `deadline` even after the game ends,
    // so the over screen must read the frozen `run`/`oms` values statically —
    // never derive a countdown from `deadline` here.
    paintClocks(m);
    var r = $("chess-result");
    var RESULT_KEY = { win: "common.win", lose: "common.lose", draw: "common.draw" };
    r.textContent = t(RESULT_KEY[m.result] || "common.draw");
    r.className = "result " + (m.result === "win" ? "win" : m.result === "lose" ? "lose" : "draw");
    // A literal-keyed lookup, not string concat -- pr-check.mjs's i18n scanner only
    // sees direct t("literal") calls (same reason kmk.js's LBL[] is a lookup, not concat).
    var REASON_KEY = {
      mate: "chess.reason_mate", stalemate: "chess.reason_stalemate", resign: "chess.reason_resign",
      flag: "chess.reason_flag", flagdraw: "chess.reason_flagdraw", material: "chess.reason_material",
      rep3: "chess.reason_rep3", rep5: "chess.reason_rep5", move50: "chess.reason_move50",
      move75: "chess.reason_move75", agree: "chess.reason_agree", left: "chess.reason_left",
    };
    $("chess-reason").textContent = t(REASON_KEY[m.reason] || m.reason);
    if (prevPhase !== "over") {
      if (m.result === "win") { A.sfx("win"); A.vibe([40, 60, 40, 60, 120]); }
      else if (m.result === "lose") { A.sfx("lose"); A.vibe(200); }
    }
  }

  A.handlers.chess = function (m) {
    route("chess");
    if (A.view !== "chess") return;
    if (rematchTimer && m.phase !== "over") { clearTimeout(rematchTimer); rematchTimer = null; }
    lastMsg = m;
    $("chess-leave").classList.toggle("hide", m.phase === "lobby");
    if (m.phase === "lobby") {
      stopClockTimer();
      selFrom = -1; movesFrom = {}; hidePromo(); prevYourTurn = false; prevMyCheck = false;
      sub("lobby");
      A.lobbyView($("chess-incoming"), $("chess-players"), m.challenges);
    } else if (m.phase === "playing") {
      if (prevPhase !== "playing") { startClockTimer(); selFrom = -1; }
      renderPlay(m);
    } else if (m.phase === "over") {
      stopClockTimer();
      hidePromo();
      renderOver(m);
    }
    prevPhase = m.phase;
  };

  document.addEventListener("DOMContentLoaded", function () {
    $("chess-resign").addEventListener("click", function () {
      var btn = $("chess-resign");
      if (!resignArmed) {
        resignArmed = true;
        btn.textContent = t("chess.sure");
        if (resignTimer) clearTimeout(resignTimer);
        resignTimer = setTimeout(function () {
          resignArmed = false; resignTimer = null;
          btn.textContent = t("chess.resign");
        }, 2000);
      } else {
        clearTimeout(resignTimer); resignTimer = null; resignArmed = false;
        btn.textContent = t("chess.resign");
        A.sfx("buzz"); send({ t: "resign" });
      }
    });
    $("chess-draw").addEventListener("click", function () { A.sfx("buzz"); send({ t: "draw" }); });
    $("chess-claim").addEventListener("click", function () { A.sfx("buzz"); send({ t: "claim" }); });
    $("chess-leave").addEventListener("click", function () { send({ t: "leaveGame" }); });
    $("chess-back").addEventListener("click", function () { send({ t: "leaveGame" }); });
    $("chess-rematch").addEventListener("click", function () {
      A.sfx("buzz"); send({ t: "rematch" });
      if (rematchTimer) clearTimeout(rematchTimer);
      rematchTimer = setTimeout(function () {
        rematchTimer = null;
        if (prevPhase === "over" && A.view === "chess") { toast(t("common.opp_left")); send({ t: "leaveGame" }); }
      }, 1500);
    });

    var promoBtns = document.querySelectorAll("#chess-promo .ch-promo-btn");
    for (var pi = 0; pi < promoBtns.length; pi++) {
      (function (btn) {
        btn.addEventListener("click", function () {
          if (!pendingPromo) return;
          var promo = parseInt(btn.getAttribute("data-promo"), 10);
          sendMove(pendingPromo.from, pendingPromo.to, promo);
          hidePromo(); // re-renders the board (see hidePromo's comment)
        });
      })(promoBtns[pi]);
    }
    // Tap the dimmed backdrop (outside the card) to cancel the pending move.
    $("chess-promo").addEventListener("click", function (e) { if (e.target === $("chess-promo")) hidePromo(); });
  });
})();
