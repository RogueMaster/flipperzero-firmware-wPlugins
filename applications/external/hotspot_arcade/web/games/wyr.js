/* Would You Rather — a whole-group live A/B poll. The ESP is authoritative and
   self-organizing, driving {t:"wyr", phase, ...}: lobby (ready up) -> countdown
   -> vote (tap A or B, live tallies) -> reveal (final split) -> ... -> final.
   No scoring: it's about seeing what the group picks. We send ready/answer/again. */
(function () {
  var myready = false;

  function sub(name) {
    ["lobby", "count", "play", "final"].forEach(function (id) {
      $("wyr-" + id).classList.toggle("hide", id !== name);
    });
  }

  function renderLobby(m) {
    sub("lobby");
    myready = A.readyLobby({ players: m.players, listId: "wyr-players", readyId: "wyr-ready", meId: "wyr-me" });
  }

  function renderCount(m) {
    sub("count");
    A.countdown("wyr-count-num", m.sec);
  }

  var revealedFor = -1;
  function renderPlay(m) {
    sub("play");
    var reveal = m.phase === "reveal";
    $("wyr-meta").textContent = "Would you rather... (" + m.round + " / " + m.rounds + ")";
    var counts = m.counts || [0, 0];
    var total = counts[0] + counts[1];
    var mine = (typeof m.myvote === "number") ? m.myvote : -1;
    var texts = [m.a, m.b];
    var wrap = $("wyr-opts");
    wrap.innerHTML = "";
    var locked = reveal || mine >= 0;
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
        var tint = (counts[i] >= counts[1 - i] && total) ? "rgba(255,130,0,.20)" : "rgba(120,120,120,.16)";
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

  function renderFinal() {
    sub("final");
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
