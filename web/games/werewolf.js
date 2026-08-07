/* Werewolf — hidden-role social deduction. The ESP is authoritative and holds
   every role; this module only draws what the server chose to tell THIS phone.
   {t:"werewolf", phase, ...}: lobby (ready up, needs 5) -> countdown -> play with
   a stage of roles / night / dawn / day / dusk, looping until one side wins ->
   final (roles revealed, a night-by-night log, podium). We send ready / kill /
   see / guard / accuse / again. The talking happens in the room, not on the
   phone — which is also why the wolves' running tally is rendered so plainly:
   sitting in the same room, they cannot say a word to each other. */
(function () {
  var myready = false;
  var WOLF = 2, SEER = 3, DOCTOR = 4;
  var ROLE_ICON = { 1: "🧑‍🌾", 2: "🐺", 3: "🔮", 4: "🛡️" };
  var ROLE_KEY = {
    1: "werewolf.role_villager", 2: "werewolf.role_wolf",
    3: "werewolf.role_seer", 4: "werewolf.role_doctor",
  };
  function roleName(r) { return ROLE_KEY[r] ? t(ROLE_KEY[r]) : ""; }

  function sub(name) {
    ["lobby", "count", "play", "final"].forEach(function (id) {
      $("ww-" + id).classList.toggle("hide", id !== name);
    });
  }
  function stopBar() { A.timebarStop("ww-bar"); hide("ww-bar"); }

  function renderLobby(m) {
    sub("lobby");
    stopBar();
    $("werewolf").classList.remove("ww-night");
    myready = A.readyLobby({ players: m.players, listId: "ww-players", readyId: "ww-ready", meId: "ww-me" });
    // The lobby must ALWAYS say why it is waiting. A silent lobby is
    // indistinguishable from a broken game -- which is exactly how a
    // below-quorum room got reported as "Werewolf does not start".
    var players = m.players || [];
    var n = players.length;
    var unready = players.filter(function (p) { return !p.ready; }).length;
    $("ww-need").textContent = !m.enough
      ? t("werewolf.need", { min: m.min, n: n })
      : unready ? t("werewolf.wait_ready", { n: unready, total: n })
        : t("werewolf.all_ready");
  }

  function renderCount(m) {
    sub("count");
    stopBar();
    A.countdown("ww-count-num", m.sec);
  }

  /* One row per player. pickable() decides which pids can be tapped and onPick()
     fires for them; counts is a pid -> tally badge, and blocked() greys a row out
     with a reason (the doctor's no-repeat rule). */
  function renderList(m, cfg) {
    cfg = cfg || {};
    var box = $("ww-list");
    box.innerHTML = "";
    (m.players || []).forEach(function (p) {
      var row = document.createElement("div");
      var cls = "ww-row";
      if (!p.alive || !p.in) cls += " out";
      if (p.pid === m.you) cls += " self";
      var block = cfg.blocked && cfg.blocked(p);
      var can = !block && cfg.pickable && cfg.pickable(p);
      if (can) cls += " tap";
      if (block) cls += " barred";
      if (cfg.chosen === p.pid) cls += " chosen";
      row.className = cls;
      // The role span is only ever filled from what the server sent: no role
      // field means this phone was not told, and nothing is inferred locally.
      var tag = p.role
        ? '<span class="ww-tag r' + p.role + '">' + ROLE_ICON[p.role] + " " + esc(roleName(p.role)) + "</span>"
        : (!p.in ? '<span class="ww-tag ghost">' + esc(t("werewolf.watching")) + "</span>" : "");
      if (block) tag = '<span class="ww-tag ghost">' + esc(t("werewolf.guard_repeat")) + "</span>" + tag;
      var n = cfg.counts && cfg.counts[p.pid];
      row.innerHTML =
        '<span class="ww-av">' + esc(p.avatar || "🙂") + "</span>" +
        '<span class="ww-name">' + esc(p.nick) + "</span>" +
        tag +
        (n ? '<span class="ww-count">' + n + "</span>" : "");
      if (can) row.addEventListener("click", function () {
        A.sfx("buzz"); A.vibe(15);
        cfg.onPick(p.pid);
      });
      box.appendChild(row);
    });
  }

  function tally(list) {
    var c = {};
    (list || []).forEach(function (v) { c[v.pid] = (c[v.pid] || 0) + 1; });
    return c;
  }
  function nickOfPid(m, pid) {
    var p = (m.players || []).find(function (x) { return x.pid === pid; });
    return p ? p.nick : "";
  }
  function roleOfPid(m, pid) {
    var p = (m.players || []).find(function (x) { return x.pid === pid; });
    return p && p.role ? roleName(p.role) : "";
  }

  var lastStage = "", lastDay = -1;
  function renderPlay(m) {
    sub("play");
    var stage = m.stage;
    var alive = !!m.alive, iam = m.myrole || 0;
    var night = (stage === "night" || stage === "roles" || stage === "dawn");
    $("werewolf").classList.toggle("ww-night", night);
    $("ww-meta").textContent = stage === "roles" ? t("werewolf.roles_title")
      : night ? t("werewolf.night", { n: m.day }) : t("werewolf.day", { n: m.day });
    $("ww-role").textContent = iam ? ROLE_ICON[iam] + " " + roleName(iam) : "";
    $("ww-counts").textContent = t("werewolf.counts", { v: m.villagersleft, w: m.wolvesleft });

    noteDeadline(m.deadline, m.dur);
    A.timebar("ww-bar", m.deadline, m.dur, false);

    var banner = $("ww-banner"), note = $("ww-note"), check = $("ww-check");
    var wait = $("ww-wait");
    check.classList.add("hide");
    wait.textContent = "";
    // The seer's reading, whenever the server saw fit to include it. Nobody else
    // ever receives this field, so there is nothing to hide client-side.
    if (m.check) {
      check.classList.remove("hide");
      check.className = "ww-check " + (m.check.wolf ? "bad" : "good");
      check.textContent = m.check.wolf
        ? t("werewolf.check_wolf", { nick: m.check.nick })
        : t("werewolf.check_clear", { nick: m.check.nick });
    }

    if (stage === "roles") {
      banner.textContent = t("werewolf.you_are", { role: roleName(iam) });
      note.textContent = iam === WOLF ? t("werewolf.pack_note")
        : iam === DOCTOR ? t("werewolf.doc_note")
          : iam === SEER ? t("werewolf.seer_note") : t("werewolf.roles_note");
      renderList(m, {});
    } else if (stage === "night") {
      banner.textContent = t("werewolf.night_falls");
      // Only ever "do YOU still owe an action" — never how many others do. That
      // count is a headcount of the surviving special roles.
      if (m.owe) wait.textContent = t("werewolf.owe");
      if (!iam) {
        note.textContent = t("werewolf.spectating");
        renderList(m, {});
      } else if (!alive) {
        note.textContent = t("werewolf.dead_note");
        renderList(m, { counts: tally(m.packvotes) });
      } else if (m.nokill && iam === WOLF) {
        note.textContent = t("werewolf.first_night");
        renderList(m, { counts: tally(m.packvotes) });
      } else if (iam === WOLF) {
        var picked = typeof m.mykill === "number" && m.mykill > 0;
        var votes = m.packvotes || [];
        note.textContent = picked
          ? t("werewolf.victim_picked", { nick: nickOfPid(m, m.mykill) })
          : t("werewolf.pick_victim");
        wait.textContent = t("werewolf.pack_wait", { n: votes.length, total: m.packsize });
        renderList(m, {
          pickable: function (p) { return p.in && p.alive && p.role !== WOLF; },
          counts: tally(votes), chosen: picked ? m.mykill : 0,
          onPick: function (pid) { send({ t: "kill", n: pid }); },
        });
      } else if (iam === SEER && !m.check) {
        note.textContent = t("werewolf.pick_check");
        renderList(m, {
          pickable: function (p) { return p.in && p.alive && p.pid !== m.you; },
          onPick: function (pid) { send({ t: "see", n: pid }); },
        });
      } else if (iam === DOCTOR && !m.myguard) {
        note.textContent = t("werewolf.pick_guard");
        renderList(m, {
          // Self-protection is allowed; repeating last night's target is not.
          pickable: function (p) { return p.in && p.alive; },
          blocked: function (p) { return p.pid === m.lastguard; },
          onPick: function (pid) { send({ t: "guard", n: pid }); },
        });
      } else if (iam === DOCTOR) {
        note.textContent = t("werewolf.guard_set", { nick: nickOfPid(m, m.myguard) });
        renderList(m, { chosen: m.myguard });
      } else {
        note.textContent = iam === SEER ? t("werewolf.checked") : t("werewolf.sleep");
        renderList(m, {});
      }
    } else if (stage === "dawn") {
      banner.textContent = t("werewolf.dawn");
      // Four distinct outcomes: a body, a blocked attack, an idle pack, or the
      // small-table opening night. Telling them apart is the doctor's whole point.
      note.textContent = m.victim
        ? t("werewolf.died", { nick: nickOfPid(m, m.victim), role: roleOfPid(m, m.victim) })
        : m.dawnkind === "saved" ? t("werewolf.saved")
          : m.dawnkind === "nokill" ? t("werewolf.first_dawn")
            : t("werewolf.quiet_night");
      renderList(m, {});
    } else if (stage === "day") {
      banner.textContent = t("werewolf.day_banner");
      var mine = (typeof m.myvote === "number" && m.myvote > 0) ? m.myvote : 0;
      note.textContent = !iam ? t("werewolf.spectating")
        : !alive ? t("werewolf.dead_note")
          : mine ? t("werewolf.voted", { nick: nickOfPid(m, mine) })
            : t("werewolf.vote_note");
      // The day tally is public, so the outstanding-vote count is safe to show.
      wait.textContent = t("werewolf.waiting", {
        n: m.waiting, total: m.voters, hammer: m.needed,
      });
      renderList(m, {
        pickable: function (p) { return iam && alive && p.in && p.alive && p.pid !== m.you; },
        counts: tally(m.votes), chosen: mine,
        onPick: function (pid) { send({ t: "accuse", n: pid }); },
      });
    } else { // dusk
      banner.textContent = t("werewolf.dusk");
      note.textContent = m.lynched
        ? t("werewolf.voted_out", { nick: nickOfPid(m, m.lynched), role: roleOfPid(m, m.lynched) })
        : t("werewolf.no_majority");
      renderList(m, {});
    }

    if (stage !== lastStage || m.day !== lastDay) {
      if (stage === "dawn" || stage === "dusk") { A.sfx("buzz"); A.vibe(25); }
      else if (stage === "night") { A.sfx("tick"); A.vibe(12); }
      lastStage = stage; lastDay = m.day;
    }
  }

  /* Night-by-night recap on the final screen: what each night did and who the day
     after it voted out. Half the fun of the game is the retelling. */
  function renderLog(m) {
    var box = $("ww-log");
    box.innerHTML = "";
    (m.log || []).forEach(function (d) {
      var night = document.createElement("div");
      night.className = "ww-logrow";
      night.textContent = d.victim
        ? t("werewolf.log_night_died", { n: d.day, nick: nickOfPid(m, d.victim), role: roleOfPid(m, d.victim) })
        : t("werewolf.log_night_safe", { n: d.day });
      box.appendChild(night);
      var day = document.createElement("div");
      day.className = "ww-logrow day";
      day.textContent = d.lynched
        ? t("werewolf.log_day_out", { n: d.day, nick: nickOfPid(m, d.lynched), role: roleOfPid(m, d.lynched) })
        : t("werewolf.log_day_none", { n: d.day });
      box.appendChild(day);
    });
  }

  function renderFinal(m) {
    sub("final");
    stopBar();
    $("werewolf").classList.remove("ww-night");
    var wolves = m.winner === "wolves";
    $("ww-winner").textContent = wolves ? t("werewolf.wolves_win") : t("werewolf.village_wins");
    $("ww-winner").className = "ww-winner " + (wolves ? "bad" : "good");
    var box = $("ww-roles");
    box.innerHTML = "";
    (m.players || []).forEach(function (p) {
      if (!p.in) return;
      var row = document.createElement("div");
      row.className = "ww-row" + (p.alive ? "" : " out") + (p.pid === m.you ? " self" : "");
      row.innerHTML =
        '<span class="ww-av">' + esc(p.avatar || "🙂") + "</span>" +
        '<span class="ww-name">' + esc(p.nick) + "</span>" +
        '<span class="ww-tag r' + p.role + '">' + ROLE_ICON[p.role] + " " + esc(roleName(p.role)) + "</span>";
      box.appendChild(row);
    });
    renderLog(m);
    A.podium("ww-podium", m.board);
    var iWon = (m.myrole === WOLF) === wolves;
    if (iWon) { A.sfx("win"); A.vibe([30, 50, 30]); } else { A.sfx("start"); A.vibe(20); }
    lastStage = ""; lastDay = -1;
  }

  A.handlers.werewolf = function (m) {
    route("werewolf");
    if (A.view !== "werewolf") return;
    switch (m.phase) {
      case "lobby": renderLobby(m); break;
      case "countdown": renderCount(m); break;
      case "play": renderPlay(m); break;
      case "final": renderFinal(m); break;
    }
  };

  $("ww-ready").addEventListener("click", function () {
    A.sfx("buzz"); A.vibe(15);
    send({ t: "ready", ready: !myready });
  });
  $("ww-again").addEventListener("click", function () {
    A.sfx("start"); A.vibe(20);
    send({ t: "again" });
  });
})();
