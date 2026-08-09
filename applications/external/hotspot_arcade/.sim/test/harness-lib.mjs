// Thin ergonomic wrapper over the exported C API, shared by the headless tests.
import createEngine from "../web/engine.js";

export async function newEngine() {
  const M = await createEngine();
  const drain = () => JSON.parse(M.ccall("ha_drain", "string", [], []));
  const api = {
    drain,
    reset: () => { M.ccall("ha_reset", null, [], []); return drain(); },
    tick: (ms) => { M.ccall("ha_tick", null, ["number"], [ms]); return drain(); },
    input: (wsId, obj) => {
      M.ccall("ha_input", null, ["number", "string"], [wsId, JSON.stringify(obj)]);
      return drain();
    },
    disconnect: (wsId) => { M.ccall("ha_disconnect", null, ["number"], [wsId]); return drain(); },
    // Which phone a socket sits on. Sockets default to one device each (a panel per
    // phone); give two of them the same key to model two browser contexts on one
    // phone, or 0 to model a device the firmware couldn't identify. The key is split
    // into two 32-bit halves because ccall has no 64-bit argument type.
    setDevice: (wsId, key) => {
      const hi = Math.floor(key / 2 ** 32) >>> 0;
      M.ccall("ha_ws_device", null, ["number", "number", "number"], [wsId, hi, key >>> 0]);
    },
    selectGame: (id) => { M.ccall("ha_select_game", null, ["number"], [id]); return drain(); },
    roundEnd: () => { M.ccall("ha_round_end", null, [], []); return drain(); },
    resetScores: () => { M.ccall("ha_reset_scores", null, [], []); return drain(); },
    setLang: (lang) => { M.ccall("ha_set_lang", null, ["string"], [lang || ""]); return drain(); },
    triviaClear: () => { M.ccall("ha_trivia_clear", null, [], []); return drain(); },
    triviaAddTopic: (name) => { M.ccall("ha_trivia_add_topic", null, ["string"], [name]); return drain(); },
    triviaAddQ: (json) => { M.ccall("ha_trivia_add_q", null, ["string"], [json]); return drain(); },
    contentClear: () => { M.ccall("ha_content_clear", null, [], []); return drain(); },
    contentPack: (game, name) => {
      M.ccall("ha_content_pack", null, ["number", "string"], [game, name]);
      return drain();
    },
    contentItem: (json) => { M.ccall("ha_content_item", null, ["string"], [json]); return drain(); },
    // HA_CHESS_TEST-only hooks: load an arbitrary position into match slot 0 (must
    // already be a live game from challenge/accept), and perft a scratch position
    // against the real move generator.
    chessLoad: (board64, stm, rights, ep, halfmove, wms, bms) => {
      M.ccall(
        "ha_chess_load", null,
        ["string", "number", "number", "number", "number", "number", "number"],
        [board64, stm, rights, ep, halfmove, wms, bms],
      );
      return drain();
    },
    chessPerft: (board64, stm, rights, ep, depth) =>
      M.ccall(
        "ha_chess_perft", "number",
        ["string", "number", "number", "number", "number"],
        [board64, stm, rights, ep, depth],
      ),
  };
  api.join = (wsId, nick) => api.input(wsId, { t: "hello", nick, avatar: "🙂" });
  return api;
}

/**
 * A MAC as the opaque device key the engine identifies a phone by. On hardware the
 * firmware builds this from the station's MAC; the exact encoding is its business, so
 * tests only need distinct, stable numbers (48 bits fits a JS integer exactly).
 */
export function macKey(a, b, c, d, e, f) {
  return ((((a * 256 + b) * 256 + c) * 256 + d) * 256 + e) * 256 + f;
}

/** Last broadcast (to:"all") whose msg.t equals `type`, or undefined. */
export function lastBroadcast(items, type) {
  return items.filter((o) => o.to === "all" && o.msg && o.msg.t === type).pop();
}

/**
 * Last unicast (to:"ws") sent to `wsId` whose msg.t equals `type`, or undefined.
 *
 * Most engine state pushes (trivia/duel/lobby/...) go through pushAll(), which
 * calls haWsSendWs() once per connected socket rather than haWsBroadcast() -- so
 * `lastBroadcast` above never matches them. This is the helper the per-player
 * game tests actually need.
 */
export function lastToWs(items, wsId, type) {
  return items
    .filter((o) => o.to === "ws" && o.id === wsId && o.msg && o.msg.t === type)
    .pop();
}
