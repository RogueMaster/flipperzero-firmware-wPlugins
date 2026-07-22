// Drives the WASM engine and routes its outbox: ws/all -> phone panels,
// uart -> the Flipper panel. Every entry point drains, so nothing is buffered
// across a frame boundary.
import { makeSocket, deliver, broadcast, getSocket, MAX_PHONES } from "./phones.js";

// Single on-page banner for "something is broken": fatal() (missing build outputs,
// halts the module) and the tick-loop's repeating-error case (below, non-fatal, keeps
// ticking) both funnel through showBanner() so there is exactly one banner implementation.
let bannerShown = false;

function showBanner(html) {
  if (bannerShown) return;
  bannerShown = true;
  const banner = document.createElement("div");
  banner.innerHTML = html;
  banner.style.cssText =
    "position:fixed;top:0;left:0;right:0;z-index:9999;padding:6px 10px;" +
    "background:#5a1e1e;color:#ffd7d7;border-bottom:1px solid #a33;" +
    "font:12px ui-monospace,SFMono-Regular,Menlo,monospace;";
  document.body.prepend(banner);
}

function fatal(msg) {
  showBanner(msg);
  throw new Error(msg);
}

// A static `import createEngine from "./engine.js"` does NOT work as a guard: when
// engine.js 404s (the normal state on a fresh clone — both build outputs are
// git-ignored), the module graph fails to resolve before this file's body ever runs, so
// no try/catch inside here can observe it (verified: with a static import, a 404'd
// engine.js produces silent console errors and a blank page, no banner). A dynamic
// import()'s rejection, unlike a static import's, IS observable from inside the
// importing module, so that's what's used here.
let M;
try {
  const { default: createEngine } = await import("./engine.js");
  M = await createEngine();
} catch {
  fatal("Engine failed to load. Run <code>sim/engine/build.sh</code> first.");
}
const uartSubscribers = [];
let nextPhoneId = 1;

const call = (name, argTypes, args) => M.ccall(name, null, argTypes, args);
const drainRaw = () => JSON.parse(M.ccall("ha_drain", "string", [], []));

function routeNow(items) {
  for (const it of items) {
    if (it.to === "ws") deliver(it.id, JSON.stringify(it.msg));
    else if (it.to === "all") broadcast(JSON.stringify(it.msg));
    else if (it.to === "uart") uartSubscribers.forEach((fn) => fn(it));
  }
}

// route() synchronously calls into a client's onmessage (via deliver/broadcast above).
// If a client ever sent a message from inside its own onmessage handler, that send
// would flow through engine.input -> drain() -> route() and nest inside this loop --
// so a later recipient in the OUTER batch could end up receiving a message produced by
// a NESTED, logically-later input before it gets the rest of its own (earlier) batch.
// On hardware the whole batch is queued and drained in order; nothing interleaves like
// that. Not reachable today (every client send here is behind a user gesture, so there
// is no in-page code path that sends from inside onmessage), but it's cheap to guard:
// a nested call while already routing enqueues its items instead of recursing, and the
// outer call drains the queue (a trampoline) once its own batch is done, so batches
// never interleave.
let routing = false;
const pendingRoutes = [];

function route(items) {
  if (routing) {
    pendingRoutes.push(items);
    return;
  }
  routing = true;
  try {
    routeNow(items);
    while (pendingRoutes.length) routeNow(pendingRoutes.shift());
  } finally {
    routing = false;
  }
}

const drain = () => route(drainRaw());

export const engine = {
  reset: () => { call("ha_reset", [], []); drain(); },
  input: (wsId, json) => { call("ha_input", ["number", "string"], [wsId, json]); drain(); },
  disconnect: (wsId) => { call("ha_disconnect", ["number"], [wsId]); drain(); },
  selectGame: (id) => { call("ha_select_game", ["number"], [id]); drain(); },
  roundEnd: () => { call("ha_round_end", [], []); drain(); },
  resetScores: () => { call("ha_reset_scores", [], []); drain(); },
  triviaClear: () => { call("ha_trivia_clear", [], []); drain(); },
  triviaAddTopic: (n) => { call("ha_trivia_add_topic", ["string"], [n]); drain(); },
  triviaAddQ: (j) => { call("ha_trivia_add_q", ["string"], [j]); drain(); },
  // Generic content ingest — the way every pack game (trivia/wyr/scramble/draw) is fed.
  contentClear: () => { call("ha_content_clear", [], []); drain(); },
  contentPack: (game, name) => { call("ha_content_pack", ["number", "string"], [game, name]); drain(); },
  contentItem: (json) => { call("ha_content_item", ["string"], [json]); drain(); },
};

export function subscribeUart(fn) { uartSubscribers.push(fn); }

// The client asks the parent for its socket (see harnessSocket() in web/core/app.js).
// The ?harness= value IS the wsId, so a panel and its engine-side player stay paired
// across reloads and removals — the harness never has to guess which socket is whose.
window.HA_HARNESS = {
  connect(id) {
    const wsId = Number(id);
    // The teardown callback is what lets close() (whichever path reaches it first, see
    // phones.js) tell the engine a player left, same as removePhone() below.
    return makeSocket(wsId, (sid, data) => engine.input(sid, data), (sid) => engine.disconnect(sid));
  },
};

export function addPhone() {
  if (document.querySelectorAll(".phone").length >= MAX_PHONES) return;
  const wsId = nextPhoneId++;
  const wrap = document.createElement("div");
  wrap.className = "phone";
  wrap.dataset.wsId = String(wsId);
  const frame = document.createElement("iframe");
  frame.src = "../../web/dist/index.html?harness=" + wsId;
  wrap.appendChild(frame);
  // frame.onerror does NOT fire for a 404'd iframe: a bad src is a navigation inside the
  // frame's own browsing context, not a resource-load failure on the <iframe> element
  // itself, so the element's error event never triggers (verified against a real 404 from
  // sim/serve.sh's http.server, which loads "successfully" as far as the element is
  // concerned). Detect a missing build instead, once the frame finishes loading, by
  // checking for the real client's own document rather than a generic error page —
  // same-origin here, so contentDocument is reachable.
  frame.addEventListener("load", () => {
    let ok = false;
    try {
      ok = !!frame.contentDocument && frame.contentDocument.title === "Hotspot Arcade";
    } catch {
      ok = true; // cross-origin: can't inspect it, assume the real client loaded
    }
    if (!ok) fatal("Client not built. Run <code>cd web && node build.mjs</code> first.");
  });
  document.getElementById("phones").appendChild(wrap);
}

export function removePhone() {
  const all = document.querySelectorAll(".phone");
  if (!all.length) return;
  const wrap = all[all.length - 1];
  const wsId = Number(wrap.dataset.wsId);
  // Route teardown through the socket's own close(), exactly as if the client (or a real
  // AsyncWebServer disconnect) had gotten there first. That single path tells the engine
  // the player went away and drops the socket from phones.js's map, and close()'s own
  // guard means it's harmless if the client also calls close() around the same time.
  // wrap.remove() must run even if sock.close() throws (e.g. a bad onclose handler),
  // otherwise a stale panel is left in the DOM with no way to remove it -- try/finally,
  // not a swallowing catch, since we still want the throw to surface.
  try {
    const sock = getSocket(wsId);
    if (sock) sock.close();
  } finally {
    wrap.remove();
  }
}

// Engine clock. The firmware ticks from millis(); here rAF drives it.
const started = performance.now();

// A bad frame (e.g. ha_drain() handing drainRaw() a malformed/empty string, or a routed
// handler throwing) must not wedge the sim forever — requestAnimationFrame(loop) has to
// run no matter what happened above it. We still don't want an error that repeats every
// frame (~60/sec) to flood the console, so: log the first occurrence of each distinct
// error, then once it's clearly not a one-off, log a single "giving up on logging this"
// note and put up a one-time visible banner. A later, different error resets the count
// and gets its own log line.
let lastLoopErrorMsg = null;
let loopErrorRepeatCount = 0;

// message is a caught JS error's .message — not attacker-controlled here, but escape it
// anyway since it's not a literal we wrote (unlike fatal()'s call sites, which pass
// hardcoded <code>...</code> strings straight into showBanner's innerHTML).
function escapeHtml(s) {
  return s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
}

function loop() {
  try {
    call("ha_tick", ["number"], [Math.floor(performance.now() - started)]);
    drain();
    lastLoopErrorMsg = null;
    loopErrorRepeatCount = 0;
  } catch (err) {
    const msg = err && err.message ? err.message : String(err);
    if (msg === lastLoopErrorMsg) {
      loopErrorRepeatCount++;
    } else {
      lastLoopErrorMsg = msg;
      loopErrorRepeatCount = 1;
      console.error("hotspot-arcade sim: tick/drain failed, dropping this frame:", err);
    }
    if (loopErrorRepeatCount === 3) {
      console.error(`hotspot-arcade sim: "${msg}" is repeating every frame; ` +
        "suppressing further console spam for it, see the on-page banner instead");
      showBanner("Simulator error (see console): " + escapeHtml(msg));
    }
  }
  // Always re-arm, even after a caught failure above — one bad frame must not stop the tick.
  requestAnimationFrame(loop);
}

engine.reset();
addPhone();
addPhone();
requestAnimationFrame(loop);
