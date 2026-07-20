/* Word Scramble race — everyone unscrambles the same word; the fastest correct
   guesses earn the most. Server-driven {t:"scramble", phase, ...}: lobby -> countdown
   -> play (type the word, live scores, timer) -> reveal (the answer) -> ... -> final
   podium. We send ready / guess / again. */
(function () {
  var myready = false;
  var solvedFor = -1;

  function sub(name) {
    ["lobby", "count", "play", "final"].forEach(function (id) {
      $("scr-" + id).classList.toggle("hide", id !== name);
    });
  }

  function renderLobby(m) {
    sub("lobby");
    A.hideLead();
    myready = A.readyLobby({ players: m.players, listId: "scr-players", readyId: "scr-ready", meId: "scr-me" });
  }

  function renderCount(m) {
    sub("count");
    A.hideLead();
    A.countdown("scr-count-num", m.sec);
  }

  function renderPlay(m) {
    sub("play");
    var reveal = m.phase === "reveal";
    $("scr-meta").textContent = "Word " + m.round + " / " + m.rounds;
    var letters = $("scr-letters");
    var form = $("scr-form"), status = $("scr-status");
    if (reveal) {
      A.timebarStop("scr-bar"); hide("scr-bar");
      letters.className = "scr-letters answer";
      letters.textContent = (m.word || "").toUpperCase();
      form.classList.add("hide");
      status.textContent = "Answer";
      solvedFor = -1;
    } else {
      noteDeadline(m.deadline, m.dur); A.timebar("scr-bar", m.deadline, m.dur, false);
      letters.className = "scr-letters";
      letters.textContent = (m.scram || "").toUpperCase().split("").join(" ");
      var solved = !!m.solved;
      form.classList.toggle("hide", solved);
      status.textContent = solved ? "Solved! Waiting for the round to end." : (m.len + " letters");
      if (solved && solvedFor !== m.round) { solvedFor = m.round; A.sfx("correct"); A.vibe([25, 40, 25]); }
    }
    A.showLead(m.scores || [], reveal);
  }

  function renderFinal(m) {
    sub("final");
    A.hideLead();
    var b = A.podium("scr-podium", m.scores);
    if (b.length && b[0].pid === A.pid) { A.sfx("win"); A.vibe([30, 50, 30]); }
    else A.sfx("lose");
  }

  A.handlers.scramble = function (m) {
    route("scramble");
    if (A.view !== "scramble") return;
    switch (m.phase) {
      case "lobby": renderLobby(m); break;
      case "countdown": renderCount(m); break;
      case "play": case "reveal": renderPlay(m); break;
      case "final": renderFinal(m); break;
    }
  };

  $("scr-ready").addEventListener("click", function () {
    A.sfx("buzz"); A.vibe(15);
    send({ t: "ready", ready: !myready });
  });
  $("scr-again").addEventListener("click", function () {
    A.sfx("start"); A.vibe(20);
    send({ t: "again" });
  });
  $("scr-form").addEventListener("submit", function (e) {
    e.preventDefault();
    var inp = $("scr-input");
    var g = inp.value.trim().slice(0, 24);
    inp.value = "";
    if (!g) return;
    A.sfx("buzz"); A.vibe(12);
    send({ t: "guess", text: g });
  });
})();
