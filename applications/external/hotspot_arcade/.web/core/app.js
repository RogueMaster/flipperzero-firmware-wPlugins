/* Hotspot Arcade client core. Shares state through the global `A` so the game
   modules (trivia.js, connect4.js) can plug in without a bundler. */
"use strict";
var A = {
  ws: null,
  pid: null,
  nick: "",
  avatar: "🙂",   // emoji avatar, picked on the landing screen
  view: "landing",     // landing | lobby | trivia | duel | draw | pong
  joined: false,       // true once the user committed a nick (Play) or is rejoining
  offset: 0,           // serverMillis - localNow, learned from first timed msg
  offsetSet: false,
  retry: 0,
  handlers: {},        // messageType -> fn(msg), filled by game modules
};

var $ = function (id) { return document.getElementById(id); };
function show(id) { $(id).classList.remove("hide"); }
function hide(id) { $(id).classList.add("hide"); }
function esc(s) {
  return String(s == null ? "" : s).replace(/[&<>"]/g, function (c) {
    return { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c];
  });
}

/* Which lobby `game` string maps to which top-level screen, and its title.
   The three duels share the single "duel" screen; the duel message's `kind`
   drives the actual board. */
var SCREENS = ["landing", "lobby", "trivia", "duel", "draw", "pong", "wyr", "scramble", "react"];
var GAME_SCREEN = {
  trivia: "trivia", connect4: "duel", tictactoe: "duel", dots: "duel",
  reversi: "duel", draw: "draw", pong: "pong",
  wyr: "wyr", scramble: "scramble", react: "react",
};
var GAME_LABEL = {
  trivia: "Trivia", connect4: "Connect 4", tictactoe: "Tic-Tac-Toe",
  dots: "Dots & Boxes", reversi: "Reversi", draw: "Draw & Guess", pong: "Pong",
  wyr: "Would You Rather", scramble: "Word Scramble", react: "Reaction Duel",
};

/* Show exactly one top-level screen. */
function screen(name) {
  A.view = name;
  SCREENS.forEach(function (s) {
    $(s).classList.toggle("hide", s !== name);
  });
  // Reactions are available once you've joined; hidden on the landing screen.
  var rb = $("react-bar");
  if (rb) {
    rb.classList.toggle("hide", name === "landing");
    rb.classList.remove("open");
  }
  // The shared leaderboard only belongs to the group-score games, which manage
  // it themselves; anywhere else, drop it so it never lingers over a lobby.
  if (name !== "trivia" && name !== "scramble" && name !== "react") A.hideLead();
}

/* Auto-route driven by server state. Ignored until the user has joined so we
   never yank someone off the landing screen before they pick a nickname. */
function route(name) { if (A.joined) screen(name); }

/* Cosmetic clock aligned to the server. deadline - serverNow() = ms remaining. */
function serverNow() { return Date.now() + A.offset; }
// The ESP sends deadlines in ms but window durations in seconds; normalise a
// duration to ms so a bare seconds value (e.g. 20) and a ms value (20000) both work.
A.durMs = function (dur) { return dur ? (dur < 1000 ? dur * 1000 : dur) : 0; };
function noteDeadline(deadline, dur) {
  // Estimate the offset once from the first timed message so the countdown
  // matches the server. dur is the full window length (seconds or ms).
  dur = A.durMs(dur);
  if (!A.offsetSet && deadline && dur) {
    A.offset = (deadline - dur) - Date.now();
    A.offsetSet = true;
  }
}

var toastTimer;
function toast(msg) {
  var el = $("toast");
  el.textContent = msg;
  el.classList.remove("hide");
  clearTimeout(toastTimer);
  toastTimer = setTimeout(function () { el.classList.add("hide"); }, 2600);
}

/* Append one chat line to a log, capping length to keep the DOM small. */
function chatAppend(logId, nick, text, cls) {
  var log = $(logId);
  if (!log) return;
  var div = document.createElement("div");
  div.className = "cmsg" + (cls ? " " + cls : "");
  div.innerHTML = "<b>" + esc(nick) + "</b> " + esc(text);
  log.appendChild(div);
  while (log.children.length > 40) log.removeChild(log.firstChild);
  log.scrollTop = log.scrollHeight;
}

/* Shared chat handler for the lobby and Draw & Guess. The server echoes your
   own message back, so we never render locally (avoids duplicates). */
A.handlers.chat = function (m) {
  chatAppend(A.view === "draw" ? "draw-chat" : "lobby-chat", m.nick, m.text);
};

/* Shared collapsible live leaderboard, pinned under the header. Any group-score
   game (trivia / scramble / reaction) calls A.showLead(board) each state update;
   A.hideLead() removes it (lobbies, countdowns, finals). `board` is a list of
   {pid,nick,avatar,score}; we sort defensively. Collapsed by default. */
A.leadOpen = false;
A.leadBoard = null;
function noMotionPref() {
  return window.matchMedia && matchMedia("(prefers-reduced-motion:reduce)").matches;
}
A.showLead = function (board, bump) {
  var b = (board || []).slice().sort(function (x, y) { return (y.score || 0) - (x.score || 0); });
  A.leadBoard = b;
  if (!b.length) { hide("lead"); return; }
  show("lead");
  var top = b[0];
  $("lead-sum").innerHTML = "<b>1.</b> " + esc(top.avatar || "") + " " + esc(top.nick) + "  " + (top.score || 0);
  var ol = $("lead-full");
  ol.innerHTML = "";
  b.forEach(function (p, i) {
    var li = document.createElement("li");
    if (p.pid === A.pid) li.className = "self";
    li.innerHTML =
      '<span class="r">' + (i + 1) + "</span>" +
      '<span class="av">' + esc(p.avatar || "🙂") + "</span>" +
      '<span class="pn">' + esc(p.nick) + "</span>" +
      '<span class="ps">' + (p.score || 0) + "</span>";
    ol.appendChild(li);
  });
  ol.classList.toggle("hide", !A.leadOpen);
  $("lead-caret").innerHTML = A.leadOpen ? "&#9650;" : "&#9660;";
  $("lead-bar").setAttribute("aria-expanded", A.leadOpen ? "true" : "false");
  if (bump && !noMotionPref()) {
    var el = $("lead");
    el.classList.remove("bump"); void el.offsetWidth; el.classList.add("bump");
  }
};
A.hideLead = function () { hide("lead"); };

/* ---- shared game-UI components -------------------------------------------
   The whole-group games (trivia / would-you-rather / scramble / reaction) all
   share the same lobby-with-ready, countdown, timer bar, and final podium. These
   render helpers take element ids so each game reuses one implementation. */

// Sort a score list high-to-low (defensive; the ESP already ranks).
A.ranked = function (list) {
  return (list || []).slice().sort(function (a, b) { return (b.score || 0) - (a.score || 0); });
};

// Ready-up lobby: player list (avatar + nick + ready flag) and the ready CTA.
// Returns whether *you* are ready, for the caller's toggle intent.
A.readyLobby = function (cfg) {
  if (cfg.meId) $(cfg.meId).textContent = A.nick ? "You: " + A.nick : "";
  var ul = $(cfg.listId);
  ul.innerHTML = "";
  var mine = false;
  (cfg.players || []).forEach(function (p) {
    var li = document.createElement("li");
    if (p.pid === A.pid) { li.className = "self"; mine = !!p.ready; }
    li.innerHTML =
      '<span class="av">' + esc(p.avatar || "🙂") + "</span>" +
      '<span class="pn">' + esc(p.nick) + "</span>" +
      '<span class="rdy' + (p.ready ? " on" : "") + '">' + (p.ready ? "Ready" : "...") + "</span>";
    ul.appendChild(li);
  });
  var rb = $(cfg.readyId);
  rb.textContent = mine ? "Ready. Tap to cancel" : "I'm ready";
  rb.classList.toggle("on", mine);
  return mine;
};

// Countdown number with a per-second tick + pop animation.
A.countdown = function (numId, sec) {
  var n = $(numId);
  n.textContent = sec;
  A.sfx("tick"); A.vibe(10);
  if (!noMotionPref()) { n.classList.remove("pop"); void n.offsetWidth; n.classList.add("pop"); }
};

// Final podium (avatar + rank + score). Returns the ranked list so the caller
// can fire a win/lose cue.
A.podium = function (listId, board) {
  var b = A.ranked(board);
  var ol = $(listId);
  ol.innerHTML = "";
  b.forEach(function (p, i) {
    var li = document.createElement("li");
    li.className = "place p" + (i + 1) + (p.pid === A.pid ? " self" : "");
    li.innerHTML =
      '<span class="rank">' + (i + 1) + "</span>" +
      '<span class="av">' + esc(p.avatar || "🙂") + "</span>" +
      '<span class="pn">' + esc(p.nick) + "</span>" +
      '<span class="ps">' + (p.score || 0) + "</span>";
    ol.appendChild(li);
  });
  return b;
};

// Shared countdown timer bar (used by trivia + scramble). Per-bar state lives on
// the element. `ticks` plays blips at 3/2/1s. Call A.timebarStop to freeze it.
A.timebar = function (barId, deadline, dur, ticks) {
  dur = A.durMs(dur);
  var bar = $(barId), fill = bar.firstElementChild;
  A.timebarStop(barId);
  show(barId);
  var st = bar._ha || (bar._ha = { anim: null, ticks: [] });
  var remain = deadline - serverNow();
  bar.classList.remove("hot");
  if (remain <= 0 || !dur) {
    fill.style.transition = "none"; fill.style.transform = "scaleX(0)"; bar.classList.add("hot"); return;
  }
  var frac = Math.max(0, Math.min(1, remain / dur));
  fill.style.transition = "none";
  fill.style.transform = "scaleX(" + frac + ")";
  requestAnimationFrame(function () {
    fill.style.transition = "transform " + remain + "ms linear";
    fill.style.transform = "scaleX(0)";
  });
  if (remain <= 3000) bar.classList.add("hot");
  else st.anim = setTimeout(function () { bar.classList.add("hot"); }, remain - 3000);
  if (ticks) [3000, 2000, 1000].forEach(function (t) {
    if (remain > t) st.ticks.push(setTimeout(function () { A.sfx("tick"); }, remain - t));
  });
};
A.timebarStop = function (barId) {
  var bar = $(barId), st = bar._ha;
  if (st) { if (st.anim) { clearTimeout(st.anim); st.anim = null; } st.ticks.forEach(clearTimeout); st.ticks = []; }
};

function send(obj) {
  if (A.ws && A.ws.readyState === 1) A.ws.send(JSON.stringify(obj));
}

/* Header status dot: "" connected (orange), "warn" reconnecting, "bad" down. */
function setDot(cls) { $("dot").className = "dot" + (cls ? " " + cls : ""); }
function setNick() { $("hdr-nick").textContent = A.nick || ""; }

/* WebSocket URL derives from the host so a local mock server also works.
   On the ESP this resolves to ws://192.168.4.1/ws. */
function wsUrl() {
  var proto = location.protocol === "https:" ? "wss:" : "ws:";
  var host = location.host || "192.168.4.1";
  return proto + "//" + host + "/ws";
}

function connect() {
  var ws;
  try { ws = new WebSocket(wsUrl()); }
  catch (e) { scheduleReconnect(); return; }
  A.ws = ws;

  ws.onopen = function () {
    A.retry = 0;
    hide("netbar");
    setDot("");           // connected
    // Auto (re)join only if we already have a nickname. A first-time visitor
    // stays on the landing screen until they press Play.
    if (A.joined) send({ t: "hello", nick: A.nick, avatar: A.avatar });
  };

  ws.onmessage = function (ev) {
    var m;
    try { m = JSON.parse(ev.data); } catch (e) { return; }
    dispatch(m);
  };

  ws.onclose = function () { scheduleReconnect(); };
  ws.onerror = function () { try { ws.close(); } catch (e) {} };
}

function scheduleReconnect() {
  setDot(A.retry > 4 ? "bad" : "warn");   // down after repeated failures
  if (A.view !== "landing") {
    $("netbar").textContent = "Reconnecting...";
    show("netbar");
  }
  var wait = Math.min(1000 * Math.pow(1.6, A.retry), 8000);
  A.retry++;
  setTimeout(connect, wait);
}

/* Core message routing. Game-specific messages hand off to registered
   handlers so the modules stay self-contained. */
function dispatch(m) {
  switch (m.t) {
    case "welcome":
      A.pid = m.pid;
      if (m.nick) { A.nick = m.nick; setNick(); }
      break;
    case "lobby":
      onLobby(m);
      break;
    case "toast":
      toast(m.msg);
      break;
    case "pong":
      // "pong" is overloaded: a bare {t:"pong"} is the keepalive reply, while
      // the Pong game sends {t:"pong", phase, ...}. Only the latter has a phase.
      if (m.phase && A.handlers.pong) A.handlers.pong(m);
      break;
    default:
      if (A.handlers[m.t]) A.handlers[m.t](m);
  }
}

/* Look up a player's nick from the last lobby snapshot. */
function nickOf(pid) {
  var p = (A.players || []).find(function (x) { return x.pid === pid; });
  return p ? p.nick : "Player";
}

/* Shared 1v1 challenge lobby, used by both the duel screen and pong. Renders
   incoming/outgoing challenges into `incEl` and the challengeable-player list
   into `listEl`; all three intents (challenge/accept/cancel) are generic. */
function lobbyView(incEl, listEl, challenges) {
  challenges = challenges || [];
  function btn(parent, label, cls, fn) {
    var b = document.createElement("button");
    b.className = cls; b.textContent = label;
    b.addEventListener("click", fn);
    parent.appendChild(b);
    return b;
  }

  incEl.innerHTML = "";
  challenges.forEach(function (c) {
    var row = document.createElement("div");
    row.className = "challenge";
    if (c.to === A.pid) {
      row.innerHTML = '<span class="pn">' + esc(nickOf(c.from)) + " challenged you</span>";
      btn(row, "Accept", "btn sm", function () { A.sfx("buzz"); send({ t: "accept", from: c.from }); });
      btn(row, "Decline", "btn ghost sm", function () { send({ t: "cancel" }); });
    } else if (c.from === A.pid) {
      row.innerHTML = '<span class="pn">Waiting on ' + esc(nickOf(c.to)) + "...</span>";
      btn(row, "Cancel", "btn ghost sm", function () { send({ t: "cancel" }); });
    } else { return; }
    incEl.appendChild(row);
  });

  var iChallenged = challenges.some(function (c) { return c.from === A.pid; });
  var others = (A.players || []).filter(function (p) { return p.pid !== A.pid; });
  listEl.innerHTML = "";
  if (!others.length) {
    var empty = document.createElement("li");
    empty.innerHTML = '<span class="pn" style="color:var(--text-dim)">No other players yet.</span>';
    listEl.appendChild(empty);
  }
  others.forEach(function (p) {
    var li = document.createElement("li");
    li.innerHTML = '<span class="dot"></span><span class="pn">' + esc(p.nick) + "</span>";
    var pending = challenges.some(function (c) {
      return (c.from === A.pid && c.to === p.pid) || (c.from === p.pid && c.to === A.pid);
    });
    var b = btn(li, pending ? "Pending" : "Challenge", "btn sm", function () {
      A.sfx("buzz"); send({ t: "challenge", to: p.pid });
    });
    if (pending || iChallenged) b.disabled = true;
    listEl.appendChild(li);
  });
}
A.lobbyView = lobbyView;

/* Lobby: player list + which game the host picked. Routes into the active
   game view; a game message can also switch us in (see game modules). */
function onLobby(m) {
  if (m.me) A.pid = m.me;
  var prevCount = (A.players || []).length;
  A.players = m.players || [];   // kept for the duel/pong challenge lists
  // A new arrival (after we ourselves joined) gets a little blip.
  if (A.joined && A.players.length > prevCount && prevCount > 0) { A.sfx("join"); A.vibe(20); }
  $("lobby-me").textContent = A.nick ? "You: " + A.nick : "";

  var list = $("players");
  list.innerHTML = "";
  A.players.forEach(function (p) {
    var li = document.createElement("li");
    if (p.pid === A.pid) li.className = "self";
    li.innerHTML =
      '<span class="av">' + esc(p.avatar || "🙂") + "</span>" +
      '<span class="pn">' + esc(p.nick) + "</span>" +
      '<span class="ps">' + (p.score || 0) + "</span>";
    list.appendChild(li);
  });

  var g = m.game || "none";
  var gs = $("lobby-game");
  gs.textContent = g === "none"
    ? "Waiting for the host to pick a game."
    : (GAME_LABEL[g] || g) + " starting...";

  // If the host went back to the plain lobby, leave any game screen. Otherwise
  // show the shell of the chosen game; the game message fills in details.
  if (g === "none" && A.view !== "landing") route("lobby");
  else if (g !== "none" && A.view === "lobby" && GAME_SCREEN[g]) route(GAME_SCREEN[g]);
}

/* Landing flow */
function startPlay() {
  var n = $("nick").value.trim().slice(0, 12);
  if (!n) { $("nick").focus(); return; }
  A.nick = n;
  A.joined = true;
  setNick();
  A.initAudio();          // first gesture: unlock audio for the session
  A.sfx("start"); A.vibe(30);
  try { localStorage.setItem("ha_nick", n); localStorage.setItem("ha_avatar", A.avatar); } catch (e) {}
  send({ t: "hello", nick: n, avatar: A.avatar });
  screen("lobby");
}

/* Look up a player's avatar from the last lobby snapshot. */
function avatarOf(pid) {
  var p = (A.players || []).find(function (x) { return x.pid === pid; });
  return p ? (p.avatar || "🙂") : "🙂";
}
A.avatarOf = avatarOf;

// ---- avatar picker (landing) --------------------------------------------
var AVATARS = ["🙂", "😎", "🤖", "👾", "🐱", "🐶", "🦊", "🐸",
               "🦄", "🐙", "🐼", "🐯", "🍕", "🌮", "👻", "🚀"];
function buildAvatarPicker() {
  var wrap = $("avatars");
  if (!wrap) return;
  wrap.innerHTML = "";
  AVATARS.forEach(function (em) {
    var b = document.createElement("button");
    b.type = "button";
    b.className = "av-pick" + (em === A.avatar ? " on" : "");
    b.textContent = em;
    b.addEventListener("click", function () {
      A.avatar = em;
      A.sfx("tick"); A.vibe(10);
      Array.prototype.forEach.call(wrap.children, function (c) {
        c.classList.toggle("on", c.textContent === em);
      });
    });
    wrap.appendChild(b);
  });
}

// ---- emoji reactions (float up, in any screen) --------------------------
var REACTS = ["👍", "😂", "🔥", "😮", "🎉", "❤️"];
function buildReactBar() {
  var row = $("react-row");
  if (!row) return;
  row.innerHTML = "";
  REACTS.forEach(function (em) {
    var b = document.createElement("button");
    b.type = "button";
    b.className = "rx";
    b.textContent = em;
    b.addEventListener("click", function () {
      send({ t: "react", emoji: em });
      A.sfx("tick"); A.vibe(10);
      $("react-bar").classList.remove("open");
    });
    row.appendChild(b);
  });
}
// Float one emoji from the bottom; server echoes our own back so we don't
// render locally (keeps every phone in sync, avoids a double).
function floatReact(emoji, nick) {
  var layer = $("react-layer");
  if (!layer) return;
  var el = document.createElement("div");
  el.className = "float";
  el.textContent = emoji;
  el.style.left = (8 + Math.random() * 78) + "vw";
  layer.appendChild(el);
  setTimeout(function () { if (el.parentNode) el.parentNode.removeChild(el); }, 2200);
}
A.handlers.emoji = function (m) {
  floatReact(m.emoji, m.nick);
  if (m.pid !== A.pid) A.vibe(8);
};

function initApp() {
  var saved = "";
  try {
    saved = localStorage.getItem("ha_nick") || "";
    A.avatar = localStorage.getItem("ha_avatar") || A.avatar;
  } catch (e) {}
  A.nick = saved;
  A.joined = !!saved;   // returning player: rejoin automatically on connect
  $("nick").value = saved;
  setNick();
  setDot("warn");       // not connected yet
  buildAvatarPicker();
  buildReactBar();

  // Reactions FAB: tap to reveal the emoji row; the bar is hidden on landing.
  $("react-fab").addEventListener("click", function () {
    $("react-bar").classList.toggle("open");
  });

  // Shared leaderboard: toggle the collapsible list; re-render from last board.
  $("lead-bar").addEventListener("click", function () {
    A.leadOpen = !A.leadOpen;
    A.sfx("tick");
    if (A.leadBoard) A.showLead(A.leadBoard, false);
  });

  $("play").addEventListener("click", startPlay);
  $("nick").addEventListener("keydown", function (e) {
    if (e.key === "Enter") startPlay();
  });

  // Lobby chat: emit {t:"say"}; the server broadcasts it back as {t:"chat"}.
  $("lobby-form").addEventListener("submit", function (e) {
    e.preventDefault();
    var inp = $("lobby-input");
    var text = inp.value.trim().slice(0, 100);
    if (!text) return;
    A.sfx("buzz"); A.vibe(12);
    send({ t: "say", text: text });
    inp.value = "";
  });

  connect();
  // Keepalive; also nudges the server to resend state after a doze.
  setInterval(function () { send({ t: "ping" }); }, 20000);
}

document.addEventListener("DOMContentLoaded", initApp);
