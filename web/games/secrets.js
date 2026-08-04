/* Secrets — a whole-group hidden-vote party game. The ESP is authoritative, driving
   {t:"secrets", phase, ...}: lobby (ready + pack vote) -> countdown -> predict (secretly
   guess how many of the N players will say yes) -> answer (secretly tap Yes/No) -> reveal
   (only the group's total yes-count, plus your own result) -> ... -> final podium. We send
   ready / vote / predict / reply / again. Anonymity is enforced server-side: your own
   prediction/answer/points reach only you; nobody ever sees who answered what. */
(function () {
  var myready = false;
  var predVal = 0, predMax = 0, predRound = -1; // number-stepper state for the predict view

  function sub(name) {
    ["lobby", "count", "play", "final"].forEach(function (id) {
      $("sec-" + id).classList.toggle("hide", id !== name);
    });
  }
  function stopBar() { A.timebarStop("sec-bar"); hide("sec-bar"); }

  function renderLobby(m) {
    sub("lobby");
    stopBar();
    myready = A.readyLobby({ players: m.players, listId: "sec-players", readyId: "sec-ready", meId: "sec-me" });
    A.packVote({
      boxId: "sec-topics",
      packs: m.packs, myvote: m.myvote,
      onVote: function (i) { send({ t: "vote", pack: i }); },
    });
  }

  function renderCount(m) {
    sub("count");
    stopBar();
    A.countdown("sec-count-num", m.sec);
  }

  function setNum() { $("sec-num").textContent = predVal; }

  function renderPredict(m) {
    show("sec-predict"); hide("sec-answer"); hide("sec-reveal");
    var locked = (typeof m.myprediction === "number" && m.myprediction >= 0);
    predMax = m.n;
    // Fresh round: seed the stepper at the middle of the 0..N range.
    if (predRound !== m.round) { predVal = Math.round(m.n / 2); predRound = m.round; }
    if (locked) predVal = m.myprediction;
    if (predVal > predMax) predVal = predMax;
    setNum();
    // Prominent prompt above the stepper (past tense: answers came first this round).
    $("sec-predict-label").textContent = t("secrets.predict_hint", { n: m.n });
    $("sec-minus").disabled = locked;
    $("sec-plus").disabled = locked;
    $("sec-predict-go").disabled = locked;
    $("sec-predict-go").classList.toggle("hide", locked);
    $("sec-note").textContent = locked ? t("secrets.predict_locked", { n: m.myprediction }) : "";
  }

  function renderAnswer(m) {
    hide("sec-predict"); show("sec-answer"); hide("sec-reveal");
    var locked = (typeof m.myanswer === "number" && m.myanswer >= 0);
    $("sec-yes").disabled = locked;
    $("sec-no").disabled = locked;
    $("sec-yes").classList.toggle("mine", m.myanswer === 1);
    $("sec-no").classList.toggle("mine", m.myanswer === 0);
    $("sec-note").textContent = locked ? t("secrets.answer_locked") : t("secrets.answer_hint");
  }

  var revealedFor = -1;
  function renderReveal(m) {
    hide("sec-predict"); hide("sec-answer"); show("sec-reveal");
    // Hero: the yes-count as a big number with a small caption beneath it.
    $("sec-yesnum").textContent = m.yes;
    // Bucket every player's prediction by the number they guessed.
    var buckets = {};
    (m.guesses || []).forEach(function (g) { (buckets[g.n] = buckets[g.n] || []).push(g); });
    // One row per possible count 0..N (a proper axis, empty rows included); the row whose
    // number equals the actual yes-count is highlighted, and each row carries the names of
    // whoever guessed it (with their points).
    var ul = $("sec-buckets");
    ul.innerHTML = "";
    for (var i = 0; i <= m.n; i++) {
      var li = document.createElement("li");
      li.className = "sec-bucket" + (i === m.yes ? " hit" : "");
      var num = document.createElement("span");
      num.className = "sec-bnum";
      num.textContent = i;
      var names = document.createElement("span");
      names.className = "sec-bnames";
      (buckets[i] || []).forEach(function (g) {
        var chip = document.createElement("span");
        chip.className = "sec-bname";
        // Nicknames are player-typed, so use a text node (not innerHTML).
        chip.textContent = g.nick + (g.pts > 0 ? " +" + g.pts : "");
        names.appendChild(chip);
      });
      li.appendChild(num);
      li.appendChild(names);
      ul.appendChild(li);
    }
    var gain = (typeof m.mygain === "number") ? m.mygain : 0;
    $("sec-result").textContent = gain >= 3 ? t("secrets.result_exact", { gain: gain })
      : gain > 0 ? t("secrets.result_close", { gain: gain })
        : t("common.zero_round");
    if (revealedFor !== m.round) {
      revealedFor = m.round;
      A.sfx(gain > 0 ? "correct" : "buzz"); A.vibe(gain > 0 ? 25 : 12);
    }
  }

  function renderPlay(m) {
    sub("play");
    $("sec-meta").textContent = t("common.round", { n: m.round, total: m.rounds });
    $("sec-progress").textContent = t("secrets.locked_count", { n: m.locked, total: m.total });
    $("sec-q").textContent = m.q || "";
    // The timer bar ticks the predict/answer window, and the pause before the next
    // question while revealing.
    noteDeadline(m.deadline, m.dur);
    A.timebar("sec-bar", m.deadline, m.dur, m.phase !== "reveal");
    // The +gain line belongs to reveal only; clear it while answering/predicting.
    if (m.phase !== "reveal") $("sec-result").textContent = "";
    if (m.phase === "reveal") renderReveal(m);
    else { revealedFor = -1; if (m.phase === "predict") renderPredict(m); else renderAnswer(m); }
  }

  function renderFinal(m) {
    sub("final");
    stopBar();
    var b = A.podium("sec-podium", m.board);
    if (b && b.length && b[0].pid === A.pid) { A.sfx("win"); A.vibe([30, 50, 30]); }
    else { A.sfx("start"); A.vibe(20); }
  }

  A.handlers.secrets = function (m) {
    route("secrets");
    if (A.view !== "secrets") return;
    switch (m.phase) {
      case "lobby": renderLobby(m); break;
      case "countdown": renderCount(m); break;
      case "predict": case "answer": case "reveal": renderPlay(m); break;
      case "final": renderFinal(m); break;
    }
  };

  $("sec-ready").addEventListener("click", function () {
    A.sfx("buzz"); A.vibe(15);
    send({ t: "ready", ready: !myready });
  });
  $("sec-minus").addEventListener("click", function () {
    if (predVal > 0) { predVal--; setNum(); A.sfx("tick"); A.vibe(8); }
  });
  $("sec-plus").addEventListener("click", function () {
    if (predVal < predMax) { predVal++; setNum(); A.sfx("tick"); A.vibe(8); }
  });
  $("sec-predict-go").addEventListener("click", function () {
    A.sfx("start"); A.vibe(20);
    send({ t: "predict", n: predVal });
  });
  $("sec-yes").addEventListener("click", function () {
    A.sfx("buzz"); A.vibe(18);
    send({ t: "reply", v: 1 });
  });
  $("sec-no").addEventListener("click", function () {
    A.sfx("buzz"); A.vibe(18);
    send({ t: "reply", v: 0 });
  });
  $("sec-again").addEventListener("click", function () {
    A.sfx("start"); A.vibe(20);
    send({ t: "again" });
  });
})();
