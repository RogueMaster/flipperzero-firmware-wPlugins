/* Spyfall — everyone at the table shares a location and holds a role there, except
   one player: the spy, who is told neither. The ESP is authoritative and drives
   {t:"spyfall", phase, ...}: lobby (ready + pack vote) -> countdown -> play -> final.

   A playing round has four stages, and the round is driven by PRESSES, not by a clock:
     card     - your card is on screen; tap "Got it". The six minutes only start once
                everyone has, so nobody is still reading when the questioning begins.
     talk     - the card is hidden. "Show my card" reveals it only while HELD down;
                "I know the location" (spy only) and "I know the spy" (everyone, the spy
                included) end the round at any moment.
     nominate - the clock ran out: "Time's up. Stop discussing!", then a round-robin
                where each seat nominates once and the table has to agree.
     reveal   - location, spy, roles, and who missed.
   We send ready / vote / seen / solve / accuse / nominate / agree / again.

   This module never learns anything it isn't sent: the location simply isn't in the
   spy's payload until the reveal, and a role only ever arrives for its own holder. */
(function () {
  var myready = false;
  var held = false;        // "Show my card" is currently held down
  var holdTimer = null;
  var picker = "";         // "" | "loc" | "spy" | "nom" -- which list is open
  var armed = -1;          // index/pid tapped once; a second tap commits
  var last = null;         // latest play message, for re-rendering on a local tap
  var ticker = null;
  var HOLD_MAX = 15000;    // hard stop, in case an "up" event never arrives

  function sub(name) {
    ["lobby", "count", "play", "final"].forEach(function (id) {
      $("sf-" + id).classList.toggle("hide", id !== name);
    });
  }
  function stopBar() { A.timebarStop("sf-bar"); hide("sf-bar"); }
  function stopClock() {
    if (ticker) { clearInterval(ticker); ticker = null; }
    $("sf-clock").textContent = "";
  }
  // A readable mm:ss next to the bar: six minutes of talking is far too long to judge
  // from a shrinking bar alone.
  function startClock(deadline) {
    stopClock();
    function paint() {
      var left = Math.max(0, Math.round((deadline - (Date.now() + A.offset)) / 1000));
      var m = Math.floor(left / 60), s = left % 60;
      $("sf-clock").textContent = m + ":" + (s < 10 ? "0" : "") + s;
      $("sf-clock").classList.toggle("hot", left <= 30);
    }
    paint();
    ticker = setInterval(paint, 500);
  }

  function renderLobby(m) {
    sub("lobby");
    stopBar(); stopClock(); holdOff();
    myready = A.readyLobby({ players: m.players, listId: "sf-players", readyId: "sf-ready", meId: "sf-me" });
    var short = (m.players || []).length < m.need;
    $("sf-need").textContent = short ? t("sf.need", { n: m.need }) : "";
    $("sf-need").classList.toggle("hide", !short);
    A.packVote({
      boxId: "sf-topics",
      packs: m.packs, myvote: m.myvote,
      onVote: function (i) { send({ t: "vote", pack: i }); },
    });
  }

  function renderCount(m) {
    sub("count");
    stopBar(); stopClock(); holdOff();
    A.countdown("sf-count-num", m.sec);
  }

  /* ---- the card ---------------------------------------------------------------
     Held-down reveal. Touch is the awkward case, so every way a press can end is
     wired up: pointerup/pointercancel/pointerleave, touchend/touchcancel, the mouse
     pair for desktops without Pointer Events, plus window blur and a
     visibilitychange (switching apps mid-press must not leave the card up). A
     timeout backstops all of it. touchstart/pointerdown are preventDefault()ed so
     iOS doesn't turn the long press into a text-selection callout. */
  function cardHtml(m) {
    var kicker, big, sub2, kind = "";
    if (m.stage === "reveal") {
      kind = m.mygain > 0 ? "good" : "bad";
      kicker = t("sf.location");
      big = m.loc;
      sub2 = t("sf.spy_was", { nick: m.spyNick });
    } else if (!m.me) {
      kicker = t("sf.waiting_kicker");
      big = t("sf.waiting");
      sub2 = "";
    } else if (m.spy) {
      kind = "spy";
      kicker = t("sf.you_are");
      big = t("sf.the_spy");
      sub2 = t("sf.spy_blurb");
    } else {
      kicker = t("sf.location");
      big = m.loc;
      sub2 = t("sf.your_role", { role: m.role });
    }
    var c = $("sf-card");
    c.className = "sf-card" + (kind ? " " + kind : "");
    c.innerHTML =
      '<span class="sf-kicker">' + esc(kicker) + "</span>" +
      '<span class="sf-big">' + esc(big) + "</span>" +
      (sub2 ? '<span class="sf-sub">' + esc(sub2) + "</span>" : "");
  }
  function showCard(on) { $("sf-card").classList.toggle("hide", !on); }

  function holdOn(e) {
    if (e && e.cancelable) e.preventDefault();
    if (held) return;
    held = true;
    showCard(true);
    A.vibe(8);
    if (holdTimer) clearTimeout(holdTimer);
    holdTimer = setTimeout(holdOff, HOLD_MAX);
  }
  function holdOff() {
    if (holdTimer) { clearTimeout(holdTimer); holdTimer = null; }
    if (!held) return;
    held = false;
    // Only the stages that hide the card by default need it put back.
    if (last && last.stage !== "card" && last.stage !== "reveal") showCard(false);
  }

  /* ---- pickers: one tap arms an entry, a second commits. Every one of these costs
     something (your single accusation, or the round itself), so none of them fires
     on a stray tap. ---------------------------------------------------------------*/
  function row(cls, left, right) {
    var d = document.createElement("div");
    d.className = "sf-row" + (cls ? " " + cls : "");
    d.innerHTML = '<span class="sf-rl">' + esc(left) + "</span>" +
                  '<span class="sf-rr">' + esc(right) + "</span>";
    return d;
  }
  function pickRow(box, key, label, onCommit) {
    var r = row("tap" + (armed === key ? " armed" : ""), label,
                armed === key ? t("sf.confirm") : "");
    r.addEventListener("click", function () {
      if (armed === key) { A.sfx("start"); A.vibe(30); armed = -1; onCommit(); return; }
      armed = key;
      A.sfx("buzz"); A.vibe(12);
      renderPlay(last);
    });
    box.appendChild(r);
  }
  function others(m) {
    return (m.cands || []).filter(function (p) { return p.pid !== A.pid; });
  }

  function renderMisses(m) {
    (m.misses || []).forEach(function (x) {
      $("sf-list").appendChild(row("miss", t("sf.miss", { by: x.by, of: x.of }), "✗"));
    });
  }

  function renderTalk(m) {
    show("sf-hold");
    var box = $("sf-list");
    // Both pickers are gated on the state the server actually reports, not just on the
    // local flag: a push that arrives while one is open (someone else's miss, or your
    // own accusation coming back as `spent`) must not leave a dead list on screen.
    if (picker === "loc" && m.spy && m.locs) {
      $("sf-listhead").textContent = t("sf.pick_loc");
      show("sf-listhead");
      (m.locs || []).forEach(function (name, i) {
        pickRow(box, i, name, function () { send({ t: "solve", loc: i }); });
      });
      $("sf-note").textContent = t("sf.pick_loc_hint");
      return;
    }
    if (picker === "spy" && !m.spent) {
      $("sf-listhead").textContent = t("sf.pick_spy");
      show("sf-listhead");
      others(m).forEach(function (p) {
        pickRow(box, p.pid, (p.avatar || "🙂") + " " + p.nick, function () {
          send({ t: "accuse", pid: p.pid });
        });
      });
      $("sf-note").textContent = t("sf.pick_spy_hint");
      return;
    }
    show("sf-acts");
    if (m.spy) show("sf-loc");
    show("sf-who");
    // On the spy's phone the button says out loud what pressing it is for: cover.
    $("sf-who").textContent = m.spy ? t("sf.know_spy_spy") : t("sf.know_spy");
    $("sf-who").disabled = !!m.spent;
    $("sf-who").classList.toggle("spent", !!m.spent);
    renderMisses(m);
    $("sf-note").textContent = m.spent ? t("sf.spent") : t("sf.hold_hint");
  }

  function renderNominate(m) {
    show("sf-hold");
    if (m.nomStage === "hush") {
      $("sf-note").textContent = t("sf.hush_note");
      return;
    }
    if (m.nomStage === "poll") {
      if (m.myagree < 0) show("sf-poll");
      $("sf-note").textContent = m.myagree < 0
        ? t("sf.poll_count", { n: m.agreed, need: m.need })
        : t("sf.poll_wait", { n: m.agreed, need: m.need });
      return;
    }
    // "pick"
    if (m.nomMe) {
      $("sf-listhead").textContent = t("sf.pick_spy");
      show("sf-listhead");
      var box = $("sf-list");
      others(m).forEach(function (p) {
        pickRow(box, p.pid, (p.avatar || "🙂") + " " + p.nick, function () {
          send({ t: "nominate", pid: p.pid });
        });
      });
    }
    $("sf-note").textContent = m.nomMe ? t("sf.confirm_hint") : t("sf.nom_wait_note");
  }

  function renderReveal(m) {
    $("sf-listhead").textContent = t("sf.reveal_roles");
    show("sf-listhead");
    var box = $("sf-list");
    (m.roles || []).forEach(function (r) {
      box.appendChild(row(r.spy ? "spy" : "",
        r.nick + " · " + (r.spy ? t("sf.the_spy") : r.role), ""));
    });
    renderMisses(m);
    $("sf-note").textContent = m.mygain ? t("sf.gain", { g: m.mygain }) : t("common.zero_round");
  }

  function headline(m) {
    if (m.stage === "reveal") {
      return m.outcome === "caught" ? t("sf.out_caught")
        : m.outcome === "solved" ? t("sf.out_solved")
          : m.outcome === "failed" ? t("sf.out_failed", { loc: m.called || "?" })
            : m.outcome === "aborted" ? t("sf.out_aborted")
              : m.blamedNick ? t("sf.out_condemned", { nick: m.blamedNick })
                : t("sf.out_novote");
    }
    if (!m.me) return t("sf.waiting_note");
    if (m.stage === "card") return t("sf.card_hint");
    if (m.stage === "talk") return t("sf.ask");
    if (m.nomStage === "hush") return t("sf.hush");
    if (m.nomStage === "poll") return t("sf.poll_q", { by: m.nominatorNick, of: m.nomineeNick });
    return m.nomMe ? t("sf.nom_you") : t("sf.nom_wait", { nick: m.nominatorNick });
  }

  var lastRound = -1, lastStage = "", lastNom = "";
  function renderPlay(m) {
    sub("play");
    var fresh = lastRound !== m.round || lastStage !== m.stage ||
                lastNom !== (m.nomStage || "");
    if (fresh) { picker = ""; armed = -1; }
    last = m;

    $("sf-meta").textContent = t("common.round", { n: m.round, total: m.rounds });
    noteDeadline(m.deadline, m.dur);
    A.timebar("sf-bar", m.deadline, m.dur, false);
    if (fresh) startClock(m.deadline);

    hide("sf-ok"); hide("sf-hold"); hide("sf-acts"); hide("sf-poll");
    hide("sf-loc"); hide("sf-who"); hide("sf-listhead");
    $("sf-list").innerHTML = "";
    $("sf-listhead").textContent = "";
    $("sf-note").textContent = "";
    cardHtml(m);
    $("sf-head").textContent = headline(m);

    if (m.stage === "reveal") {
      holdOff();
      showCard(true); // the reveal is public: leave it up
      renderReveal(m);
      if (fresh) { A.sfx(m.mygain ? "correct" : "buzz"); A.vibe(m.mygain ? 25 : 12); }
    } else if (!m.me) {
      // Joined mid-round: no location, no role, nothing to give away.
      showCard(true);
    } else if (m.stage === "card") {
      showCard(true);
      if (!m.myseen) show("sf-ok");
      else $("sf-note").textContent = t("sf.card_wait", { n: m.seen, total: m.total });
    } else {
      showCard(held);
      if (m.stage === "talk") renderTalk(m);
      else renderNominate(m);
    }
    lastRound = m.round; lastStage = m.stage; lastNom = m.nomStage || "";
  }

  function renderFinal(m) {
    sub("final");
    stopBar(); stopClock(); holdOff();
    var b = A.podium("sf-podium", m.board);
    if (b && b.length && b[0].pid === A.pid) { A.sfx("win"); A.vibe([30, 50, 30]); }
    else { A.sfx("start"); A.vibe(20); }
  }

  A.handlers.spyfall = function (m) {
    route("spyfall");
    if (A.view !== "spyfall") return;
    switch (m.phase) {
      case "lobby": renderLobby(m); break;
      case "countdown": renderCount(m); break;
      case "play": renderPlay(m); break;
      case "final": renderFinal(m); break;
    }
  };

  $("sf-ready").addEventListener("click", function () {
    A.sfx("buzz"); A.vibe(15);
    send({ t: "ready", ready: !myready });
  });
  $("sf-again").addEventListener("click", function () {
    A.sfx("start"); A.vibe(20);
    send({ t: "again" });
  });
  $("sf-ok").addEventListener("click", function () {
    A.sfx("buzz"); A.vibe(15);
    send({ t: "seen" });
  });
  $("sf-loc").addEventListener("click", function () {
    A.sfx("buzz"); A.vibe(12);
    picker = "loc"; armed = -1; renderPlay(last);
  });
  $("sf-who").addEventListener("click", function () {
    if (this.disabled) return;
    A.sfx("buzz"); A.vibe(12);
    picker = "spy"; armed = -1; renderPlay(last);
  });
  $("sf-in").addEventListener("click", function () {
    A.sfx("start"); A.vibe(20);
    send({ t: "agree", in: true });
  });
  $("sf-no").addEventListener("click", function () {
    A.sfx("buzz"); A.vibe(12);
    send({ t: "agree", in: false });
  });

  var hb = $("sf-hold");
  hb.addEventListener("pointerdown", holdOn);
  hb.addEventListener("touchstart", holdOn, { passive: false });
  hb.addEventListener("mousedown", holdOn);
  ["pointerup", "pointercancel", "pointerleave", "touchend", "touchcancel",
   "mouseup", "mouseleave"].forEach(function (ev) {
    hb.addEventListener(ev, holdOff);
  });
  hb.addEventListener("contextmenu", function (e) { e.preventDefault(); });
  window.addEventListener("blur", holdOff);
  document.addEventListener("visibilitychange", holdOff);
})();
