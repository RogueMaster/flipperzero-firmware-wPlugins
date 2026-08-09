/* Kiss Marry Kill — predict a player's picks. Server-driven {t:"kmk", phase, ...}:
   lobby (ready + pack vote) -> countdown -> play (stage choose: the chooser tags
   the three people Kiss/Marry/Kill; stage guess: everyone else predicts that; stage
   reveal: the chooser's tags + who got them right) -> ... -> final podium. We send
   ready / vote / assign / again. Labels: 0 kiss, 1 marry, 2 kill. */
(function () {
  var myready = false;
  var LBL = ["kmk.kiss", "kmk.marry", "kmk.kill"]; // index = label key
  var EMO = ["💋", "💍", "💀"]; // kiss/marry/kill

  function sub(name) {
    ["lobby", "count", "play", "final"].forEach(function (id) {
      $("kmk-" + id).classList.toggle("hide", id !== name);
    });
  }
  function stopBar() { A.timebarStop("kmk-bar"); hide("kmk-bar"); }

  // My working assignment while choosing/guessing: label per person (-1 = none).
  var mine = [-1, -1, -1];
  var canEdit = false;

  function valid(a) { // a permutation of 0,1,2?
    return a.indexOf(0) >= 0 && a.indexOf(1) >= 0 && a.indexOf(2) >= 0;
  }
  function nextLabel(cur) { return cur >= 2 ? -1 : cur + 1; } // none->kiss->marry->kill->none

  function renderLobby(m) {
    sub("lobby");
    stopBar();
    myready = A.readyLobby({ players: m.players, listId: "kmk-players", readyId: "kmk-ready", meId: "kmk-me" });
    A.packVote({
      boxId: "kmk-topics",
      packs: m.packs, myvote: m.myvote,
      onVote: function (i) { send({ t: "vote", pack: i }); },
    });
  }

  function renderCount(m) {
    sub("count");
    stopBar();
    A.countdown("kmk-count-num", m.sec);
  }

  // Render the three people as tappable rows showing the current label. On reveal a
  // guesser sees the answer with a ✓/✗ against their own pick; the chooser instead
  // sees, per person, how many guessers read them right (counts/total).
  function renderPeople(people, labels, interactive, answer, counts, total) {
    var box = $("kmk-people");
    box.innerHTML = "";
    (people || []).forEach(function (name, i) {
      var row = document.createElement("div");
      row.className = "kmk-row";
      var lab = labels && labels[i] >= 0 ? labels[i] : -1;
      var tag = lab >= 0 ? '<span class="kmk-tag l' + lab + '">' + EMO[lab] + " " + t(LBL[lab]) + "</span>"
                         : '<span class="kmk-tag none">' + t("kmk.tap_set") + '</span>';
      var extra = "";
      if (answer) {
        tag = '<span class="kmk-tag l' + answer[i] + '">' + EMO[answer[i]] + " " + t(LBL[answer[i]]) + "</span>";
        if (counts) {
          // chooser view: how many guessers matched this person
          extra = '<span class="kmk-count">' + counts[i] + "/" + total + "</span>";
        } else if (labels) {
          var right = labels[i] === answer[i];
          extra = '<span class="kmk-mark ' + (right ? "ok" : "no") + '">' + (right ? "✓" : "✗") + "</span>";
        }
      }
      row.innerHTML = '<span class="kmk-name">' + esc(name) + "</span>" + tag + extra;
      if (interactive) {
        row.classList.add("tap");
        row.addEventListener("click", function () {
          if (!canEdit) return;
          mine[i] = nextLabel(mine[i]);
          A.sfx("buzz"); A.vibe(10);
          renderPeople(people, mine, true);
          $("kmk-go").disabled = !valid(mine);
        });
      }
      box.appendChild(row);
    });
  }

  var lastRound = -1, lastStage = "";
  function renderPlay(m) {
    sub("play");
    var stage = m.stage; // choose | guess | reveal
    $("kmk-meta").textContent = t("common.round", { n: m.round, total: m.rounds });
    $("kmk-role").textContent = m.iam ? t("kmk.you_choose") : t("kmk.chooser_is", { nick: m.chooser });
    noteDeadline(m.deadline, m.dur);
    A.timebar("kmk-bar", m.deadline, m.dur, false);

    var go = $("kmk-go"), note = $("kmk-note");

    // fresh round/stage: reset my working assignment (or adopt what the server has)
    if (lastRound !== m.round || lastStage !== stage) {
      mine = (m.mine && m.mine.length === 3) ? m.mine.slice() : [-1, -1, -1];
    }

    if (stage === "reveal") {
      canEdit = false;
      go.classList.add("hide");
      var g = (typeof m.mygain === "number") ? m.mygain : 0;
      if (m.iam) {
        // chooser: no self-check; show how many guessers read each person right
        var gs = m.guesses || [];
        var counts = [0, 0, 0];
        gs.forEach(function (gg) {
          for (var i = 0; i < 3; i++) if (gg.pick && gg.pick[i] === m.answer[i]) counts[i]++;
        });
        renderPeople(m.people, null, false, m.answer, counts, gs.length);
        var perfect = gs.filter(function (gg) { return gg.pts >= 3; }).length;
        note.textContent = t("kmk.read_perfect", { perfect: perfect, total: gs.length, g: g });
      } else {
        renderPeople(m.people, m.mine, false, m.answer);
        note.textContent = g >= 3 ? t("kmk.perfect", { g: g }) : g ? t("kmk.close", { g: g }) : t("common.zero_round");
      }
      if (lastRound !== m.round || lastStage !== "reveal") { A.sfx(g ? "correct" : "buzz"); A.vibe(g ? 25 : 12); }
    } else if (stage === "choose") {
      if (m.iam) {
        canEdit = true;
        renderPeople(m.people, mine, true);
        go.classList.remove("hide");
        go.textContent = t("kmk.lock_picks");
        go.disabled = !valid(mine);
        note.textContent = t("kmk.tap_tag");
      } else {
        canEdit = false;
        renderPeople(m.people, null, false);
        go.classList.add("hide");
        note.textContent = t("kmk.deciding", { nick: m.chooser });
      }
    } else { // guess
      if (m.iam) {
        canEdit = false;
        renderPeople(m.people, m.answer, false); // chooser sees their own locked picks
        go.classList.add("hide");
        note.textContent = t("kmk.locked_waiting");
      } else {
        var locked = m.mine && valid(m.mine);
        canEdit = !locked;
        renderPeople(m.people, locked ? m.mine : mine, !locked);
        go.classList.toggle("hide", locked);
        go.textContent = t("kmk.lock_guess");
        go.disabled = !valid(mine);
        note.textContent = locked ? t("kmk.guess_locked")
                                  : t("kmk.predict", { nick: m.chooser });
      }
    }
    lastRound = m.round; lastStage = stage;
  }

  function renderFinal(m) {
    sub("final");
    stopBar();
    var b = A.podium("kmk-podium", m.board);
    if (b && b.length && b[0].pid === A.pid) { A.sfx("win"); A.vibe([30, 50, 30]); }
    else { A.sfx("start"); A.vibe(20); }
  }

  A.handlers.kmk = function (m) {
    route("kmk");
    if (A.view !== "kmk") return;
    switch (m.phase) {
      case "lobby": renderLobby(m); break;
      case "countdown": renderCount(m); break;
      case "play": renderPlay(m); break;
      case "final": renderFinal(m); break;
    }
  };

  $("kmk-ready").addEventListener("click", function () {
    A.sfx("buzz"); A.vibe(15);
    send({ t: "ready", ready: !myready });
  });
  $("kmk-go").addEventListener("click", function () {
    if (!valid(mine)) return;
    A.sfx("start"); A.vibe(20);
    send({ t: "assign", kiss: mine.indexOf(0), marry: mine.indexOf(1), kill: mine.indexOf(2) });
    canEdit = false;
    $("kmk-go").classList.add("hide");
  });
  $("kmk-again").addEventListener("click", function () {
    A.sfx("start"); A.vibe(20);
    send({ t: "again" });
  });
})();
