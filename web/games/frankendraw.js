/* Frankendraw — the exquisite-corpse drawing game. Server-driven
   {t:"frankendraw", phase, ...}: lobby (ready-up) -> countdown -> draw (three
   rounds; the sheets rotate a seat each round so head/torso/legs come from three
   different hands) -> show (the gallery walk: each finished creature for a few
   seconds with live thumbs, then the winner once more) -> final. We send
   ready / stroke / undo / done / thumb{sheet,v} / again.

   Strokes go up as {t:"stroke"} with normalised 0..1 coordinates over the WHOLE
   sheet, exactly like Draw & Guess — no bitmaps ever move. The server quantises
   them onto its own 0..FD_UNIT grid and hands ink back in those units, which is
   why every message carries `unit`/`band`/`over` instead of the client hardcoding
   them. The only ink a drawer is sent is the sliver of the panel above.

   The gallery picture arrives as its own {t:"fdart"} message, broadcast once per
   creature; the small {phase:"show"} state is what gets pushed on every thumb tap,
   so the counts move live without re-sending the drawing each time. */
(function () {
  var myready = false;
  var cv, cx, gv, gx;            // the drawing canvas and the gallery canvas
  var st = null;                 // last draw-phase message
  var art = null;                // last fdart message (+ .q, its ink as 0..1 quads)
  var mine = [];                 // my own strokes this panel, as 0..1 quads
  var slice = [];                // the sliver handed down from the panel above
  var inkUsed = 0, inkCap = 0;   // segment budget for this panel (the ink bar)
  var drawing = false, lastX = 0, lastY = 0, lastSent = 0;
  var finaleFor = -1;            // winner cue guard
  var PANEL = ["fd.head", "fd.torso", "fd.legs"];
  var PAPER = "#EDEDE6", FOLD = "#c9c9be", AWAY = "#d7d7cd", GUIDE = "#b6b6a8";

  function ready() {
    cv = $("fd-canvas"); cx = cv.getContext("2d");
    gv = $("fd-gallery"); gx = gv.getContext("2d");
  }

  function sub(name) {
    ["lobby", "count", "play", "show", "final"].forEach(function (id) {
      $("fd-" + id).classList.toggle("hide", id !== name);
    });
  }
  function stopBar() { A.timebarStop("fd-bar"); hide("fd-bar"); }

  /* Fit a play/gallery column into what is actually left below the header, measured
     rather than guessed (the CSS calc() is only a pre-paint fallback: header height and
     safe-area insets vary by phone). Every control has its own row and the canvas takes
     the remainder, so nothing can push the buttons off screen and the page has nothing
     to scroll. */
  function fitCol(id) {
    var el = $(id);
    el.style.height = "auto";
    var top = el.getBoundingClientRect().top + (window.pageYOffset || 0);
    var pad = parseFloat(getComputedStyle($("fd")).paddingBottom) || 0;
    el.style.height = Math.max(240, Math.round(window.innerHeight - top - pad)) + "px";
  }

  /* ---- canvas helpers. The sheet is the full canvas; y 0..1 spans all three
     panels, so a panel's band is [top/unit, bot/unit]. ---- */
  function fit(c) {
    var r = c.getBoundingClientRect();
    var w = Math.max(1, Math.round(r.width)), h = Math.max(1, Math.round(r.height));
    if (c.width !== w || c.height !== h) { c.width = w; c.height = h; }
  }
  function paper(c, k) { k.fillStyle = PAPER; k.fillRect(0, 0, c.width, c.height); }
  function folds(c, k, bands, unit) {
    k.strokeStyle = FOLD; k.lineWidth = 1;
    for (var i = 1; i < 3; i++) {
      var y = Math.round(i * bands / unit * c.height) + 0.5;
      k.beginPath(); k.moveTo(0, y); k.lineTo(c.width, y); k.stroke();
    }
  }
  function pen(c, k) {
    k.strokeStyle = "#111"; k.lineWidth = Math.max(2, c.width * 0.01);
    k.lineCap = "round"; k.lineJoin = "round";
  }
  function seg(c, k, q) {
    k.beginPath();
    k.moveTo(q[0] * c.width, q[1] * c.height);
    k.lineTo(q[2] * c.width, q[3] * c.height);
    k.stroke();
  }
  // Flat [x0,y0,x1,y1,...] in sheet units -> 0..1 quads.
  function quads(flat, unit) {
    var o = [], i;
    flat = flat || [];
    for (i = 0; i + 3 < flat.length; i += 4)
      o.push([flat[i] / unit, flat[i + 1] / unit, flat[i + 2] / unit, flat[i + 3] / unit]);
    return o;
  }

  /* A join mark on one band edge: a faint dashed line, two ticks about where the
     body should cross it, and a small label. It suggests where the neck or the hips
     belong so the next panel doesn't start half a sheet away — nothing is enforced,
     the whole band stays drawable. */
  function guide(c, k, y, label, half) {
    var py = Math.round(y * c.height) + 0.5, mid = c.width / 2;
    k.save();
    k.strokeStyle = GUIDE; k.lineWidth = 1;
    if (k.setLineDash) k.setLineDash([5, 5]);
    k.beginPath(); k.moveTo(0, py); k.lineTo(c.width, py); k.stroke();
    if (k.setLineDash) k.setLineDash([]);
    k.lineWidth = 3;
    k.beginPath();
    k.moveTo(mid - half * c.width, py - 6); k.lineTo(mid - half * c.width, py + 6);
    k.moveTo(mid + half * c.width, py - 6); k.lineTo(mid + half * c.width, py + 6);
    k.stroke();
    k.fillStyle = "#9a9a8c"; k.font = "10px sans-serif";
    k.textAlign = "center"; k.textBaseline = "bottom";
    k.fillText(label, mid, py - 8);
    k.restore();
  }
  // Boundary 1 is where the torso meets the head, boundary 2 where the legs start.
  function joinLabel(b) { return b === 1 ? t("fd.join_neck") : t("fd.join_legs"); }
  function joinWidth(b) { return b === 1 ? 0.09 : 0.15; }

  function paintPlay() {
    if (!st || st.panel < 0 || !cv) return;
    fit(cv); paper(cv, cx);
    folds(cv, cx, st.band, st.unit);
    var top = st.top / st.unit, bot = st.bot / st.unit, over = st.over / st.unit;
    // Fold the rest of the sheet away: you draw your third and nothing else.
    cx.fillStyle = AWAY;
    if (top > over) cx.fillRect(0, 0, cv.width, (top - over) * cv.height);
    if (bot < 1) cx.fillRect(0, bot * cv.height, cv.width, (1 - bot) * cv.height);
    if (st.panel > 0) guide(cv, cx, top, joinLabel(st.panel), joinWidth(st.panel));
    if (st.panel < st.rounds - 1)
      guide(cv, cx, bot, joinLabel(st.panel + 1), joinWidth(st.panel + 1));
    pen(cv, cx);
    var i;
    for (i = 0; i < slice.length; i++) seg(cv, cx, slice[i]);
    for (i = 0; i < mine.length; i++) seg(cv, cx, mine[i]);
    if (top > 0) {  // tint the sliver so it reads as "carry on from here"
      cx.fillStyle = "rgba(255,138,0,.10)";
      cx.fillRect(0, (top - over) * cv.height, cv.width, over * cv.height);
    }
  }

  function paintArt() {
    if (!art || !gv) return;
    fit(gv); paper(gv, gx);
    folds(gv, gx, art.band, art.unit);
    pen(gv, gx);
    var p, i;
    for (p = 0; p < art.q.length; p++)
      for (i = 0; i < art.q[p].length; i++) seg(gv, gx, art.q[p][i]);
    // A small name box at the top-left of each band: who drew that part.
    gx.font = "bold 11px sans-serif";
    gx.textAlign = "left"; gx.textBaseline = "middle";
    for (p = 0; p < (art.who || []).length; p++) {
      var label = art.who[p] || t("fd.nobody");
      var y = (p * art.band / art.unit) * gv.height + 13;
      var w = gx.measureText(label).width + 12;
      gx.fillStyle = "rgba(17,17,17,.72)";
      gx.fillRect(5, y - 9, w, 18);
      gx.fillStyle = PAPER;
      gx.fillText(label, 11, y);
    }
  }

  /* ---- ink budget. The server accepts every segment until the panel's cap and
     refuses the rest, so counting locally is exact; we adopt the server's `used`
     whenever we have no local history (a fresh panel, or a reconnect). ---- */
  function inkBar() {
    var bar = $("fd-ink"), fill = bar.firstElementChild;
    var frac = inkCap ? Math.min(1, inkUsed / inkCap) : 0;
    fill.style.transform = "scaleX(" + frac + ")";
    bar.classList.toggle("hot", frac >= 0.85);
  }
  function dry() { return inkCap > 0 && inkUsed >= inkCap; }
  function canDraw() { return st && st.panel >= 0 && !st.done && !dry(); }

  function refreshPlay() {
    inkBar();
    var can = canDraw();
    cv.classList.toggle("drawable", !!can);
    $("fd-undo").disabled = !!(st && st.done) || !mine.length;
    $("fd-next").disabled = !!(st && st.done);
    $("fd-note").textContent = st && st.done ? t("fd.waiting", { n: st.waiting })
      : dry() ? t("fd.ink_full")
        : st && st.panel === 0 ? t("fd.hint_head") : t("fd.hint_next");
  }

  function clamp01(v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
  function norm(e) {
    var r = cv.getBoundingClientRect();
    var p = e.touches && e.touches[0] ? e.touches[0] : e;
    return { x: clamp01((p.clientX - r.left) / r.width), y: clamp01((p.clientY - r.top) / r.height) };
  }
  // Keep the pen inside your own band. The server clamps too — this is only so the
  // line you see matches the line it stores.
  function band(y) {
    var top = st.top / st.unit, bot = st.bot / st.unit;
    return y < top ? top : y > bot ? bot : y;
  }

  function down(e) {
    if (!canDraw()) return;
    e.preventDefault();
    drawing = true;
    var p = norm(e); lastX = p.x; lastY = band(p.y);
  }
  function moveEvt(e) {
    if (!canDraw() || !drawing) return;
    e.preventDefault();
    var now = Date.now();
    if (now - lastSent < 40) return;
    var p = norm(e), y = band(p.y);
    // The budget is finite, so only send a segment once the pen has actually
    // travelled — a doodle, not a sample stream.
    if (Math.abs(p.x - lastX) + Math.abs(y - lastY) < 0.02) return;
    lastSent = now;
    var q = [lastX, lastY, p.x, y];
    mine.push(q);
    inkUsed++;
    pen(cv, cx); seg(cv, cx, q);
    send({ t: "stroke", x0: q[0], y0: q[1], x1: q[2], y1: q[3] });
    lastX = p.x; lastY = y;
    refreshPlay();
  }
  function up() { drawing = false; }

  /* ---- screens ---- */
  function renderLobby(m) {
    sub("lobby"); stopBar();
    st = null; art = null;
    myready = A.readyLobby({ players: m.players, listId: "fd-players", readyId: "fd-ready", meId: "fd-me" });
    var short = m.need - (m.players || []).length;
    $("fd-need").textContent = short > 0 ? t("fd.need", { n: m.need }) : "";
  }

  function renderCount(m) {
    sub("count"); stopBar();
    A.countdown("fd-count-num", m.sec);
  }

  function renderPlay(m) {
    sub("play");
    var fresh = !st || st.round !== m.round;
    st = m;
    finaleFor = -1;
    if (fresh) mine = [];
    if (fresh || !mine.length) inkUsed = m.used || 0;
    inkCap = m.cap || 0;
    slice = quads(m.ink, m.unit);
    $("fd-meta").textContent = t("common.round", { n: m.round, total: m.rounds }) +
      (m.panel >= 0 ? " · " + t(PANEL[m.panel]) : "");
    noteDeadline(m.deadline, m.dur);
    A.timebar("fd-bar", m.deadline, m.dur, false);

    fitCol("fd-play");

    if (m.panel < 0) {   // joined mid-game: no sheet this time round
      hide("fd-ink"); hide("fd-stage"); $("fd-undo").disabled = true; $("fd-next").disabled = true;
      $("fd-note").textContent = t("fd.wait_next");
      return;
    }
    show("fd-ink"); show("fd-stage");
    if (fresh) { A.sfx("start"); A.vibe(20); }
    refreshPlay();
    paintPlay();
  }

  function renderShow(m) {
    sub("show"); stopBar();
    $("fd-show-meta").textContent = m.final
      ? t("fd.winner_net", { n: m.net })
      : t("fd.gallery", { n: m.n + 1, total: m.total });
    noteDeadline(m.deadline, m.dur);
    A.timebar("fd-sbar", m.deadline, m.dur, false);
    $("fd-thumbs").classList.toggle("hide", !!m.final);
    $("fd-upn").textContent = m.up || 0;
    $("fd-downn").textContent = m.down || 0;
    $("fd-up").classList.toggle("on", m.mine > 0);
    $("fd-down").classList.toggle("on", m.mine < 0);
    if (m.final && finaleFor !== m.n) { finaleFor = m.n; A.sfx("win"); A.vibe([30, 50, 30]); }
    fitCol("fd-show");
    paintArt();
  }

  function names(who) {
    var o = [], i;
    for (i = 0; i < (who || []).length; i++) o.push(who[i] || t("fd.nobody"));
    return o.join(" · ");
  }

  function renderFinal(m) {
    sub("final"); stopBar();
    st = null;
    $("fd-best").textContent = t("fd.best", { who: names(m.who), n: m.net || 0 });
    var b = A.podium("fd-podium", m.board);
    if (b && b.length && b[0].pid === A.pid) { A.sfx("win"); A.vibe([30, 50, 30]); }
    else { A.sfx("start"); A.vibe(20); }
  }

  A.handlers.frankendraw = function (m) {
    route("fd");
    if (A.view !== "fd") return;
    if (!cv) ready();
    switch (m.phase) {
      case "lobby": renderLobby(m); break;
      case "countdown": renderCount(m); break;
      case "draw": renderPlay(m); break;
      case "show": renderShow(m); break;
      case "final": renderFinal(m); break;
    }
  };

  // The gallery picture, broadcast once per creature (see the header note).
  A.handlers.fdart = function (m) {
    if (!cv) ready();
    m.q = [];
    for (var p = 0; p < (m.ink || []).length; p++) m.q.push(quads(m.ink[p], m.unit));
    art = m;
    if (A.view === "fd") paintArt();
  };

  function thumb(v) {
    if (!art) return;
    A.sfx("buzz"); A.vibe(12);
    send({ t: "thumb", sheet: art.n, v: v });
  }

  document.addEventListener("DOMContentLoaded", function () {
    ready();
    cv.addEventListener("pointerdown", down);
    cv.addEventListener("pointermove", moveEvt);
    window.addEventListener("pointerup", up);
    cv.addEventListener("touchstart", down, { passive: false });
    cv.addEventListener("touchmove", moveEvt, { passive: false });
    window.addEventListener("touchend", up);

    $("fd-ready").addEventListener("click", function () {
      A.sfx("buzz"); A.vibe(15);
      send({ t: "ready", ready: !myready });
    });
    $("fd-undo").addEventListener("click", function () {
      if (!st || st.panel < 0 || st.done || !mine.length) return;
      A.sfx("buzz"); A.vibe(10);
      mine.pop();
      if (inkUsed > 0) inkUsed--;
      send({ t: "undo" });
      refreshPlay(); paintPlay();
    });
    $("fd-next").addEventListener("click", function () {
      if (!st || st.panel < 0 || st.done) return;
      A.sfx("start"); A.vibe(20);
      send({ t: "done" });
      st.done = true;
      refreshPlay();
    });
    $("fd-up").addEventListener("click", function () { thumb(1); });
    $("fd-down").addEventListener("click", function () { thumb(-1); });
    $("fd-again").addEventListener("click", function () {
      A.sfx("start"); A.vibe(20);
      send({ t: "again" });
    });

    window.addEventListener("resize", function () {
      if (A.view !== "fd") return;
      if (!$("fd-play").classList.contains("hide")) fitCol("fd-play");
      if (!$("fd-show").classList.contains("hide")) fitCol("fd-show");
      paintPlay(); paintArt();
    });
  });
})();
