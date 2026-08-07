/* Fill the Blank — a judge picks the funniest answer (a genre homage to Cards Against
   Humanity; every card we ship is our own). Server-driven {t:"fillblank", phase, ...}:
   lobby (ready up + pack vote) -> countdown -> play (stage "play": everyone but the Czar
   taps one answer card from their hand, which then stays put marked as chosen while the
   rest grey out; stage "judge": the submissions come back shuffled and anonymous — with
   one extra card drawn by the deck itself mixed in — and only the Czar can tap one; stage
   "reveal": every card with the player who played it, the winner called out) -> ... ->
   final podium. We send ready/vote/play/pick/again.
   The engine sends no authorship at all before the Czar has picked. */
(function () {
  var myready = false;

  function sub(name) {
    ["lobby", "count", "play", "final"].forEach(function (id) {
      $("fb-" + id).classList.toggle("hide", id !== name);
    });
  }

  function stopBar() { A.timebarStop("fb-bar"); hide("fb-bar"); }

  function renderLobby(m) {
    sub("lobby");
    stopBar();
    myready = A.readyLobby({ players: m.players, listId: "fb-players", readyId: "fb-ready", meId: "fb-me" });
    A.packVote({
      boxId: "fb-topics",
      packs: m.packs, myvote: m.myvote,
      onVote: function (i) { send({ t: "vote", pack: i }); },
    });
    // The game needs a Czar plus two answers to judge between, so the lobby waits
    // below that instead of starting a round nobody can play.
    var have = (m.players || []).length;
    var need = m.min || 3;
    $("fb-need").textContent = have < need ? t("fb.need", { n: need }) : "";
  }

  function renderCount(m) {
    sub("count");
    stopBar();
    A.countdown("fb-count-num", m.sec);
  }

  function clear(boxId) {
    $(boxId).innerHTML = "";
    $(boxId).classList.add("hide");
  }

  /* Your hand. Index-stable: an empty slot arrives as "" and is skipped, so the index we
     send back in play{card} is the slot the engine dealt. Once you have played, the card
     stays where it is with the orange "chosen" treatment and every other card greys out —
     and they are really unselectable (disabled buttons, no listener), not just styled. */
  function renderHand(m) {
    var box = $("fb-hand");
    box.innerHTML = "";
    var cards = m.hand || [];
    var locked = m.mine >= 0;
    var any = false;
    for (var i = 0; i < cards.length; i++) {
      if (!cards[i]) continue;
      any = true;
      var b = document.createElement("button");
      b.type = "button";
      var cls = "fb-card hand";
      if (locked) cls += (i === m.mine) ? " mine" : " dim";
      b.className = cls;
      b.appendChild(el("span", "fb-txt", cards[i]));
      if (locked) {
        b.disabled = true;
        if (i === m.mine) b.appendChild(el("span", "fb-by", t("fb.your_card")));
      } else {
        b.addEventListener("click", (function (idx) {
          return function () { A.sfx("buzz"); A.vibe(15); send({ t: "play", card: idx }); };
        })(i));
      }
      box.appendChild(b);
    }
    box.classList.toggle("hide", !any);
  }

  function el(tag, cls, text) {
    var n = document.createElement(tag);
    n.className = cls;
    if (text != null) n.textContent = text;
    return n;
  }

  /* The judging pile. Before the pick this is bare shuffled text — the deck's own card is
     in there and looks exactly like a player's. On reveal each card gains the name of
     whoever played it (or the deck), and the winner is called out with its +1. */
  function renderPile(m, reveal) {
    var box = $("fb-subs");
    box.innerHTML = "";
    var cards = m.subs || [];
    if (!cards.length) { box.classList.add("hide"); return; }
    var authors = m.authors || [];
    var deckIdx = (typeof m.deckcard === "number") ? m.deckcard : -1;
    for (var i = 0; i < cards.length; i++) {
      var b = document.createElement("button");
      b.type = "button";
      var cls = "fb-card sub";
      if (reveal && i === m.pick) cls += " win";
      b.className = cls;
      b.appendChild(el("span", "fb-txt", cards[i]));
      if (reveal) {
        var isDeck = (i === deckIdx);
        var who = isDeck ? t("fb.deck_card") : (authors[i] || t("fb.someone"));
        var by = el("span", isDeck ? "fb-by deck" : "fb-by", who);
        // The winning card carries its +1 (nothing to award when the deck won).
        if (i === m.pick && !isDeck) by.appendChild(el("b", "fb-pts", "+1"));
        b.appendChild(by);
        b.disabled = true;
      } else if (m.iam) {
        b.addEventListener("click", (function (idx) {
          return function () { A.sfx("buzz"); A.vibe(15); send({ t: "pick", i: idx }); };
        })(i));
      } else {
        b.disabled = true;
      }
      box.appendChild(b);
    }
    box.classList.remove("hide");
  }

  var lastRound = -1, lastStage = "";
  function renderPlay(m) {
    sub("play");
    var stage = m.stage; // play | judge | reveal
    $("fb-meta").textContent = t("common.round", { n: m.round, total: m.rounds });
    $("fb-role").textContent = m.iam ? t("fb.you_czar") : t("fb.czar_is", { nick: m.czar });
    noteDeadline(m.deadline, m.dur);
    A.timebar("fb-bar", m.deadline, m.dur, false);
    $("fb-prompt").textContent = m.prompt || "";
    var note = $("fb-note");
    var tally = t("fb.tally", { n: m.played, total: m.total });

    if (stage === "play") {
      clear("fb-subs");
      if (m.iam) {
        clear("fb-hand");
        note.textContent = t("fb.czar_waits") + " " + tally;
      } else if (m.waiting) {
        // Joined mid-round: a hand is already dealt, but this round plays without us.
        clear("fb-hand");
        note.textContent = t("fb.next_round");
      } else {
        renderHand(m);
        note.textContent = (m.mine >= 0) ? t("fb.card_in") + " " + tally : t("fb.pick_card");
      }
    } else if (stage === "judge") {
      clear("fb-hand");
      renderPile(m, false);
      note.textContent = m.iam ? t("fb.you_judge") : t("fb.czar_judging", { nick: m.czar });
    } else { // reveal
      clear("fb-hand");
      renderPile(m, true);
      if (m.deckwon) {
        note.textContent = m.iam ? t("fb.czar_fooled") : t("fb.deck_won");
      } else if (!m.winner) {
        note.textContent = t("fb.no_winner");
      } else if (m.iam) {
        note.textContent = t("fb.czar_scored", { nick: m.winner });
      } else {
        note.textContent = m.mywin ? t("fb.you_won") : t("fb.won_round", { nick: m.winner });
      }
      if (lastRound !== m.round || lastStage !== "reveal") {
        var got = m.mygain > 0;
        A.sfx(got ? "correct" : "buzz");
        A.vibe(got ? 25 : 12);
      }
    }
    lastRound = m.round; lastStage = stage;
  }

  function renderFinal(m) {
    sub("final");
    stopBar();
    var b = A.podium("fb-podium", m.board);
    if (b && b.length && b[0].pid === A.pid) { A.sfx("win"); A.vibe([30, 50, 30]); }
    else { A.sfx("start"); A.vibe(20); }
  }

  A.handlers.fillblank = function (m) {
    route("fillblank");
    if (A.view !== "fillblank") return;
    switch (m.phase) {
      case "lobby": renderLobby(m); break;
      case "countdown": renderCount(m); break;
      case "play": renderPlay(m); break;
      case "final": renderFinal(m); break;
    }
  };

  $("fb-ready").addEventListener("click", function () {
    A.sfx("buzz"); A.vibe(15);
    send({ t: "ready", ready: !myready });
  });
  $("fb-again").addEventListener("click", function () {
    A.sfx("start"); A.vibe(20);
    send({ t: "again" });
  });
})();
