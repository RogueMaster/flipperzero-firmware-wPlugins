/* Word Scramble race — everyone unscrambles the same word; the fastest correct
   guesses earn the most. Server-driven {t:"scramble", phase, ...}: lobby -> countdown
   -> play (type the word, live scores, timer) -> reveal (the answer) -> ... -> final
   podium. We send ready / guess / again. */
(function () {
  var myready = false;
  var solvedFor = -1;

  // Upper-case ASCII letters only, so accented letters keep the case the answer check
  // accepts and stay one tile each. "ß" is special: its JS upper-case is "SS" (two
  // tiles, breaks the letter count), so map it to the capital esz-zett "ẞ" instead --
  // one uppercase glyph that still reads as a ß and still matches a typed "ß".
  function upTiles(s) {
    return String(s || "")
      .replace(/[a-z]/g, function (c) { return c.toUpperCase(); })
      .replace(/ß/g, "ẞ");
  }

  function sub(name) {
    ["lobby", "count", "play", "final"].forEach(function (id) {
      $("scr-" + id).classList.toggle("hide", id !== name);
    });
  }

  function renderLobby(m) {
    sub("lobby");
    A.hideLead();
    myready = A.readyLobby({ players: m.players, listId: "scr-players", readyId: "scr-ready", meId: "scr-me" });
    A.packVote({
      boxId: "scr-topics",
      packs: m.packs, myvote: m.myvote,
      onVote: function (i) { send({ t: "vote", pack: i }); },
    });
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
      // Upper-case ASCII only: "ß".toUpperCase() is "SS" (two chars) and umlauts would
      // show a case the ASCII-folding answer check doesn't accept, so the tiles would
      // stop matching what you type. Leaving non-ASCII as-is keeps display == answer.
      letters.textContent = upTiles(m.word);
      form.classList.add("hide");
      status.textContent = "Answer";
      solvedFor = -1;
    } else {
      noteDeadline(m.deadline, m.dur); A.timebar("scr-bar", m.deadline, m.dur, false);
      letters.className = "scr-letters";
      letters.textContent = upTiles(m.scram).split("").join(" ");
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
