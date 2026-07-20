// Zero-dependency mock of the ESP referee, for eyeballing the client in a real
// browser. Serves dist/index.html at "/" and speaks just enough of RFC 6455 at
// "/ws" to drive the core games: trivia, the board duels (connect4 / tic-tac-toe
// / dots & boxes), draw & guess, and pong (with emoji avatars). The v0.2.0 party
// games (would-you-rather / scramble / reaction) and reversi run on hardware.
//
//   npm run build && npm run mock   ->   open http://localhost:8080
//
// This is a demo harness, not the real referee: it scripts plausible sequences
// and implements just-good-enough rules so each screen renders and plays. It
// rotates through the games; use each game's Back / Leave button (or, for
// trivia, just wait) to advance to the next one.
import { createServer } from "node:http";
import { createHash } from "node:crypto";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";

const ROOT = dirname(fileURLToPath(import.meta.url));
const PAGE = join(ROOT, "dist", "index.html");
const PORT = process.env.PORT || 8080;
const GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

const server = createServer((req, res) => {
  try {
    const body = readFileSync(PAGE);
    res.writeHead(200, { "Content-Type": "text/html" });
    res.end(body);
  } catch {
    res.writeHead(500).end("Run `npm run build` first.");
  }
});

server.on("upgrade", (req, socket) => {
  if (!req.url.startsWith("/ws")) { socket.destroy(); return; }
  const key = req.headers["sec-websocket-key"];
  const accept = createHash("sha1").update(key + GUID).digest("base64");
  socket.write(
    "HTTP/1.1 101 Switching Protocols\r\n" +
    "Upgrade: websocket\r\nConnection: Upgrade\r\n" +
    "Sec-WebSocket-Accept: " + accept + "\r\n\r\n"
  );
  handle(socket);
});

function frame(socket, obj) {
  const data = Buffer.from(JSON.stringify(obj));
  const len = data.length;
  let head;
  if (len < 126) head = Buffer.from([0x81, len]);
  else if (len < 65536) head = Buffer.from([0x81, 126, len >> 8, len & 255]);
  else head = Buffer.from([0x81, 127, 0, 0, 0, 0, len >>> 24, (len >> 16) & 255, (len >> 8) & 255, len & 255]);
  try { socket.write(Buffer.concat([head, data])); } catch {}
}

function handle(socket) {
  const ME = 1, BOT = 2, CATHY = 3;
  const nicks = { 1: "You", 2: "Botby", 3: "Cathy" };
  const avatars = { 1: "🙂", 2: "🤖", 3: "🦊" };
  const scores = { 1: 0, 2: 0, 3: 0 };
  let timers = [];
  let loopId = 0;
  const at = (ms, fn) => timers.push(setTimeout(fn, ms));
  const clearTimers = () => { timers.forEach(clearTimeout); timers = []; if (loopId) { clearInterval(loopId); loopId = 0; } };

  const send = (obj) => frame(socket, obj);
  const players = () => [1, 2, 3].map((p) => ({ pid: p, nick: nicks[p], avatar: avatars[p], score: scores[p] }));
  const lobby = (game) => send({ t: "lobby", game, players: players(), me: ME });
  const sc = () => players();

  // ---- game rotation --------------------------------------------------------
  // Test knobs: STAGE=<name> jumps the rotation to a game; LOBBY=1 sits in the
  // app lobby (game "none") so the lobby chat can be exercised on its own.
  const STAGES = ["trivia", "c4", "ttt", "dots", "draw", "pong"];
  const START = STAGES.indexOf(process.env.STAGE || "");
  let stage = START >= 0 ? START : 0;
  const curStage = () => STAGES[stage % STAGES.length];
  function startStage() {
    clearTimers();
    const s = curStage();
    if (s === "trivia") runTrivia();
    else if (s === "draw") runDraw();
    else if (s === "pong") runPong();
    else runDuel(s);
  }
  function nextStage() { stage++; lobby("none"); at(700, startStage); }

  // ---- trivia (fully phone-driven, self-organizing) -------------------------
  // The mock plays the ESP referee: it tracks ready/vote state, runs a 5s
  // countdown once everyone is ready, asks a few questions (bots auto-answer),
  // reveals with counts + a moving leaderboard, then a final + `again` restart.
  const QDUR = 8000;
  const TOPIC_NAMES = ["Science", "Movies", "History"];
  const QUESTIONS = [
    { q: "Which planet is the largest in our solar system?", o: ["Mars", "Jupiter", "Saturn", "Neptune"], correct: 1 },
    { q: "What is the chemical symbol for gold?", o: ["Gd", "Au", "Ag", "Go"], correct: 1 },
    { q: "How many continents are there on Earth?", o: ["5", "6", "7", "8"], correct: 2 },
  ];
  let tv = null;

  function tvBoard() {
    return [1, 2, 3].map((p) => ({ pid: p, nick: nicks[p], avatar: avatars[p], score: tv.score[p] }))
      .sort((a, b) => b.score - a.score);
  }
  function tvTopics() {
    return TOPIC_NAMES.map((name, i) => ({
      name, votes: Object.values(tv.votes).filter((v) => v === i).length,
    }));
  }
  function sendTvLobby() {
    send({
      t: "trivia", phase: "lobby",
      players: [1, 2, 3].map((p) => ({ pid: p, nick: nicks[p], avatar: avatars[p], ready: !!tv.ready[p] })),
      topics: tvTopics(),
      myvote: (ME in tv.votes) ? tv.votes[ME] : -1,
      myready: !!tv.ready[ME],
    });
  }
  function runTrivia() {
    lobby("trivia");
    tv = { phase: "lobby", ready: {}, votes: {}, score: { 1: 0, 2: 0, 3: 0 }, qi: -1, answers: {} };
    at(300, sendTvLobby);
    // Bots wander in, vote, and ready up so the human can be the last to ready.
    at(1600, () => { tv.votes[BOT] = 1; if (tv.phase === "lobby") sendTvLobby(); });
    at(2600, () => { tv.votes[CATHY] = 0; tv.ready[CATHY] = true; if (tv.phase === "lobby") { sendTvLobby(); checkStart(); } });
    at(3600, () => { tv.ready[BOT] = true; if (tv.phase === "lobby") { sendTvLobby(); checkStart(); } });
  }
  function checkStart() {
    if (tv.phase === "lobby" && [1, 2, 3].every((p) => tv.ready[p])) startCountdown();
  }
  function startCountdown() {
    tv.phase = "countdown";
    const votes = tvTopics();
    let best = 0;
    votes.forEach((t, i) => { if (t.votes > votes[best].votes) best = i; });
    tv.topicName = votes[best].name;
    tv.secs = 5;
    tickCountdown();
  }
  function tickCountdown() {
    if (tv.phase !== "countdown") return;                 // cancelled by an un-ready
    send({ t: "trivia", phase: "countdown", secs: tv.secs, topic: tv.topicName });
    if (tv.secs <= 1) { at(1000, startQuestions); return; }
    tv.secs--;
    at(1000, tickCountdown);
  }
  function startQuestions() {
    if (tv.phase !== "countdown") return;
    tv.qi = 0;
    askQuestion();
  }
  function sendQuestion() {
    const Q = QUESTIONS[tv.qi];
    send({
      t: "trivia", phase: "question", i: tv.qi, n: QUESTIONS.length, q: Q.q, o: Q.o,
      dur: QDUR, deadline: tv.deadline, mine: (ME in tv.answers) ? tv.answers[ME] : -1,
      answered: Object.keys(tv.answers).length, total: 3, topic: tv.topicName, board: tvBoard(),
    });
  }
  function askQuestion() {
    tv.phase = "question";
    tv.answers = {};
    tv.deadline = Date.now() + QDUR;
    sendQuestion();
    at(1400, () => botAnswer(BOT));
    at(2600, () => botAnswer(CATHY));
    at(QDUR + 300, revealQuestion);
  }
  function botAnswer(p) {
    if (tv.phase !== "question" || (p in tv.answers)) return;
    const Q = QUESTIONS[tv.qi];
    tv.answers[p] = Math.random() < 0.6 ? Q.correct : Math.floor(Math.random() * 4);
    sendQuestion();
  }
  function revealQuestion() {
    if (tv.phase !== "question") return;
    tv.phase = "reveal";
    const Q = QUESTIONS[tv.qi];
    const counts = [0, 0, 0, 0];
    let gained = 0;
    [1, 2, 3].forEach((p) => {
      if (!(p in tv.answers)) return;
      counts[tv.answers[p]]++;
      if (tv.answers[p] === Q.correct) { tv.score[p] += 100; if (p === ME) gained = 100; }
    });
    send({
      t: "trivia", phase: "reveal", i: tv.qi, n: QUESTIONS.length, q: Q.q, o: Q.o,
      correct: Q.correct, counts, mine: (ME in tv.answers) ? tv.answers[ME] : -1,
      gained, board: tvBoard(),
    });
    at(3200, () => {
      tv.qi++;
      if (tv.qi >= QUESTIONS.length) { tv.phase = "final"; send({ t: "trivia", phase: "final", board: tvBoard() }); }
      else askQuestion();
    });
  }

  // ---- duels (c4 / ttt / dots) ---------------------------------------------
  const GAME_OF = { c4: "connect4", ttt: "tictactoe", dots: "dots" };
  let duel = null;
  function runDuel(kind) {
    duel = null;
    lobby(GAME_OF[kind]);
    at(300, () => send({ t: "duel", kind, phase: "lobby", you: ME, me: 1,
      opp: nicks[CATHY], challenges: [{ from: CATHY, to: ME }] }));
  }

  function newDuel(kind, first) {
    const d = { kind, opp: CATHY, turn: first, first };
    if (kind === "dots") {
      d.w = 3; d.h = 3;
      d.hedges = new Array((d.h + 1) * d.w).fill(0);
      d.vedges = new Array(d.h * (d.w + 1)).fill(0);
      d.boxes = new Array(d.w * d.h).fill(0);
      d.sme = 0; d.sopp = 0;
    } else {
      d.cols = kind === "ttt" ? 3 : 7;
      d.rows = kind === "ttt" ? 3 : 6;
      d.board = new Array(d.cols * d.rows).fill(0);
    }
    return d;
  }

  function sendDuel(phase, extra) {
    const d = duel;
    const base = { t: "duel", kind: d.kind, phase, you: ME, me: 1, opp: nicks[d.opp], turn: d.turn };
    if (d.kind === "dots") Object.assign(base, { w: d.w, h: d.h, hedges: d.hedges, vedges: d.vedges, boxes: d.boxes, sme: d.sme, sopp: d.sopp });
    else Object.assign(base, { cols: d.cols, rows: d.rows, need: d.kind === "ttt" ? 3 : 4, gravity: d.kind !== "ttt", board: d.board });
    send(Object.assign(base, extra || {}));
  }
  // In the mock, "me" (pid 1) plays mark 1, the opponent plays mark 2.
  const resultFor = (winnerPid) => winnerPid == null ? "draw" : (winnerPid === ME ? "win" : "lose");

  function c4Win(b, who) {
    const at2 = (r, c) => (r >= 0 && r < 6 && c >= 0 && c < 7) ? b[r * 7 + c] : 0;
    for (let r = 0; r < 6; r++) for (let c = 0; c < 7; c++) {
      if (at2(r, c) !== who) continue;
      for (const [dr, dc] of [[0, 1], [1, 0], [1, 1], [1, -1]]) {
        let k = 1; while (at2(r + dr * k, c + dc * k) === who) k++;
        if (k >= 4) return true;
      }
    }
    return false;
  }
  const TTT_LINES = [[0, 1, 2], [3, 4, 5], [6, 7, 8], [0, 3, 6], [1, 4, 7], [2, 5, 8], [0, 4, 8], [2, 4, 6]];
  const tttWin = (b, who) => TTT_LINES.some((l) => l.every((i) => b[i] === who));

  function c4Drop(b, col, who) {
    for (let r = 5; r >= 0; r--) if (b[col + r * 7] === 0) { b[col + r * 7] = who; return true; }
    return false;
  }

  // Draw a dots edge by global index; claim any newly completed boxes for `who`.
  function dotsEdge(d, n, who) {
    const hoff = (d.h + 1) * d.w;
    if (n < hoff) { if (d.hedges[n]) return -1; d.hedges[n] = 1; }
    else { const vi = n - hoff; if (d.vedges[vi]) return -1; d.vedges[vi] = 1; }
    let made = 0;
    for (let r = 0; r < d.h; r++) for (let c = 0; c < d.w; c++) {
      const bi = r * d.w + c;
      if (d.boxes[bi]) continue;
      const top = d.hedges[r * d.w + c], bot = d.hedges[(r + 1) * d.w + c];
      const left = d.vedges[r * (d.w + 1) + c], right = d.vedges[r * (d.w + 1) + c + 1];
      if (top && bot && left && right) { d.boxes[bi] = who; made++; }
    }
    if (made) { if (who === 1) d.sme += made; else d.sopp += made; }
    return made;
  }
  const dotsDone = (d) => d.hedges.every((x) => x) && d.vedges.every((x) => x);
  const dotsEdgeCount = (d) => (d.h + 1) * d.w + d.h * (d.w + 1);
  const dotsWinner = (d) => d.sme === d.sopp ? null : (d.sme > d.sopp ? ME : CATHY);

  function botMove() {
    const d = duel;
    if (!d || d.phase === "over" || d.turn !== d.opp) return;
    if (d.kind === "dots") {
      // Bot keeps drawing while it completes boxes (extra turns).
      let guard = 0;
      while (d.turn === d.opp && !dotsDone(d) && guard++ < 60) {
        const hoff = (d.h + 1) * d.w, free = [];
        for (let n = 0; n < dotsEdgeCount(d); n++)
          if (n < hoff ? !d.hedges[n] : !d.vedges[n - hoff]) free.push(n);
        if (!free.length) break;
        const made = dotsEdge(d, free[Math.floor(Math.random() * free.length)], 2);
        if (dotsDone(d)) return over(dotsWinner(d));
        if (!made) d.turn = ME;
      }
      sendDuel("playing");
      return;
    }
    if (d.kind === "c4") {
      let col; do { col = Math.floor(Math.random() * 7); } while (!c4Drop(d.board, col, 2) && d.board.some((x) => x === 0));
      if (c4Win(d.board, 2)) return over(CATHY);
    } else {
      const free = d.board.map((v, i) => v === 0 ? i : -1).filter((i) => i >= 0);
      if (free.length) d.board[free[Math.floor(Math.random() * free.length)]] = 2;
      if (tttWin(d.board, 2)) return over(CATHY);
    }
    if (d.board.every((x) => x !== 0)) return over(null);
    d.turn = ME;
    sendDuel("playing");
  }

  function over(winnerPid) {
    if (winnerPid === ME) scores[ME] += 1;
    else if (winnerPid === CATHY) scores[CATHY] += 1;
    duel.phase = "over";
    sendDuel("over", { result: resultFor(winnerPid) });
  }

  function playerMove(n) {
    const d = duel;
    if (!d || d.phase === "over" || d.turn !== ME) return;
    if (d.kind === "dots") {
      const made = dotsEdge(d, n, 1);
      if (made < 0) return;                      // already drawn
      if (dotsDone(d)) return over(dotsWinner(d));
      if (!made) { d.turn = d.opp; sendDuel("playing"); at(500, botMove); }
      else sendDuel("playing");                  // completed a box: go again
      return;
    }
    if (d.kind === "c4") {
      if (!c4Drop(d.board, n, 1)) return;
      if (c4Win(d.board, 1)) return over(ME);
    } else {
      if ((d.board[n] || 0) !== 0) return;
      d.board[n] = 1;
      if (tttWin(d.board, 1)) return over(ME);
    }
    if (d.board.every((x) => x !== 0)) return over(null);
    d.turn = d.opp;
    sendDuel("playing");
    at(500, botMove);
  }

  function startDuelMatch(kind, first) {
    duel = newDuel(kind, first);
    duel.phase = "playing";
    sendDuel("playing");
    if (first === CATHY) at(500, botMove);
  }

  // ---- draw & guess ---------------------------------------------------------
  // Two rounds with a per-round countdown (deadline + dur), then a final board
  // and `again` restart. Round 1 you draw; round 2 you guess.
  const DRAW_DUR = 20;   // seconds per round (dur is sent in seconds)
  let draw = null;
  function drawScore(p, pts) { scores[p] += pts; }
  function drawBoard() {
    return [1, 2, 3].map((p) => ({ pid: p, nick: nicks[p], avatar: avatars[p], score: scores[p] }))
      .sort((a, b) => b.score - a.score);
  }
  function runDraw() {
    lobby("draw");
    draw = { round: 1, rounds: 2, secret: "", done: false };
    at(300, drawRound);
  }
  function drawRound() {
    if (!draw) return;
    draw.done = false;
    const deadline = Date.now() + DRAW_DUR * 1000;
    if (draw.round === 1) {
      // You draw. A bot "guesses" correctly partway through the window.
      draw.secret = "BANANA";
      send({ t: "draw", phase: "draw", role: "drawer", word: draw.secret,
        round: draw.round, rounds: draw.rounds, drawer: ME, deadline, dur: DRAW_DUR, scores: sc() });
      at(7000, () => { if (draw && draw.round === 1 && !draw.done) { drawScore(CATHY, 50); drawReveal(CATHY); } });
    } else {
      // You guess. The bot scribbles; a wrong bot guess lands in chat.
      draw.secret = "ROCKET";
      send({ t: "draw", phase: "draw", role: "guesser", len: draw.secret.length,
        round: draw.round, rounds: draw.rounds, drawer: nicks[BOT], deadline, dur: DRAW_DUR, scores: sc() });
      at(600, () => send({ t: "ink", clear: true }));
      for (let i = 0; i < 8; i++)
        at(1000 + i * 150, ((k) => () => send({ t: "ink", x0: 0.5, y0: 0.2 + k * 0.06, x1: 0.5, y1: 0.26 + k * 0.06 }))(i));
      at(2500, () => send({ t: "chat", nick: nicks[CATHY], text: "banana?" }));
      // Fallback so the harness never stalls if you never guess.
      at(DRAW_DUR * 1000, () => { if (draw && draw.round === 2 && !draw.done) drawReveal(null); });
    }
  }
  function drawReveal(winner) {
    if (!draw) return;
    draw.done = true;
    send({ t: "draw", phase: "reveal", word: draw.secret, winner,
      round: draw.round, rounds: draw.rounds, scores: sc() });
    at(3000, () => {
      if (!draw) return;
      if (draw.round >= draw.rounds) { send({ t: "draw", phase: "final", board: drawBoard() }); }
      else { draw.round++; drawRound(); }
    });
  }

  // ---- pong -----------------------------------------------------------------
  let pong = null;
  function runPong() {
    pong = null;
    lobby("pong");
    at(300, () => send({ t: "pong", phase: "lobby", challenges: [{ from: CATHY, to: ME }] }));
  }
  function startPong() {
    pong = { p1: 0.5, p2: 0.5, s1: 0, s2: 0, bx: 0.5, by: 0.5, vx: 0.012, vy: 0.008, dir: 0 };
    loopId = setInterval(pongTick, 33);
  }
  function pongSend(phase, extra) {
    const p = pong;
    send(Object.assign({ t: "pong", phase, you: ME, me: 1, opp: nicks[CATHY],
      ball: { x: p.bx, y: p.by }, p1: p.p1, p2: p.p2, s1: p.s1, s2: p.s2 }, extra || {}));
  }
  function pongTick() {
    const p = pong; if (!p) return;
    p.p1 = Math.max(0.11, Math.min(0.89, p.p1 + p.dir * 0.03));   // your input
    p.p2 += Math.max(-0.02, Math.min(0.02, p.by - p.p2));         // bot tracks ball
    p.p2 = Math.max(0.11, Math.min(0.89, p.p2));
    p.bx += p.vx; p.by += p.vy;
    if (p.by < 0.02) { p.by = 0.02; p.vy = -p.vy; }
    if (p.by > 0.98) { p.by = 0.98; p.vy = -p.vy; }
    if (p.bx < 0.03) {
      if (Math.abs(p.by - p.p1) < 0.14) { p.bx = 0.03; p.vx = Math.abs(p.vx); }
      else { p.s2++; return pongPoint(); }
    }
    if (p.bx > 0.97) {
      if (Math.abs(p.by - p.p2) < 0.14) { p.bx = 0.97; p.vx = -Math.abs(p.vx); }
      else { p.s1++; return pongPoint(); }
    }
    pongSend("playing");
  }
  function pongPoint() {
    const p = pong;
    if (p.s1 >= 3 || p.s2 >= 3) {
      clearInterval(loopId); loopId = 0;
      if (p.s1 > p.s2) scores[ME] += 1; else scores[CATHY] += 1;
      pongSend("over", { result: p.s1 > p.s2 ? "win" : "lose" });
      pong = null;
      return;
    }
    p.bx = 0.5; p.by = 0.5;
    p.vx = (Math.random() < 0.5 ? -1 : 1) * 0.012;
    p.vy = (Math.random() < 0.5 ? -1 : 1) * 0.008;
    pongSend("playing");
  }

  // ---- input ----------------------------------------------------------------
  socket.on("data", (buf) => {
    const msg = decode(buf);
    if (msg == null) return;
    let m; try { m = JSON.parse(msg); } catch { return; }
    const cur = curStage();

    if (m.t === "hello") {
      clearTimers();               // avoid overlapping rotations on a re-hello
      nicks[ME] = m.nick || "You";
      send({ t: "welcome", pid: ME, nick: nicks[ME] });
      lobby("none");
      if (process.env.LOBBY) {
        // Sit in the app lobby so lobby chat can be tested; a couple of bots
        // say hi. No game auto-starts in this mode.
        at(700, () => send({ t: "chat", nick: nicks[CATHY], text: "hey, ready to play?" }));
        at(1600, () => send({ t: "chat", nick: nicks[BOT], text: "lets go!" }));
      } else {
        at(1000, startStage);
      }
      return;
    }
    if (m.t === "ping") { send({ t: "pong" }); return; }
    if (m.t === "leaveGame") { nextStage(); return; }
    // Lobby chat: broadcast the sender's line back (mock has one real client).
    if (m.t === "say" && typeof m.text === "string") {
      const text = m.text.trim().slice(0, 100);
      if (text) send({ t: "chat", nick: nicks[ME], text });
      return;
    }

    if (cur === "trivia") {
      if (!tv) return;
      if (m.t === "ready") {
        tv.ready[ME] = !!m.ready;
        if (!m.ready && tv.phase === "countdown") { tv.phase = "lobby"; sendTvLobby(); }
        else if (tv.phase === "lobby") { sendTvLobby(); checkStart(); }
      } else if (m.t === "vote" && typeof m.topic === "number") {
        tv.votes[ME] = m.topic;
        if (tv.phase === "lobby") sendTvLobby();
      } else if (m.t === "answer" && typeof m.c === "number") {
        if (tv.phase === "question" && !(ME in tv.answers)) { tv.answers[ME] = m.c; sendQuestion(); }
      } else if (m.t === "again") {
        clearTimers();
        runTrivia();     // fresh lobby, scores reset
      }
      return;
    }
    if (cur === "c4" || cur === "ttt" || cur === "dots") {
      if (m.t === "accept" || m.t === "challenge") startDuelMatch(cur, ME);
      else if (m.t === "cancel") send({ t: "duel", kind: cur, phase: "lobby", you: ME, me: 1, opp: nicks[CATHY], challenges: [] });
      else if (m.t === "move" && typeof m.n === "number") playerMove(m.n);
      else if (m.t === "rematch" && duel) startDuelMatch(cur, duel.first === ME ? CATHY : ME);
      return;
    }
    if (cur === "draw") {
      if (!draw) return;
      if (m.t === "guess" && draw.round === 2 && !draw.done) {
        const text = String(m.text || "").trim();
        if (text.toUpperCase() === draw.secret) { drawScore(ME, 60); drawReveal(ME); }
        else if (text) send({ t: "chat", nick: nicks[ME], text });  // wrong -> chat (once)
      } else if (m.t === "again") {
        clearTimers();
        runDraw();       // replay from round 1
      }
      // stroke/clear from round 1 are silent (no other clients in the mock).
      return;
    }
    if (cur === "pong") {
      if (m.t === "accept" || m.t === "challenge") startPong();
      else if (m.t === "cancel") send({ t: "pong", phase: "lobby", challenges: [] });
      else if (m.t === "paddle" && pong) pong.dir = m.dir | 0;
      return;
    }
  });
  socket.on("close", clearTimers);
  socket.on("error", () => {});
}

// Minimal single-frame text decoder (client frames are always masked).
function decode(buf) {
  if (buf.length < 2) return null;
  const op = buf[0] & 0x0f;
  if (op === 0x8) return null; // close
  let len = buf[1] & 0x7f, off = 2;
  if (len === 126) { len = buf.readUInt16BE(2); off = 4; }
  else if (len === 127) { len = Number(buf.readBigUInt64BE(2)); off = 10; }
  const mask = buf.slice(off, off + 4); off += 4;
  const out = Buffer.alloc(len);
  for (let i = 0; i < len; i++) out[i] = buf[off + i] ^ mask[i & 3];
  return out.toString("utf8");
}

server.listen(PORT, () => console.log("Mock server: http://localhost:" + PORT));
