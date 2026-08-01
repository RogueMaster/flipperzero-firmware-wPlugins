/* Guess the Color — whole-group. Server shows a random swatch; you dial R/G/B
   (0-255) with hold-to-repeat arrows and submit. Closest guess wins the round,
   with a speed bonus on top. Server-driven {t:"gc", phase, ...}:
   lobby -> countdown -> play -> reveal -> ... -> final podium. We send
   ready / guess / again. */
(function () {
  var myready = false;
  var gr = 128, gg = 128, gb = 128; // local guess, adjusted with the arrows
  var round = -1, submitted = false;

  function sub(name) {
    ["lobby", "count", "play", "reveal", "final"].forEach(function (id) {
      $("gc-" + id).classList.toggle("hide", id !== name);
    });
  }

  function clamp(v) { return v < 0 ? 0 : v > 255 ? 255 : v; }

  function get(ch) { return ch === "r" ? gr : ch === "g" ? gg : gb; }
  function set(ch, v) { v = clamp(v); if (ch === "r") gr = v; else if (ch === "g") gg = v; else gb = v; }

  // Only the numbers (and slider thumbs) move while guessing — we deliberately do
  // NOT preview the guessed color (that would turn the game into "drag until it
  // matches"). The player's color is revealed after the round so they see how far off.
  function paintGuess() {
    ["r", "g", "b"].forEach(function (ch) {
      var v = get(ch);
      $("gc-" + ch + "-val").textContent = v;
      var s = $("gc-" + ch + "-slider");
      if (+s.value !== v) s.value = v;
    });
  }

  function lock(on) {
    submitted = on;
    $("gc-submit").classList.toggle("hide", on);
    $("gc-wait").classList.toggle("hide", !on);
    ["gc-r-up", "gc-r-dn", "gc-g-up", "gc-g-dn", "gc-b-up", "gc-b-dn",
     "gc-r-slider", "gc-g-slider", "gc-b-slider"].forEach(function (id) {
      $(id).disabled = on;
    });
  }

  function renderLobby(m) {
    sub("lobby");
    A.hideLead();
    myready = A.readyLobby({ players: m.players, listId: "gc-players", readyId: "gc-ready", meId: "gc-me" });
  }

  function renderCount(m) {
    sub("count");
    A.hideLead();
    A.countdown("gc-count-num", m.sec);
  }

  function renderPlay(m) {
    sub("play");
    $("gc-meta").textContent = "Round " + m.round + " / " + m.rounds;
    $("gc-target").style.background = m.color;
    if (m.round !== round) {           // new round: reset the guess and unlock
      round = m.round;
      gr = gg = gb = 128;
      paintGuess();
      lock(false);
      A.sfx("start");
    }
    if (m.submitted && !submitted) lock(true);
    A.showLead(m.scores || [], false);
  }

  function renderReveal(m) {
    sub("reveal");
    $("gc-rmeta").textContent = "Round " + m.round + " / " + m.rounds;
    $("gc-rtarget").style.background = m.color; // the answer color, shown big up top
    $("gc-answer").textContent = "Answer  " + m.r + ", " + m.g + ", " + m.b;
    // One row per player who guessed, closest (highest points) first. Each row's
    // swatch is split down the middle: left half is the answer, right half is that
    // player's guess, so how well the two halves match reads at a glance.
    var gs = (m.guesses || []).slice().sort(function (a, b) {
      return b.points - a.points || a.dist - b.dist;
    });
    var list = $("gc-guesses");
    list.innerHTML = "";
    gs.forEach(function (g) {
      var row = document.createElement("div");
      row.className = "gc-grow" +
        (g.pid === A.pid ? " you" : "") +
        (g.pid === m.winnerPid ? " win" : "");
      row.innerHTML =
        '<span class="gc-gsw">' +
          '<span class="gc-h" style="background:' + esc(m.color) + '"></span>' +
          '<span class="gc-h" style="background:' + esc(g.color) + '"></span>' +
        "</span>" +
        '<span class="gc-gn">' + esc(g.nick) + "</span>" +
        '<span class="gc-gd">off ' + g.dist + "</span>" +
        '<span class="gc-gp">' + g.points + "/10</span>";
      list.appendChild(row);
    });
    if (!gs.length) list.innerHTML = '<div class="gc-grow"><span class="gc-gn">No guesses</span></div>';
    if (m.winner) {
      $("gc-winner").textContent = m.iwon ? "You were closest!" : esc(m.winner) + " was closest";
      if (m.iwon) { A.sfx("win"); A.vibe([30, 50, 30]); } else A.sfx("tick");
    } else {
      $("gc-winner").textContent = "";
    }
    round = -1; // force a reset when the next play round arrives
    A.showLead(m.scores || [], true);
  }

  function renderFinal(m) {
    sub("final");
    A.hideLead();
    var b = A.podium("gc-podium", m.board); // final JSON carries the scoreboard as `board`
    if (b.length && b[0].pid === A.pid) { A.sfx("win"); A.vibe([30, 50, 30]); }
    else A.sfx("lose");
    round = -1;
  }

  A.handlers.gc = function (m) {
    route("gc");
    if (A.view !== "gc") return;
    switch (m.phase) {
      case "lobby": renderLobby(m); break;
      case "countdown": renderCount(m); break;
      case "play": renderPlay(m); break;
      case "reveal": renderReveal(m); break;
      case "final": renderFinal(m); break;
    }
  };

  // Hold-to-repeat stepper: one step on press, then a repeating step that
  // accelerates the longer the button is held (slow taps for fine control,
  // hold to run to an extreme fast). Mirrors pong.js bindHold, but local-only.
  function bindStep(el, apply) {
    var timer = 0, delay = 0;
    function step() {
      apply();
      delay = Math.max(28, delay - 24);
      timer = setTimeout(step, delay);
    }
    function down(e) {
      e.preventDefault();
      if (submitted) return;
      apply();
      delay = 200;
      timer = setTimeout(step, delay);
    }
    function up(e) { if (e) e.preventDefault(); if (timer) { clearTimeout(timer); timer = 0; } }
    el.addEventListener("pointerdown", down);
    el.addEventListener("pointerup", up);
    el.addEventListener("pointerleave", up);
    el.addEventListener("pointercancel", up);
  }

  document.addEventListener("DOMContentLoaded", function () {
    ["r", "g", "b"].forEach(function (ch) {
      // Drag the slider to get close fast; the -/+ buttons nudge by 1 (hold to run).
      bindStep($("gc-" + ch + "-up"), function () { set(ch, get(ch) + 1); paintGuess(); A.vibe(4); });
      bindStep($("gc-" + ch + "-dn"), function () { set(ch, get(ch) - 1); paintGuess(); A.vibe(4); });
      $("gc-" + ch + "-slider").addEventListener("input", function (e) {
        if (submitted) return;
        set(ch, +e.target.value);
        paintGuess();
      });
    });
    $("gc-ready").addEventListener("click", function () {
      A.sfx("buzz"); A.vibe(15);
      send({ t: "ready", ready: !myready });
    });
    $("gc-submit").addEventListener("click", function () {
      if (submitted) return;
      lock(true);
      A.sfx("score"); A.vibe(20);
      send({ t: "guess", r: gr, g: gg, b: gb });
    });
    $("gc-again").addEventListener("click", function () {
      A.sfx("start"); A.vibe(20);
      send({ t: "again" });
    });
  });
})();
