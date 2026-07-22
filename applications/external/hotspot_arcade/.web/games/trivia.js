/* Trivia module. The ESP is authoritative and self-organizing: it drives the
   whole flow through {t:"trivia", phase, ...} messages and we render server
   state, sending only intents (ready / vote / answer / again). Five sub-states
   live inside the one Trivia screen: lobby, countdown, question, reveal, final. */
(function () {
  var LABELS = ["A", "B", "C", "D"];
  var last = null;      // last trivia message (kept for reference)
  var revealedFor = -1; // question index we already played the reveal sound for
  var myready = false;  // mirrors the lobby ready toggle for the ready intent
  var finaledFor = false; // played the final win/lose cue for this game once

  // Show exactly one trivia sub-state block.
  function sub(name) {
    ["lobby", "count", "play", "final"].forEach(function (id) {
      $("tv-" + id).classList.toggle("hide", id !== name);
    });
  }

  // Leaderboard is the shared component (A.showLead), mounted from the board.
  function renderLead(m, bump) { A.showLead(m.board || [], bump); }

  // ---- lobby (ready + vote) -------------------------------------------------
  function renderLobby(m) {
    sub("lobby");
    A.hideLead();
    finaledFor = false;
    myready = A.readyLobby({ players: m.players, listId: "tv-players", readyId: "tv-ready", meId: "tv-lobby-me" });

    var tl = $("tv-topics");
    tl.innerHTML = "";
    (m.topics || []).forEach(function (tp, i) {
      var b = document.createElement("button");
      b.className = "topic" + (m.myvote === i ? " mine" : "");
      b.innerHTML =
        '<span class="txt">' + esc(tp.name) + "</span>" +
        '<span class="votes">' + (tp.votes || 0) + "</span>";
      b.addEventListener("click", function () {
        A.sfx("buzz"); A.vibe(12);
        send({ t: "vote", topic: i });
      });
      tl.appendChild(b);
    });
  }

  // ---- countdown ------------------------------------------------------------
  function renderCount(m) {
    sub("count");
    A.hideLead();
    $("tv-count-topic").textContent = m.topic || "";
    A.countdown("tv-count-num", m.secs);
  }

  // ---- question / reveal shared option rendering ----------------------------
  function head(m) {
    // Question number first so it's never the part that gets truncated; the topic
    // name follows and ellipsises if the row is tight.
    $("tv-qmeta").textContent =
      "Q" + ((m.i || 0) + 1) + " / " + (m.n || 0) + (m.topic ? " · " + m.topic : "");
  }

  function renderOpts(m, reveal, mine) {
    var opts = $("tv-opts");
    var counts = m.counts || [];
    var picked = mine >= 0;
    opts.innerHTML = "";
    (m.o || []).forEach(function (text, i) {
      var b = document.createElement("button");
      b.className = "opt";
      var cls = [];
      if (reveal) {
        if (i === m.correct) cls.push("correct");
        else if (i === mine) cls.push("wrong");
      } else if (i === mine) {
        cls.push("mine");
      }
      if (cls.length) b.className += " " + cls.join(" ");
      if (reveal || picked) b.disabled = true;

      var cnt = reveal ? ('<span class="cnt">' + (counts[i] || 0) + "</span>") : "";
      b.innerHTML =
        '<span class="lab">' + LABELS[i] + "</span>" +
        '<span class="txt">' + esc(text) + "</span>" + cnt;

      if (!reveal) b.addEventListener("click", function () {
        if (b.disabled) return;
        A.sfx("buzz"); A.vibe(15);
        send({ t: "answer", c: i });
        // Optimistic lock; the server confirms via the next trivia message.
        Array.prototype.forEach.call(opts.children, function (c) { c.disabled = true; });
        b.classList.add("mine");
      });
      opts.appendChild(b);
    });
  }

  function renderQuestion(m) {
    sub("play");
    revealedFor = -1;
    head(m);
    $("tv-answered").textContent = "Answered " + (m.answered || 0) + " / " + (m.total || 0);
    $("tv-q").textContent = m.q || "";
    var mine = (typeof m.mine === "number") ? m.mine : -1;
    noteDeadline(m.deadline, m.dur); A.timebar("tv-bar", m.deadline, m.dur, true);
    renderOpts(m, false, mine);
    renderLead(m, false);
  }

  function renderReveal(m) {
    sub("play");
    A.timebarStop("tv-bar"); hide("tv-bar");
    head(m);
    var gained = (typeof m.gained === "number") ? m.gained : 0;
    $("tv-answered").textContent = gained > 0 ? "+" + gained + " pts" : "";
    $("tv-q").textContent = m.q || "";
    var mine = (typeof m.mine === "number") ? m.mine : -1;
    renderOpts(m, true, mine);
    // Outcome cue once per question: correct if you nailed it, wrong if you
    // picked and missed, silent if you never answered.
    if (revealedFor !== m.i) {
      revealedFor = m.i;
      if (mine >= 0) {
        if (mine === m.correct) { A.sfx("correct"); A.vibe([25, 40, 25]); }
        else { A.sfx("wrong"); A.vibe(120); }
      }
    }
    renderLead(m, true);   // subtle bump as the board updates
  }

  // ---- final ----------------------------------------------------------------
  function renderFinal(m) {
    sub("final");
    A.hideLead();
    var b = A.podium("tv-podium", m.board);
    // Win/lose cue once per game.
    if (!finaledFor) {
      finaledFor = true;
      if (b.length && b[0].pid === A.pid) { A.sfx("win"); A.vibe([30, 50, 30, 50, 60]); }
      else { A.sfx("lose"); }
    }
  }

  function render(m) {
    route("trivia");
    if (A.view !== "trivia") return;   // still on landing; render once joined
    switch (m.phase) {
      case "lobby": renderLobby(m); break;
      case "countdown": renderCount(m); break;
      case "question": renderQuestion(m); break;
      case "reveal": renderReveal(m); break;
      case "final": renderFinal(m); break;
    }
  }

  // One-time wiring for the persistent controls (script runs after the DOM
  // body, so these elements already exist). The leaderboard toggle lives in
  // app.js now (shared across games).
  $("tv-ready").addEventListener("click", function () {
    A.sfx("buzz"); A.vibe(15);
    send({ t: "ready", ready: !myready });
  });
  $("tv-again").addEventListener("click", function () {
    A.sfx("start"); A.vibe(20);
    send({ t: "again" });
  });

  A.handlers.trivia = function (m) { last = m; render(m); };
})();
