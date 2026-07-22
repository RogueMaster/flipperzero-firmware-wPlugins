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
    selectGame: (id) => { M.ccall("ha_select_game", null, ["number"], [id]); return drain(); },
    roundEnd: () => { M.ccall("ha_round_end", null, [], []); return drain(); },
    resetScores: () => { M.ccall("ha_reset_scores", null, [], []); return drain(); },
    triviaClear: () => { M.ccall("ha_trivia_clear", null, [], []); return drain(); },
    triviaAddTopic: (name) => { M.ccall("ha_trivia_add_topic", null, ["string"], [name]); return drain(); },
    triviaAddQ: (json) => { M.ccall("ha_trivia_add_q", null, ["string"], [json]); return drain(); },
    contentClear: () => { M.ccall("ha_content_clear", null, [], []); return drain(); },
    contentPack: (game, name) => {
      M.ccall("ha_content_pack", null, ["number", "string"], [game, name]);
      return drain();
    },
    contentItem: (json) => { M.ccall("ha_content_item", null, ["string"], [json]); return drain(); },
  };
  api.join = (wsId, nick) => api.input(wsId, { t: "hello", nick, avatar: "🙂" });
  return api;
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
