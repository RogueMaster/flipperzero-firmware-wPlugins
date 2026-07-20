/* Draw & Guess. Registers handlers for {t:"draw"} (round/role/phase/final),
   {t:"ink"} (relayed segments for guessers). Incoming {t:"chat"} (wrong
   guesses) is handled centrally in app.js. Coordinates on the wire are
   normalised 0..1 so any canvas size renders the same picture. The server
   rotates the drawer, runs the clock, and ends the game with a final board. */
(function () {
  var canvas, cx;               // 2D context
  var drawing = false;          // pointer is down (drawer only)
  var lastX = 0, lastY = 0;
  var lastSent = 0;             // throttle stroke sends
  var role = "";                // "drawer" | "guesser"
  var revealedRound = -1;       // reveal sound guard
  var finaledFor = false;       // played the final win/lose cue once

  function ready() {
    canvas = $("draw-canvas");
    cx = canvas.getContext("2d");
  }

  // Match the backing store to the CSS box so lines stay crisp; then clear.
  function sizeCanvas() {
    var r = canvas.getBoundingClientRect();
    var w = Math.max(1, Math.round(r.width));
    var h = Math.max(1, Math.round(r.height));
    if (canvas.width !== w || canvas.height !== h) {
      canvas.width = w; canvas.height = h;
    }
    clearCanvas();
  }

  function clearCanvas() {
    cx.fillStyle = "#EDEDE6";               // off-white "paper"
    cx.fillRect(0, 0, canvas.width, canvas.height);
  }

  function seg(x0, y0, x1, y1) {
    cx.strokeStyle = "#111";
    cx.lineWidth = Math.max(2, canvas.width * 0.008);
    cx.lineCap = "round"; cx.lineJoin = "round";
    cx.beginPath();
    cx.moveTo(x0 * canvas.width, y0 * canvas.height);
    cx.lineTo(x1 * canvas.width, y1 * canvas.height);
    cx.stroke();
  }

  function norm(e) {
    var r = canvas.getBoundingClientRect();
    var t = e.touches && e.touches[0] ? e.touches[0] : e;
    return {
      x: Math.min(1, Math.max(0, (t.clientX - r.left) / r.width)),
      y: Math.min(1, Math.max(0, (t.clientY - r.top) / r.height)),
    };
  }

  function down(e) {
    if (role !== "drawer") return;
    e.preventDefault();
    drawing = true;
    var p = norm(e); lastX = p.x; lastY = p.y;
  }
  function moveEvt(e) {
    if (role !== "drawer" || !drawing) return;
    e.preventDefault();
    var now = Date.now();
    if (now - lastSent < 20) return;        // ~50/s cap
    lastSent = now;
    var p = norm(e);
    seg(lastX, lastY, p.x, p.y);            // draw locally
    send({ t: "stroke", x0: lastX, y0: lastY, x1: p.x, y1: p.y });
    lastX = p.x; lastY = p.y;
  }
  function up() { drawing = false; }

  function blanks(len) {
    var s = "";
    for (var i = 0; i < len; i++) s += (i ? " " : "") + "_";
    return s;
  }

  function chatLine(nick, text, cls) {
    var log = $("draw-chat");
    var div = document.createElement("div");
    div.className = "cmsg" + (cls ? " " + cls : "");
    div.innerHTML = '<b>' + esc(nick) + "</b> " + esc(text);
    log.appendChild(div);
    log.scrollTop = log.scrollHeight;
  }

  // The round timer, drawer podium, and chat routing are shared (see app.js).
  function renderPodium(board) {
    var b = A.podium("draw-podium", board);
    if (!finaledFor) {
      finaledFor = true;
      if (b.length && b[0].pid === A.pid) { A.sfx("win"); A.vibe([30, 50, 30, 50, 60]); }
      else { A.sfx("lose"); }
    }
  }

  A.handlers.draw = function (m) {
    route("draw");
    if (A.view !== "draw") return;
    if (!canvas) ready();

    var status = $("draw-status"), word = $("draw-word");
    var reveal = $("draw-reveal");

    if (m.phase === "final") {
      role = ""; A.timebarStop("draw-bar");
      hide("draw-bar"); hide("draw-word"); hide("draw-canvas");
      hide("draw-tools"); hide("draw-guess"); hide("draw-reveal");
      show("draw-final");
      status.textContent = "Game over";
      renderPodium(m.board);
      return;
    }

    // Leaving the final screen (or any non-final message): restore the board.
    hide("draw-final"); show("draw-word"); show("draw-canvas");
    finaledFor = false;

    if (m.phase === "reveal") {
      role = ""; A.timebarStop("draw-bar"); hide("draw-bar");
      hide("draw-tools"); hide("draw-guess"); show("draw-reveal");
      var who = m.winner == null ? "Nobody guessed it" : (esc(nickOf(m.winner)) + " got it");
      reveal.innerHTML = "Word: <b>" + esc(m.word || "") + "</b><br>" + who;
      status.textContent = "Round over";
      if (revealedRound !== m.round) {
        revealedRound = (m.round == null ? -2 : m.round);
        if (m.winner === A.pid) { A.sfx("win"); A.vibe([40, 60, 40]); }
        else if (m.winner != null) A.sfx("score");
      }
      return;
    }

    if (m.phase === "idle") {
      role = ""; A.timebarStop("draw-bar"); hide("draw-bar");
      hide("draw-tools"); hide("draw-guess"); show("draw-reveal");
      reveal.textContent = "Get ready...";
      status.textContent = "Waiting";
      word.textContent = "";
      return;
    }

    // phase === "draw"
    hide("draw-reveal");
    show("draw-word");
    role = m.role;
    revealedRound = -1;
    status.textContent = "Round " + (m.round || 1) + " / " + (m.rounds || m.round || 1);
    noteDeadline(m.deadline, m.dur);
    A.timebar("draw-bar", m.deadline, m.dur, false);

    if (role === "drawer") {
      word.className = "draw-word";
      word.textContent = m.word || "";
      show("draw-tools"); hide("draw-guess");
      sizeCanvas();
      canvas.classList.add("drawable");
    } else {
      word.className = "draw-word blanks";
      word.textContent = blanks(m.len || 0);
      hide("draw-tools"); show("draw-guess");
      $("draw-chat").innerHTML = "";
      var artist = esc(m.drawer || "Someone");
      chatLine("—", artist + " is drawing", "sys");
      sizeCanvas();
      canvas.classList.remove("drawable");
    }
  };

  A.handlers.ink = function (m) {
    if (A.view !== "draw" || role !== "guesser") return;
    if (m.clear) { clearCanvas(); return; }
    seg(m.x0, m.y0, m.x1, m.y1);
  };

  document.addEventListener("DOMContentLoaded", function () {
    ready();
    canvas.addEventListener("pointerdown", down);
    canvas.addEventListener("pointermove", moveEvt);
    window.addEventListener("pointerup", up);
    // Touch fallback for browsers without pointer events.
    canvas.addEventListener("touchstart", down, { passive: false });
    canvas.addEventListener("touchmove", moveEvt, { passive: false });
    window.addEventListener("touchend", up);

    $("draw-clear").addEventListener("click", function () {
      if (role !== "drawer") return;
      clearCanvas(); A.sfx("buzz"); send({ t: "clear" });
    });

    $("draw-form").addEventListener("submit", function (e) {
      e.preventDefault();
      var inp = $("draw-input");
      var text = inp.value.trim();
      if (!text) return;
      A.sfx("buzz"); A.vibe(15);
      // Do NOT echo locally: the server broadcasts guesses back as {t:"chat"}.
      send({ t: "guess", text: text });
      inp.value = "";
    });

    $("draw-again").addEventListener("click", function () {
      A.sfx("start"); A.vibe(20);
      send({ t: "again" });
    });

    window.addEventListener("resize", function () {
      if (A.view === "draw" && canvas) {
        // Resizing wipes the picture; the drawer keeps drawing, guessers wait
        // for the next stroke. Acceptable for a casual party game.
        sizeCanvas();
      }
    });
  });
})();
