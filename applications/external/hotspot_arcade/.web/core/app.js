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

// Shared pack-vote strip for the party games (Would You Rather, Word Scramble).
// Mirrors trivia's own inline topic-vote block (see trivia.js renderLobby),
// just generalized: `cfg` names the container element, the packs list
// ({name,votes}), the caller's current vote index, and a send callback, so
// every pack-driven game votes the same way trivia already votes topics.
A.packVote = function (cfg) {
  var box = $(cfg.boxId);
  if (!box) return;
  box.innerHTML = "";
  (cfg.packs || []).forEach(function (p, i) {
    var b = document.createElement("button");
    b.className = "topic" + (cfg.myvote === i ? " mine" : "");
    // Pack names are committed content, not player input, but esc() anyway --
    // cheap, and it keeps "never build innerHTML from an unescaped string" uniform.
    b.innerHTML =
      '<span class="txt">' + esc(p.name) + "</span>" +
      '<span class="votes">' + (p.votes || 0) + "</span>";
    b.addEventListener("click", function () {
      A.sfx("buzz"); A.vibe(12);
      cfg.onVote(i);
    });
    box.appendChild(b);
  });
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
function setNick() {
  var el = $("hdr-nick");
  if (!el) return;
  // Show avatar + name, and only reveal (and enable tapping) once the player has one.
  el.textContent = A.nick ? (A.avatar + " " + A.nick) : "";
  el.classList.toggle("hide", !A.nick);
}

/* WebSocket URL derives from the host so a local mock server also works.
   On the ESP this resolves to ws://192.168.4.1/ws. */
function wsUrl() {
  var proto = location.protocol === "https:" ? "wss:" : "ws:";
  var host = location.host || "192.168.4.1";
  return proto + "//" + host + "/ws";
}

/* The local simulator (sim/) runs the real ESP engine in WASM inside the parent
   page, where there is no server to open a socket against. With "?harness=<id>"
   the client asks the parent for a duck-typed socket instead. The client pulls
   rather than the parent pushing, so this can't depend on script ordering.
   Dead code in production: no query param and no parent harness, no change. */
function harnessSocket() {
  var q = new URLSearchParams(location.search);
  if (!q.has("harness")) return null;
  if (window.parent === window || !window.parent.HA_HARNESS) return null;
  return window.parent.HA_HARNESS.connect(q.get("harness"));
}

/* Saved-identity key. Real phones are separate devices with separate storage, but
   the simulator's panels are iframes on one origin sharing one localStorage, so
   without this every panel would rejoin as whoever saved last. Namespacing by the
   harness id also makes "returning player auto-rejoins" testable per panel, which
   it otherwise isn't. Unchanged in production: no "?harness=", no suffix. */
function storeKey(name) {
  var h = new URLSearchParams(location.search).get("harness");
  return h ? name + "_" + h : name;
}

function connect() {
  var ws = harnessSocket();
  if (!ws) {
    try { ws = new WebSocket(wsUrl()); }
    catch (e) { scheduleReconnect(); return; }
  }
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
  maybeCaptive();
  setTimeout(connect, wait);
}

/* Captive-browser handoff. iOS/Android open a Wi-Fi "captive" mini-browser that
   can't hold a WebSocket, so the socket just loops on reconnect. We only surface
   the handoff overlay when we're NOT already in the real browser: in the mini
   browser the page host is a probe domain (captive.apple.com, connectivitycheck…),
   while a real browser is at the AP's 192.168.4.1. Never fires under the simulator
   harness (its socket connects fine). */
var captiveSnoozeUntil = 0;   // don't re-show until A.retry passes this (after a dismiss)
function captiveEligible() {
  if (new URLSearchParams(location.search).has("harness")) return false;
  var h = location.hostname;
  return !!h && h !== "192.168.4.1";
}
function maybeCaptive() {
  // A couple of failed reconnects in a captive context means the socket won't hold.
  if (!captiveEligible() || A.retry < 2 || A.retry < captiveSnoozeUntil) return;
  show("captive");
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
  return p ? p.nick : "PLAYER";
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
  // Exclude yourself and anyone currently in a 1v1 match (playing or still on their
  // over screen) — they can't be challenged until they come back to the lobby.
  var others = (A.players || []).filter(function (p) { return p.pid !== A.pid && !p.busy; });
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
  // Uppercase to match how every name is shown (the ESP does the same on hello,
  // so this is just to avoid a flash of the typed casing before the echo lands).
  var n = $("nick").value.trim().slice(0, 12).toUpperCase();
  // Nobody should be stuck on the landing screen because they didn't want to think
  // of a name. Give them one, and an avatar to match, rather than blocking on the
  // empty field.
  if (!n) {
    n = randomNick();
    A.avatar = AVATARS[Math.floor(Math.random() * AVATARS.length)];
    $("nick").value = n;
    buildAvatarPicker();
  }
  A.nick = n;
  A.joined = true;
  setNick();
  A.initAudio();          // first gesture: unlock audio for the session
  A.sfx("start"); A.vibe(30);
  try { localStorage.setItem(storeKey("ha_nick"), n); localStorage.setItem(storeKey("ha_avatar"), A.avatar); } catch (e) {}
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

/* Fallback nickname for players who hit Play with the field empty. Kept to 12
   chars (the input's maxlength) and picked from short words that read well in a
   leaderboard row. The number keeps two silent players from colliding. */
var NICK_WORDS = ["NOVA", "PIXEL", "TURBO", "GHOST", "MANGO", "COMET", "NINJA",
                  "VOLT", "ZEBRA", "ROCKET", "TIGER", "DISCO", "FUZZY", "BANDIT"];
function randomNick() {
  var w = NICK_WORDS[Math.floor(Math.random() * NICK_WORDS.length)];
  return w + (10 + Math.floor(Math.random() * 90));
}
/* Render the emoji grid into `wrap`, highlighting `current` and calling onPick(em)
   on each tap. Shared by the landing picker and the header identity editor. */
function renderAvatarPicker(wrap, current, onPick) {
  if (!wrap) return;
  wrap.innerHTML = "";
  AVATARS.forEach(function (em) {
    var b = document.createElement("button");
    b.type = "button";
    b.className = "av-pick" + (em === current ? " on" : "");
    b.textContent = em;
    b.addEventListener("click", function () {
      A.sfx("tick"); A.vibe(10);
      Array.prototype.forEach.call(wrap.children, function (c) {
        c.classList.toggle("on", c.textContent === em);
      });
      onPick(em);
    });
    wrap.appendChild(b);
  });
}
// Landing picker: commits straight to A.avatar as before.
function buildAvatarPicker() {
  renderAvatarPicker($("avatars"), A.avatar, function (em) { A.avatar = em; });
}

/* Header identity editor. Tap the header name to change nickname/avatar mid-game;
   Save re-sends `hello` (the ESP updates the player in place) and persists locally.
   Edits are held in temporaries so Cancel changes nothing. */
var editAvatar = "";
function openIdEdit() {
  if (!A.joined) return;
  editAvatar = A.avatar;
  $("id-nick").value = A.nick || "";
  renderAvatarPicker($("id-avatars"), A.avatar, function (em) { editAvatar = em; });
  show("id-edit");
  $("id-nick").focus();
}
function closeIdEdit() { hide("id-edit"); }
function saveIdEdit() {
  var n = $("id-nick").value.trim().slice(0, 12).toUpperCase();
  if (!n) { $("id-nick").focus(); return; }   // keep the old name rather than blanking it
  A.nick = n;
  A.avatar = editAvatar || A.avatar;
  setNick();
  A.sfx("start"); A.vibe(20);
  try { localStorage.setItem(storeKey("ha_nick"), n); localStorage.setItem(storeKey("ha_avatar"), A.avatar); } catch (e) {}
  send({ t: "hello", nick: n, avatar: A.avatar });
  closeIdEdit();
}

// ---- emoji reactions (float up, in any screen) --------------------------
var REACTS = ["👍", "😂", "🔥", "😮", "🎉", "❤️", "😠"];
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
function floatReact(emoji, nick, avatar) {
  var layer = $("react-layer");
  if (!layer) return;
  var el = document.createElement("div");
  el.className = "float";
  // Nicknames are player-typed, so these go in as text nodes. Never build this
  // row by concatenating innerHTML.
  var who = document.createElement("span");
  who.className = "who";
  who.textContent = (avatar || "") + " " + (nick || "");
  var em = document.createElement("span");
  em.className = "em";
  em.textContent = emoji;
  el.appendChild(who);
  el.appendChild(em);
  // Keep the whole pill on screen: it is wider than a bare glyph was.
  el.style.left = (6 + Math.random() * 40) + "vw";
  layer.appendChild(el);
  setTimeout(function () { if (el.parentNode) el.parentNode.removeChild(el); }, 2200);
}
A.handlers.emoji = function (m) {
  floatReact(m.emoji, m.nick, m.avatar);
  if (m.pid !== A.pid) A.vibe(8);
};

function initApp() {
  var saved = "";
  try {
    saved = (localStorage.getItem(storeKey("ha_nick")) || "").toUpperCase();
    A.avatar = localStorage.getItem(storeKey("ha_avatar")) || A.avatar;
  } catch (e) {}
  A.nick = saved;
  A.joined = !!saved;   // returning player: rejoin automatically on connect
  $("nick").value = saved;
  setNick();
  setDot("warn");       // not connected yet
  buildAvatarPicker();
  buildReactBar();

  // Header identity: tap your name to change nickname/avatar mid-game.
  $("hdr-nick").addEventListener("click", openIdEdit);
  $("id-save").addEventListener("click", saveIdEdit);
  $("id-cancel").addEventListener("click", closeIdEdit);
  // Tap the dimmed backdrop (outside the card) to dismiss.
  $("id-edit").addEventListener("click", function (e) { if (e.target === $("id-edit")) closeIdEdit(); });

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

  // Captive handoff overlay: copy the address (execCommand works on the captive
  // http page where the async clipboard API is blocked), or dismiss to peek at the
  // lobby — it reappears if the socket keeps dropping.
  $("captive-copy").addEventListener("click", function () {
    var inp = $("captive-url");
    inp.focus(); inp.select();
    try { inp.setSelectionRange(0, inp.value.length); } catch (e) {}
    var ok = false;
    try { ok = document.execCommand("copy"); } catch (e) {}
    if (!ok && navigator.clipboard) {
      try { navigator.clipboard.writeText(inp.value); ok = true; } catch (e) {}
    }
    var btn = $("captive-copy"), was = btn.getAttribute("data-label") || btn.textContent;
    btn.setAttribute("data-label", was);
    btn.textContent = ok ? "Copied!" : "Select & copy";
    setTimeout(function () { btn.textContent = was; }, 1600);
  });
  $("captive-dismiss").addEventListener("click", function () {
    hide("captive");
    captiveSnoozeUntil = A.retry + 4;   // re-show only if it keeps dropping
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
