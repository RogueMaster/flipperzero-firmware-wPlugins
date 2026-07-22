/* Pong. Registers {t:"pong"}. Lobby reuses the shared 1v1 challenge flow.
   Playing: a canvas renders the server's ball + paddles (normalised 0..1),
   your paddle orange. Input sends paddle{dir:-1|0|1} on press/release. We keep
   the last server frame and interpolate the ball lightly for smoothness. */
(function () {
  var canvas, cx;
  var raf = 0;
  var st = null;                 // latest server state
  var render = { bx: 0.5, by: 0.5 };  // smoothed ball for drawing
  var dir = 0;                   // current input direction
  var lastServerX = 0.5;         // previous server ball x, to spot a bounce
  var lastServerDir = 0;         // which way the server ball was last travelling
  var prevPhase = "";
  var prevS1 = 0, prevS2 = 0;

  function ready() { canvas = $("pong-canvas"); cx = canvas.getContext("2d"); }

  function sizeCanvas() {
    var r = canvas.getBoundingClientRect();
    var w = Math.max(1, Math.round(r.width)), h = Math.max(1, Math.round(r.height));
    if (canvas.width !== w || canvas.height !== h) { canvas.width = w; canvas.height = h; }
  }

  function loop() {
    raf = requestAnimationFrame(loop);
    if (!st) return;
    // Ease the drawn ball toward the latest server position. The easing means the
    // drawn ball trails the real one by roughly a frame and a half, which is fine
    // mid-court but reads as "the ball bounced off nothing" at a paddle: the server
    // has already reversed while the drawing is still short of the wall. So snap
    // instead of easing on the frame the server reverses direction.
    var sdir = st.ball.x > lastServerX ? 1 : (st.ball.x < lastServerX ? -1 : 0);
    var reversed = sdir !== 0 && lastServerDir !== 0 && sdir !== lastServerDir;
    if (sdir !== 0) lastServerDir = sdir;
    lastServerX = st.ball.x;
    if (reversed) {
      render.bx = st.ball.x;
      render.by = st.ball.y;
    } else {
      render.bx += (st.ball.x - render.bx) * 0.4;
      render.by += (st.ball.y - render.by) * 0.4;
    }
    paint();
  }

  function paint() {
    var W = canvas.width, H = canvas.height;
    cx.fillStyle = "#0B0B0C"; cx.fillRect(0, 0, W, H);
    // Centre net.
    cx.strokeStyle = "#2E2E33"; cx.lineWidth = 2; cx.setLineDash([6, 8]);
    cx.beginPath(); cx.moveTo(W / 2, 0); cx.lineTo(W / 2, H); cx.stroke();
    cx.setLineDash([]);

    // PAD_W / BALL_R mirror PONG_PAD_W / PONG_BALL_R in ha_games.h, which places
    // the engine's bounce plane. If they drift apart the ball reverses off empty
    // space. The floors only bind on absurdly narrow canvases.
    var padW = Math.max(4, W * 0.02), padH = H * 0.22;
    var mine = st.me;   // 1 -> left paddle p1, 2 -> right paddle p2
    // Left paddle (p1).
    cx.fillStyle = mine === 1 ? "#FF8200" : "#EDEDE6";
    cx.fillRect(0, st.p1 * H - padH / 2, padW, padH);
    // Right paddle (p2).
    cx.fillStyle = mine === 2 ? "#FF8200" : "#EDEDE6";
    cx.fillRect(W - padW, st.p2 * H - padH / 2, padW, padH);
    // Ball (half width mirrors PONG_BALL_R).
    var br = Math.max(3, W * 0.018);
    cx.fillStyle = "#F2F2EE";
    cx.fillRect(render.bx * W - br, render.by * H - br, br * 2, br * 2);
  }

  function setDir(d) {
    if (d === dir) return;
    dir = d;
    send({ t: "paddle", dir: d });
  }

  function stopLoop() { if (raf) { cancelAnimationFrame(raf); raf = 0; } }

  function renderPlaying(m) {
    hide("pong-lobby"); hide("pong-over"); show("pong-match"); show("pong-leave");
    if (!canvas) ready();
    sizeCanvas();
    st = m;
    $("pong-score").innerHTML =
      '<span' + (m.me === 1 ? ' class="you"' : "") + ">You " + (m.me === 1 ? m.s1 : m.s2) + "</span>" +
      '<span' + (m.me === 2 ? ' class="you"' : "") + ">" + esc(m.opp || "Opp") + " " + (m.me === 1 ? m.s2 : m.s1) + "</span>";
    // Score change blip.
    if (prevPhase === "playing" && (m.s1 !== prevS1 || m.s2 !== prevS2)) { A.sfx("score"); A.vibe(20); }
    prevS1 = m.s1; prevS2 = m.s2;
    if (!raf) { render.bx = m.ball.x; render.by = m.ball.y; loop(); }
  }

  function renderOver(m) {
    stopLoop();
    hide("pong-lobby"); hide("pong-match"); show("pong-over"); show("pong-leave");
    var r = $("pong-result");
    var map = { win: ["Win", "win"], lose: ["Lose", "lose"], draw: ["Draw", "draw"] };
    var res = map[m.result] || ["Over", ""];
    r.textContent = res[0]; r.className = "result " + res[1];
    if (prevPhase !== "over") {
      if (m.result === "win") { A.sfx("win"); A.vibe([40, 60, 40, 60, 120]); }
      else if (m.result === "lose") { A.sfx("lose"); A.vibe(200); }
    }
  }

  function renderLobby(m) {
    stopLoop(); st = null;
    hide("pong-match"); hide("pong-over"); show("pong-lobby"); hide("pong-leave");
    lobbyView($("pong-incoming"), $("pong-players"), m.challenges);
  }

  A.handlers.pong = function (m) {
    route("pong");
    if (A.view !== "pong") return;
    if (m.phase === "playing") renderPlaying(m);
    else if (m.phase === "over") renderOver(m);
    else renderLobby(m);
    prevPhase = m.phase;
  };

  function bindHold(el, d) {
    var press = function (e) { e.preventDefault(); setDir(d); };
    var release = function (e) { e.preventDefault(); setDir(0); };
    el.addEventListener("pointerdown", press);
    el.addEventListener("pointerup", release);
    el.addEventListener("pointerleave", release);
    el.addEventListener("pointercancel", release);
  }

  document.addEventListener("DOMContentLoaded", function () {
    ready();
    bindHold($("pong-up"), -1);
    bindHold($("pong-dn"), 1);
    $("pong-leave").addEventListener("click", function () { setDir(0); send({ t: "leaveGame" }); });
    $("pong-back").addEventListener("click", function () { send({ t: "leaveGame" }); });

    document.addEventListener("keydown", function (e) {
      if (A.view !== "pong") return;
      if (e.key === "ArrowUp") setDir(-1);
      else if (e.key === "ArrowDown") setDir(1);
    });
    document.addEventListener("keyup", function (e) {
      if (A.view !== "pong") return;
      if (e.key === "ArrowUp" || e.key === "ArrowDown") setDir(0);
    });
    window.addEventListener("resize", function () { if (A.view === "pong" && canvas) sizeCanvas(); });
  });
})();
