/* Spectrum — wavelength-style guessing. The ESP is authoritative, driving
   {t:"spectrum", phase, ...}: lobby (ready + pack vote) -> countdown -> play
   (stage clue: the psychic sees a hidden target and types a clue; stage guess:
   everyone else slides to guess where the clue lands; stage reveal: target +
   all guesses + points) -> ... -> final podium. We send ready / vote / clue /
   slide / again. Points by closeness; the psychic scores by how well the group
   guesses, so a good clue pays off. */
(function () {
  var myready = false;

  function sub(name) {
    ["lobby", "count", "play", "final"].forEach(function (id) {
      $("sp-" + id).classList.toggle("hide", id !== name);
    });
  }
  function stopBar() { A.timebarStop("sp-bar"); hide("sp-bar"); }

  // ---- half-circle dial geometry (matches the SVG viewBox 220x128) ----
  // Value 0 sits at the left end of the arc, 100 at the right; the needle and
  // the scoring wedges are drawn from the hub at (110,114), radius 96.
  var DC = { x: 110, y: 114, r: 96 };
  var SVGNS = "http://www.w3.org/2000/svg";
  function pt(v, r) {
    if (r == null) r = DC.r;
    var deg = 180 - Math.max(0, Math.min(100, v)) * 1.8; // 0->180deg (left), 100->0deg (right)
    var a = deg * Math.PI / 180;
    return { x: DC.x + r * Math.cos(a), y: DC.y - r * Math.sin(a) };
  }
  function sector(v1, v2) { // filled wedge hub -> arc(v1..v2) -> hub
    v1 = Math.max(0, Math.min(100, v1));
    v2 = Math.max(0, Math.min(100, v2));
    var a = pt(v1), b = pt(v2);
    // sweep-flag 1: bow the outer edge outward so it follows the dial's arc line
    // (convex), instead of cutting concavely back toward the hub.
    return "M" + DC.x + " " + DC.y + " L" + a.x.toFixed(1) + " " + a.y.toFixed(1) +
      " A" + DC.r + " " + DC.r + " 0 0 1 " + b.x.toFixed(1) + " " + b.y.toFixed(1) + " Z";
  }
  function mk(tag, attrs) {
    var e = document.createElementNS(SVGNS, tag);
    for (var k in attrs) e.setAttribute(k, attrs[k]);
    return e;
  }
  // Nested wedges centred on the target, one per scoring tier: ±12 (2pt), ±7
  // (3pt), ±2 (4pt, a tight bullseye for landing right on it), brighter toward the
  // centre — so "closer = more points" is visible, not just scored. Widest first.
  function drawTarget(target) {
    var g = $("sp-bands"); g.innerHTML = "";
    [[12, "b2"], [7, "b3"], [2, "b4"]].forEach(function (z) {
      g.appendChild(mk("path", { d: sector(target - z[0], target + z[0]), class: "sp-band " + z[1] }));
    });
    var top = pt(target, DC.r), hub = pt(target, 10);
    g.appendChild(mk("line", { x1: hub.x, y1: hub.y, x2: top.x, y2: top.y, class: "sp-tline" }));
  }
  function clearTarget() { $("sp-bands").innerHTML = ""; }
  function setNeedle(v, reveal) {
    var n = $("sp-ndl"), tip = pt(v, DC.r - 8);
    n.setAttribute("x2", tip.x.toFixed(1));
    n.setAttribute("y2", tip.y.toFixed(1));
    n.classList.remove("hide");
    n.classList.toggle("reveal", !!reveal);
  }

  // Grab-the-dial guessing: map a pointer position on the SVG back to a 0..100
  // value. The SVG scales to the container, so convert client px -> viewBox coords
  // first, then take the angle from the hub.
  var spGuess = 50, spCanGuess = false, dragging = false;
  function valFromEvent(e) {
    var svg = $("sp-dial"), r = svg.getBoundingClientRect();
    if (!r.width || !r.height) return spGuess; // not laid out yet; keep current
    var cx = (e.clientX != null) ? e.clientX : (e.touches && e.touches[0].clientX);
    var cy = (e.clientY != null) ? e.clientY : (e.touches && e.touches[0].clientY);
    var x = (cx - r.left) * 220 / r.width, y = (cy - r.top) * 128 / r.height;
    var deg = Math.atan2(DC.y - y, x - DC.x) * 180 / Math.PI; // 180 left, 90 up, 0 right
    deg = Math.max(0, Math.min(180, deg));
    return Math.round((180 - deg) / 1.8);
  }
  function dragMove(e) {
    if (!dragging || !spCanGuess) return;
    spGuess = valFromEvent(e);
    setNeedle(spGuess, false);
    $("sp-note").textContent = "Guess: " + spGuess + " — lock it in when ready.";
    if (e.cancelable) e.preventDefault();
  }
  function dragStart(e) { if (!spCanGuess) return; dragging = true; dragMove(e); }
  function dragEnd() { dragging = false; }

  function renderLobby(m) {
    sub("lobby");
    stopBar();
    myready = A.readyLobby({ players: m.players, listId: "sp-players", readyId: "sp-ready", meId: "sp-me" });
    A.packVote({
      boxId: "sp-topics",
      packs: m.packs, myvote: m.myvote,
      onVote: function (i) { send({ t: "vote", pack: i }); },
    });
  }

  function renderCount(m) {
    sub("count");
    stopBar();
    A.countdown("sp-count-num", m.sec);
  }

  var lastRound = -1, lastStage = "";
  function renderPlay(m) {
    sub("play");
    var stage = m.stage; // "clue" | "guess" | "reveal"
    $("sp-meta").textContent = "Round " + m.round + " / " + m.rounds;
    $("sp-role").textContent = m.iam ? "You are the psychic" : "Psychic: " + m.psychic;
    $("sp-left").textContent = m.left || "";
    $("sp-right").textContent = m.right || "";

    noteDeadline(m.deadline, m.dur);
    A.timebar("sp-bar", m.deadline, m.dur, false);

    var needle = $("sp-ndl"), dmarks = $("sp-dmarks");
    var clueForm = $("sp-clueform"), clue = $("sp-clue");
    var guessGo = $("sp-guessgo"), note = $("sp-note");
    dmarks.innerHTML = "";
    needle.classList.add("hide");
    spCanGuess = false; // only re-enabled below when I'm a guesser mid-guess

    // The scoring wedges + target line show to the psychic while cluing, and to
    // everyone on reveal.
    if (typeof m.target === "number") drawTarget(m.target); else clearTarget();

    clue.textContent = m.clue ? "“" + m.clue + "”" : "";
    clue.classList.toggle("hide", !m.clue);

    if (stage === "clue") {
      clueForm.classList.toggle("hide", !m.iam);
      guessGo.classList.add("hide");
      if (m.iam) {
        note.textContent = "Give a clue that points to the orange zone.";
        if (lastRound !== m.round) { $("sp-cluein").value = ""; setTimeout(function () { $("sp-cluein").focus(); }, 60); }
      } else {
        note.textContent = m.psychic + " is thinking of a clue…";
      }
    } else if (stage === "guess") {
      clueForm.classList.add("hide");
      if (m.iam) {
        spCanGuess = false;
        guessGo.classList.add("hide");
        note.textContent = "Clue sent. Waiting for guesses…";
      } else {
        var locked = (typeof m.myguess === "number" && m.myguess >= 0);
        if (lastRound !== m.round || lastStage !== "guess") spGuess = locked ? m.myguess : 50;
        spCanGuess = !locked;
        setNeedle(spGuess, false); // drag the dial to move it
        guessGo.classList.toggle("hide", locked);
        guessGo.disabled = locked;
        note.textContent = locked ? "Guess locked at " + m.myguess + ". Waiting…"
                                  : "Drag the dial to where the clue lands, then lock in.";
      }
    } else { // reveal
      spCanGuess = false;
      clueForm.classList.add("hide");
      guessGo.classList.add("hide");
      (m.guesses || []).forEach(function (g) {
        var hit = g.pts > 0;
        var dot = pt(g.g, DC.r), lab = pt(g.g, DC.r + 12);
        var c = document.createElementNS(SVGNS, "circle");
        c.setAttribute("cx", dot.x.toFixed(1)); c.setAttribute("cy", dot.y.toFixed(1));
        c.setAttribute("r", "4"); c.setAttribute("class", "sp-dot" + (hit ? " hit" : ""));
        dmarks.appendChild(c);
        var t = document.createElementNS(SVGNS, "text");
        t.setAttribute("x", lab.x.toFixed(1)); t.setAttribute("y", lab.y.toFixed(1));
        t.setAttribute("text-anchor", lab.x < DC.x - 10 ? "end" : lab.x > DC.x + 10 ? "start" : "middle");
        t.setAttribute("class", "sp-dlabel" + (hit ? " hit" : ""));
        t.textContent = g.nick + " +" + g.pts;
        dmarks.appendChild(t);
      });
      var gain = (typeof m.mygain === "number") ? m.mygain : 0;
      note.textContent = m.iam ? "Your clue earned you +" + gain
                               : (gain > 0 ? "Nice — +" + gain : "+0 this round");
      if (lastRound !== m.round || lastStage !== "reveal") {
        A.sfx(gain > 0 ? "correct" : "buzz"); A.vibe(gain > 0 ? 25 : 12);
      }
    }
    lastRound = m.round; lastStage = stage;
  }

  function renderFinal(m) {
    sub("final");
    stopBar();
    var b = A.podium("sp-podium", m.board);
    if (b && b.length && b[0].pid === A.pid) { A.sfx("win"); A.vibe([30, 50, 30]); }
    else { A.sfx("start"); A.vibe(20); }
  }

  A.handlers.spectrum = function (m) {
    route("spectrum");
    if (A.view !== "spectrum") return;
    switch (m.phase) {
      case "lobby": renderLobby(m); break;
      case "countdown": renderCount(m); break;
      case "play": renderPlay(m); break;
      case "final": renderFinal(m); break;
    }
  };

  $("sp-ready").addEventListener("click", function () {
    A.sfx("buzz"); A.vibe(15);
    send({ t: "ready", ready: !myready });
  });
  // Grab the dial to guess (pointer covers mouse + touch); a lock-in confirms.
  var dial = $("sp-dial");
  dial.addEventListener("pointerdown", dragStart);
  window.addEventListener("pointermove", dragMove, { passive: false });
  window.addEventListener("pointerup", dragEnd);
  window.addEventListener("pointercancel", dragEnd);
  $("sp-cluego").addEventListener("click", function () {
    var v = $("sp-cluein").value.trim();
    if (!v) return;
    A.sfx("start"); A.vibe(20);
    send({ t: "clue", text: v });
  });
  $("sp-cluein").addEventListener("keydown", function (e) {
    if (e.key === "Enter") $("sp-cluego").click();
  });
  $("sp-guessgo").addEventListener("click", function () {
    A.sfx("buzz"); A.vibe(18);
    send({ t: "slide", n: spGuess });
    spCanGuess = false;
    this.classList.add("hide");
  });
  $("sp-again").addEventListener("click", function () {
    A.sfx("start"); A.vibe(20);
    send({ t: "again" });
  });
})();
