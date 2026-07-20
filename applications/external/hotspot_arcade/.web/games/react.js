/* Reaction Duel (fastest finger) — whole-group reflex test. Server-driven
   {t:"react", phase, ...}: lobby -> countdown -> armed (red; wait for green, tap
   first to win; tapping while red disqualifies you for the round) -> reveal
   (winner + reaction time) -> ... -> final podium. We send ready / tap / again. */
(function () {
  var myready = false;
  var armedFor = -1, greenBuzzed = false;

  function sub(name) {
    ["lobby", "count", "arena", "final"].forEach(function (id) {
      $("rc-" + id).classList.toggle("hide", id !== name);
    });
  }

  function renderLobby(m) {
    sub("lobby");
    A.hideLead();
    myready = A.readyLobby({ players: m.players, listId: "rc-players", readyId: "rc-ready", meId: "rc-me" });
  }

  function renderCount(m) {
    sub("count");
    A.hideLead();
    A.countdown("rc-count-num", m.sec);
  }

  function renderArmed(m) {
    sub("arena");
    $("rc-meta").textContent = "Round " + m.round + " / " + m.rounds;
    var pad = $("rc-pad");
    var go = m.light === "go";
    var cls = "rc-pad " + (m.dq ? "dq" : go ? "go" : "wait");
    pad.className = cls;
    if (m.dq) pad.innerHTML = '<span class="rc-big">Too soon!</span><span class="rc-sub">Wait for the next round</span>';
    else if (m.tapped) pad.innerHTML = '<span class="rc-big">Tapped!</span>';
    else if (go) pad.innerHTML = '<span class="rc-big">TAP!</span>';
    else pad.innerHTML = '<span class="rc-big">Wait...</span><span class="rc-sub">Tap when it turns green</span>';
    if (go && !greenBuzzed && armedFor === m.round) { greenBuzzed = true; A.sfx("buzz"); A.vibe(30); }
    if (armedFor !== m.round) { armedFor = m.round; greenBuzzed = false; }
    A.showLead(m.scores || [], false);
  }

  function renderReveal(m) {
    sub("arena");
    A.showLead(m.scores || [], true);
    $("rc-meta").textContent = "Round " + m.round + " / " + m.rounds;
    var pad = $("rc-pad");
    pad.className = "rc-pad reveal";
    if (m.winner) {
      pad.innerHTML = '<span class="rc-big">' + (m.iwon ? "You won!" : esc(m.winner) + " won") + "</span>" +
                      '<span class="rc-sub">' + m.ms + " ms</span>";
      if (m.iwon) { A.sfx("win"); A.vibe([30, 50, 30]); } else A.sfx("tick");
    } else {
      pad.innerHTML = '<span class="rc-big">No winner</span>';
    }
    greenBuzzed = false; armedFor = -1;
  }

  function renderFinal(m) {
    sub("final");
    A.hideLead();
    var b = A.podium("rc-podium", m.scores);
    if (b.length && b[0].pid === A.pid) { A.sfx("win"); A.vibe([30, 50, 30]); }
    else A.sfx("lose");
  }

  // The reaction-duel game state is {t:"react",phase,...}; the floating-emoji
  // broadcast is a separate {t:"emoji"} handled in app.js, so no collision.
  A.handlers.react = function (m) {
    route("react");
    if (A.view !== "react") return;
    switch (m.phase) {
      case "lobby": renderLobby(m); break;
      case "countdown": renderCount(m); break;
      case "armed": renderArmed(m); break;
      case "reveal": renderReveal(m); break;
      case "final": renderFinal(m); break;
    }
  };

  $("rc-ready").addEventListener("click", function () {
    A.sfx("buzz"); A.vibe(15);
    send({ t: "ready", ready: !myready });
  });
  $("rc-again").addEventListener("click", function () {
    A.sfx("start"); A.vibe(20);
    send({ t: "again" });
  });
  $("rc-pad").addEventListener("click", function () {
    send({ t: "tap" });
  });
})();
