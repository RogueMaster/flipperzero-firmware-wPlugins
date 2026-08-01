/* Battleship — 1v1. Shares the challenge lobby (lobbyView) and the rematch/leave
   flow with the duels, but has its own screen: a placement phase (arrange your fleet
   locally, then send it) and a firing phase with two 10x10 grids (enemy waters +
   your fleet). Server is the referee and hides the enemy's un-hit ships; we send
   place{ships} / fire{n} / rematch / leaveGame. */
(function () {
  var SIZE = 10, N = 100;
  var LENS = [5, 4, 3, 3, 2];          // fixed ship order (server matches this)
  var NAMES = ["Carrier", "Battleship", "Cruiser", "Submarine", "Destroyer"];
  var ships = null;                     // local placement: [{len,r,c,d,placed}]
  var sel = 0;                          // selected ship index
  var prevPhase = "";
  var prevTrack = null;                 // last tracking grid, to animate new shots
  var prevMine = null;                  // last fleet grid, to detect incoming hits
  var prevYourTurn = false;             // to cue when the turn flips to you
  var rematchTimer = null;

  function sub(name) {
    ["lobby", "place", "fire", "over"].forEach(function (id) {
      $("bs-" + id).classList.toggle("hide", id !== name);
    });
  }

  /* ---- generic 10x10 grid renderer ---- */
  function grid(el, cls, tap) {
    var rebuild = el.childElementCount !== N;
    if (rebuild) el.innerHTML = "";
    for (var i = 0; i < N; i++) {
      var cell = rebuild ? document.createElement("div") : el.children[i];
      cell.className = "bs-cell " + cls(i);
      if (rebuild) el.appendChild(cell);
    }
    el.onclick = tap
      ? function (e) {
          var idx = Array.prototype.indexOf.call(el.children, e.target);
          if (idx >= 0) tap(idx);
        }
      : null;
  }

  /* ---- placement (local) ---- */
  function cellsOf(s) {
    var out = [];
    for (var k = 0; k < s.len; k++) out.push(s.d ? (s.r + k) * SIZE + s.c : s.r * SIZE + (s.c + k));
    return out;
  }
  function fits(s, ignore) {
    if (s.d ? s.r + s.len > SIZE : s.c + s.len > SIZE) return false;
    if (s.r < 0 || s.c < 0) return false;
    var occ = {};
    ships.forEach(function (o, i) {
      if (i === ignore || !o.placed) return;
      cellsOf(o).forEach(function (idx) { occ[idx] = 1; });
    });
    return cellsOf(s).every(function (idx) { return !occ[idx]; });
  }
  function initPlacement() {
    ships = LENS.map(function (len) { return { len: len, r: 0, c: 0, d: 0, placed: false }; });
    sel = 0;
    randomize(); // start with a valid random fleet; players tweak or just hit Ready
  }
  function randomize() {
    for (var attempt = 0; attempt < 500; attempt++) {
      ships.forEach(function (s) { s.placed = false; });
      var ok = true;
      for (var i = 0; i < ships.length && ok; i++) {
        var placed = false;
        for (var t = 0; t < 100 && !placed; t++) {
          var s = ships[i];
          s.d = Math.random() < 0.5 ? 0 : 1;
          s.r = Math.floor(Math.random() * SIZE);
          s.c = Math.floor(Math.random() * SIZE);
          if (fits(s, i)) { s.placed = true; placed = true; }
        }
        if (!placed) ok = false;
      }
      if (ok) return;
    }
  }
  function allPlaced() { return ships.every(function (s) { return s.placed; }); }

  function renderTray() {
    var tray = $("bs-tray");
    tray.innerHTML = "";
    ships.forEach(function (s, i) {
      var b = document.createElement("button");
      b.className = "bs-ship" + (i === sel ? " sel" : "") + (s.placed ? " placed" : "");
      b.textContent = NAMES[i] + " (" + s.len + ")";
      b.onclick = function () { sel = i; renderPlace(lastPlaceMsg); };
      tray.appendChild(b);
    });
  }

  var lastPlaceMsg = null;
  function renderPlace(m) {
    lastPlaceMsg = m;
    sub("place");
    if (m.ready) {                       // committed: lock, wait for opponent
      $("bs-place-actions").classList.add("hide");
      $("bs-wait").classList.remove("hide");
      $("bs-place-hint").textContent = m.oppReady ? "Both ready..." : "Waiting for opponent...";
    } else {
      $("bs-place-actions").classList.remove("hide");
      $("bs-wait").classList.add("hide");
      $("bs-place-hint").textContent = m.oppReady ? "Opponent is ready - arrange & Ready" : "Tap a ship then a cell to move it, or Random";
    }
    $("bs-ready").disabled = !allPlaced();
    var occ = {};
    ships.forEach(function (s, i) { if (s.placed) cellsOf(s).forEach(function (idx) { occ[idx] = i; }); });
    grid($("bs-place-grid"), function (i) {
      return i in occ ? "ship" + (occ[i] === sel ? " sel" : "") : "";
    }, m.ready ? null : function (i) {
      if (i in occ) { sel = occ[i]; A.vibe(6); renderPlace(m); return; } // tap a ship to select it
      var s = ships[sel];
      var old = { r: s.r, c: s.c, placed: s.placed };
      s.r = Math.floor(i / SIZE); s.c = i % SIZE;
      if (fits(s, sel)) {
        s.placed = true;
        A.sfx("drop"); A.vibe(12);
        renderPlace(m);
      } else { s.r = old.r; s.c = old.c; s.placed = old.placed; }
    });
    renderTray();
  }

  /* ---- firing ---- */
  function renderFire(m) {
    sub("fire");
    var t = $("bs-turn");
    t.textContent = m.yourTurn ? "Your turn - fire!" : esc(m.opp || "Opponent") + "'s turn";
    t.className = "turn" + (m.yourTurn ? " you" : "");
    $("bs-ships").textContent = "You " + m.myShips + " | Them " + m.oppShips;
    var TR = ["", "miss", "hit", "sunk"];
    var track = $("bs-track-grid");
    track.classList.toggle("waiting", !m.yourTurn); // dim + no taps when it's not your turn
    track.classList.toggle("myturn", m.yourTurn);   // orange border when it's your turn to fire
    // Your shot just resolved (a track cell went from un-shot to a result): play a
    // distinct sound for miss / hit / sunk. track only changes on your own shots.
    if (prevTrack) {
      for (var s = 0; s < N; s++) {
        if (prevTrack[s] === 0 && m.track[s] !== 0) {
          var v = m.track[s];
          A.sfx(v === 1 ? "miss" : v === 3 ? "sunk" : "hit");
          A.vibe(v === 1 ? 12 : [20, 40]);
          break;
        }
      }
    }
    grid(track, function (i) {
      var cls = TR[m.track[i]];
      if (prevTrack && prevTrack[i] === 0 && m.track[i] !== 0) cls += " pop"; // this shot just landed
      return cls;
    }, m.yourTurn ? function (i) {
      if (m.track[i] !== 0) return;      // already fired here
      A.vibe(10);                        // tap feedback; the hit/miss sound plays on the result
      send({ t: "fire", n: i });
    } : null);
    prevTrack = m.track.slice();
    var MI = ["", "ship", "miss", "hit"];
    // Incoming fire: play a sound + haptic when one of your own ships takes a hit.
    if (prevMine) {
      for (var mi = 0; mi < N; mi++) {
        if (prevMine[mi] !== 3 && m.mine[mi] === 3) { A.sfx("hit"); A.vibe([30, 60]); break; }
      }
    }
    prevMine = m.mine.slice();
    // Cue when the turn flips to you (a hit keeps the opponent firing, so this only
    // fires when they miss and it's genuinely your shot).
    if (m.yourTurn && !prevYourTurn) { A.sfx("tick"); A.vibe(30); }
    prevYourTurn = m.yourTurn;
    grid($("bs-fleet-grid"), function (i) { return MI[m.mine[i]]; }, null);
  }

  function renderOver(m) {
    sub("over");
    var r = $("bs-result");
    r.textContent = m.result === "win" ? "You win!" : "Defeated";
    r.className = "result " + (m.result === "win" ? "win" : "lose");
    if (prevPhase !== "over") {
      if (m.result === "win") { A.sfx("win"); A.vibe([40, 60, 40, 60, 120]); }
      else { A.sfx("lose"); A.vibe(200); }
    }
    // reveal the enemy fleet with your shots overlaid
    var TR = ["", "miss", "hit", "sunk"];
    grid($("bs-over-grid"), function (i) {
      if (m.track[i]) return TR[m.track[i]];
      return m.oppFleet && m.oppFleet[i] ? "ship" : "";
    }, null);
  }

  A.handlers.bs = function (m) {
    route("bs");
    if (A.view !== "bs") return;
    if (rematchTimer && m.phase !== "over") { clearTimeout(rematchTimer); rematchTimer = null; }
    if (m.phase !== "fire") { prevTrack = null; prevMine = null; prevYourTurn = false; } // reset cues across phases
    $("bs-leave").classList.toggle("hide", m.phase === "lobby");
    if (m.phase === "lobby") {
      sub("lobby");
      lobbyView($("bs-incoming"), $("bs-players"), m.challenges);
    } else if (m.phase === "place") {
      if (prevPhase !== "place") initPlacement();
      renderPlace(m);
    } else if (m.phase === "fire") {
      renderFire(m);
    } else if (m.phase === "over") {
      renderOver(m);
    }
    prevPhase = m.phase;
  };

  document.addEventListener("DOMContentLoaded", function () {
    $("bs-rotate").addEventListener("click", function () {
      var s = ships[sel];
      s.d ^= 1;
      if (s.placed && !fits(s, sel)) s.d ^= 1;   // revert if the rotation doesn't fit
      A.vibe(8);
      renderPlace(lastPlaceMsg);
    });
    $("bs-random").addEventListener("click", function () {
      randomize(); sel = 0; A.sfx("drop"); A.vibe(15);
      renderPlace(lastPlaceMsg);
    });
    $("bs-ready").addEventListener("click", function () {
      if (!allPlaced()) return;
      var str = ships.map(function (s) { return s.r + "," + s.c + "," + s.d; }).join(";");
      A.sfx("buzz"); A.vibe(20);
      send({ t: "place", ships: str });
    });
    $("bs-leave").addEventListener("click", function () { send({ t: "leaveGame" }); });
    $("bs-back").addEventListener("click", function () { send({ t: "leaveGame" }); });
    $("bs-rematch").addEventListener("click", function () {
      A.sfx("buzz"); send({ t: "rematch" });
      if (rematchTimer) clearTimeout(rematchTimer);
      rematchTimer = setTimeout(function () {
        rematchTimer = null;
        if (prevPhase === "over" && A.view === "bs") { toast("Opponent left"); send({ t: "leaveGame" }); }
      }, 1500);
    });
  });
})();
