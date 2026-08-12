/* Would You Rather — a whole-group live A/B poll. The ESP is authoritative and
   self-organizing, driving {t:"wyr", phase, ...}: lobby (ready up) -> countdown
   -> vote (tap A or B, live tallies) -> reveal (final split) -> ... -> final.
   No scoring: it's about seeing what the group picks. We send ready/answer/again.
   The final phase carries the whole game's A/B history, which we turn into an
   "how much did we agree" distribution chart. */
(function () {
  var myready = false;

  function sub(name) {
    ["lobby", "count", "play", "final"].forEach(function (id) {
      $("wyr-" + id).classList.toggle("hide", id !== name);
    });
  }

  function stopBar() { A.timebarStop("wyr-bar"); hide("wyr-bar"); }

  function renderLobby(m) {
    sub("lobby");
    stopBar();
    myready = A.readyLobby({ players: m.players, listId: "wyr-players", readyId: "wyr-ready", meId: "wyr-me" });
    A.packVote({
      boxId: "wyr-topics",
      packs: m.packs, myvote: m.myvote,
      onVote: function (i) { send({ t: "vote", pack: i }); },
    });
  }

  function renderCount(m) {
    sub("count");
    stopBar();
    A.countdown("wyr-count-num", m.sec);
  }

  var revealedFor = -1;
  function renderPlay(m) {
    sub("play");
    var reveal = m.phase === "reveal";
    $("wyr-meta").textContent = t("wyr.meta", { n: m.round, total: m.rounds });
    // Countdown bar: the vote window while asking, the pause before the next prompt
    // while revealing. Both carry deadline+dur, so the shared timebar drives both.
    noteDeadline(m.deadline, m.dur);
    A.timebar("wyr-bar", m.deadline, m.dur, true);
    var counts = m.counts || [0, 0];
    var total = counts[0] + counts[1];
    var mine = (typeof m.myvote === "number") ? m.myvote : -1;
    var texts = [m.a, m.b];
    var wrap = $("wyr-opts");
    wrap.innerHTML = "";
    var locked = reveal || mine >= 0;
    // Reflect THIS prompt's lock state on the container. Rebuilding innerHTML clears
    // the children but not the wrap's own class, so without this a "locked" set on an
    // earlier vote sticks forever and every later prompt silently ignores taps.
    wrap.classList.toggle("locked", locked);
    [0, 1].forEach(function (i) {
      var pct = total ? Math.round((counts[i] / total) * 100) : 0;
      // A div (not <button>): form-control flex sizing mishandles the absolute
      // fill child and collapses the row height, so we use a role="button" div.
      var b = document.createElement("div");
      var cls = "wyr-opt";
      if (i === mine) cls += " mine";
      if (locked) cls += " locked";
      if (reveal && counts[i] >= counts[1 - i] && total) cls += " lead";
      b.className = cls;
      b.setAttribute("role", "button");
      b.innerHTML =
        '<span class="wtxt">' + esc(texts[i] || "") + "</span>" +
        '<span class="wpct">' + (reveal ? pct + "%" : (mine === i ? "✓" : "")) + "</span>";
      // On reveal, paint the vote share as a background gradient (no extra DOM,
      // so nothing perturbs the row height). The leading option tints orange.
      if (reveal) {
        var tint = (counts[i] >= counts[1 - i] && total) ? "rgba(255,130,0,.32)" : "rgba(255,255,255,.20)";
        b.style.background = "linear-gradient(90deg," + tint + " " + pct + "%,var(--surface-2) " + pct + "%)";
      }
      if (!reveal && mine < 0) b.addEventListener("click", function () {
        if (wrap.classList.contains("locked")) return;
        A.sfx("buzz"); A.vibe(15);
        send({ t: "answer", c: i });
        wrap.classList.add("locked");
        b.classList.add("mine");
      });
      wrap.appendChild(b);
    });
    $("wyr-tally").textContent = total + (total === 1 ? " vote" : " votes");
    if (reveal && revealedFor !== m.round) { revealedFor = m.round; A.sfx("correct"); A.vibe(20); }
    if (!reveal) revealedFor = -1;
  }

  /* ---- final agreement chart -------------------------------------------------
     A round's agreement is the MAJORITY share, max(a,b)/(a+b): a 1/9 split reads
     as 90% agreement exactly like 9/1, so the value never drops below 50%. Rounds
     nobody voted in carry no agreement at all and are skipped (counting them as
     100% would invent unanimity out of an empty room).

     The axis follows the number of voters rather than a fixed set of percentages:
     with n voters the only reachable values are ceil(n/2)/n … n/n — for 10 players
     that is 50/60/70/80/90/100, for 5 players 60/80/100. Players can join or leave
     mid-game, so a round's turnout need not match the final count; we keep ONE axis
     (from the final player count, which is what the group sees in front of them)
     and drop each round into the bucket whose percentage is nearest. A per-round
     axis would make the columns mean different things from bar to bar. */

  function agreementPcts(rounds) {
    var out = [];
    for (var i = 0; i < (rounds || []).length; i++) {
      var a = rounds[i].a || 0, b = rounds[i].b || 0;
      if (a + b === 0) continue; // nobody voted: nothing to agree or disagree about
      out.push((Math.max(a, b) / (a + b)) * 100);
    }
    return out;
  }

  // The reachable agreement percentages for n voters, low to high.
  function axisFor(n) {
    var ticks = [];
    for (var k = Math.ceil(n / 2); k <= n; k++) ticks.push(Math.round((k / n) * 100));
    return ticks;
  }

  function verdictKey(mean) {
    if (mean >= 90) return "wyr.verdict_high";
    if (mean >= 75) return "wyr.verdict_mid";
    if (mean >= 60) return "wyr.verdict_low";
    return "wyr.verdict_split";
  }

  function renderChart(m) {
    var box = $("wyr-chart");
    var pcts = agreementPcts(m && m.rounds);
    if (!pcts.length) { box.classList.add("hide"); return; } // nothing played yet
    box.classList.remove("hide");
    var n = (m.voters > 1) ? m.voters : 2;
    var ticks = axisFor(n);
    var counts = [], i;
    for (i = 0; i < ticks.length; i++) counts.push(0);
    for (i = 0; i < pcts.length; i++) {
      var best = 0;
      for (var j = 1; j < ticks.length; j++)
        if (Math.abs(ticks[j] - pcts[i]) < Math.abs(ticks[best] - pcts[i])) best = j;
      counts[best]++;
    }
    var max = 0;
    for (i = 0; i < counts.length; i++) if (counts[i] > max) max = counts[i];

    var wrap = $("wyr-buckets");
    wrap.innerHTML = "";
    for (i = 0; i < ticks.length; i++) {
      var col = document.createElement("div");
      col.className = "wyr-bkt" + (counts[i] ? " on" : "");
      col.innerHTML =
        '<span class="wyr-bn">' + counts[i] + "</span>" +
        '<div class="wyr-bwrap">' +
        (counts[i] ? '<i style="height:' + Math.round((counts[i] / max) * 100) + '%"></i>' : "") +
        "</div>" +
        '<span class="wyr-bx">' + ticks[i] + "%</span>";
      wrap.appendChild(col);
    }

    var mean = 0;
    for (i = 0; i < pcts.length; i++) mean += pcts[i];
    mean /= pcts.length;

    // The mean lies about BIMODAL data: half the rounds unanimous and half a dead split
    // average to a 75% nobody actually felt (and with few voters those two extremes are the
    // ONLY reachable values). When real mass sits at BOTH ends, hide the mean marker and
    // name the shape instead of a meaningless middle.
    var lo = ticks[0], hi = ticks[ticks.length - 1], atLo = 0, atHi = 0;
    for (i = 0; i < pcts.length; i++) {
      if (Math.round(pcts[i]) <= lo + 1) atLo++;
      else if (Math.round(pcts[i]) >= hi - 1) atHi++;
    }
    var line = $("wyr-mean");
    if (pcts.length >= 4 && atLo >= pcts.length / 3 && atHi >= pcts.length / 3) {
      line.classList.add("hide");
      $("wyr-verdict").textContent = t("wyr.verdict_polarized", { hi: atHi, lo: atLo });
      return;
    }

    // Otherwise draw the mean at its true horizontal position, interpolated between the two
    // bucket centres it falls between, so 62% sits just right of the 60% column.
    var slot = 0;
    if (mean >= ticks[ticks.length - 1]) slot = ticks.length - 1;
    else if (mean > ticks[0]) {
      for (i = 0; i < ticks.length - 1; i++)
        if (mean >= ticks[i] && mean <= ticks[i + 1]) {
          slot = i + (mean - ticks[i]) / (ticks[i + 1] - ticks[i]);
          break;
        }
    }
    line.classList.remove("hide");
    line.style.left = (((slot + 0.5) / ticks.length) * 100).toFixed(2) + "%";
    $("wyr-mean-val").textContent = Math.round(mean) + "%";
    $("wyr-verdict").textContent = t(verdictKey(mean), { p: Math.round(mean) });
  }

  function renderFinal(m) {
    sub("final");
    stopBar();
    renderChart(m);
    A.sfx("win"); A.vibe([20, 40, 20]);
  }

  A.handlers.wyr = function (m) {
    route("wyr");
    if (A.view !== "wyr") return;
    switch (m.phase) {
      case "lobby": renderLobby(m); break;
      case "countdown": renderCount(m); break;
      case "vote": case "reveal": renderPlay(m); break;
      case "final": renderFinal(m); break;
    }
  };

  $("wyr-ready").addEventListener("click", function () {
    A.sfx("buzz"); A.vibe(15);
    send({ t: "ready", ready: !myready });
  });
  $("wyr-again").addEventListener("click", function () {
    A.sfx("start"); A.vibe(20);
    send({ t: "again" });
  });
})();
