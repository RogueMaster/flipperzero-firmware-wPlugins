// Trivia is the self-organizing whole-group path: load a topic, two players join,
// both ready up, the countdown elapses, a question goes out, and a correct answer
// scores. Exercises selectGame + the trivia content exports + tick-driven state.
//
// Corrections to the original assumptions (verified against
// esp32/hotspot-arcade-fw/ha_games.h -- see task-2-report.md for the full list):
//   - Trivia/duel state pushes (pushAll) go per-socket via haWsSendWs, i.e.
//     {"to":"ws","id":<wsId>,...} -- never {"to":"all",...}. lastBroadcast() (which
//     filters to==="all") never matches them; use lastToWs() instead.
//   - "ready" carries an explicit boolean field: {t:"ready", ready:true}, not a
//     bare {t:"ready"}.
//   - Trivia questions/answers key the option array as "o" (not "a") and the
//     correct-option index as "c" (not "correct"), both on the way in
//     (ha_trivia_add_q) and the way out (the broadcast "trivia" message).
//   - "answer" carries the chosen index as "c" (not "i").
//   - Scoring (haUartScore) only fires from triviaDoReveal(), which runs once
//     every connected player has answered (or the 20s safety deadline elapses).
//     A single player's answer alone does not yet produce a UART score.
import assert from "node:assert/strict";
import { newEngine, lastToWs } from "./harness-lib.mjs";

const HA_GAME_TRIVIA = 1;

const e = await newEngine();
e.reset();
e.triviaClear();
e.triviaAddTopic("Test Pack");
e.triviaAddQ(JSON.stringify({
  q: "What hosts the AP?",
  o: ["Flipper", "ESP32", "Phone", "Router"],
  c: 1,
}));
e.selectGame(HA_GAME_TRIVIA);

e.join(1, "ana");
e.join(2, "bo");

// Both players ready up; an all-ready countdown starts the round.
e.input(1, { t: "ready", ready: true });
e.input(2, { t: "ready", ready: true });

// Advance past the countdown (TRIVIA_COUNTDOWN is 5 s) one second at a time,
// collecting everything the engine emits along the way.
let seen = [];
for (let ms = 1000; ms <= 8000; ms += 1000) seen = seen.concat(e.tick(ms));

const question = lastToWs(seen, 1, "trivia");
assert.ok(question, "expected a trivia push to socket 1 after the countdown");
assert.equal(question.msg.phase, "question", "the countdown should have elapsed into a question");
assert.ok(Array.isArray(question.msg.o), "question carries four options");
assert.equal(question.msg.o.length, 4);

// Correct answer scores; the Flipper is told about it over the UART. Reveal
// (and therefore the score) only fires once every connected player has
// answered, so both must answer before a UART score appears.
let answered = e.input(1, { t: "answer", c: 1 }); // ana: correct
answered = answered.concat(e.input(2, { t: "answer", c: 0 })); // bo: wrong, but triggers reveal

const score = answered.find((o) => o.to === "uart" && o.kind === "score");
assert.ok(score, "a correct answer emits a UART score");
assert.equal(score.pid, 1);
assert.ok(score.delta > 0, "correct answers score positive");

console.log("trivia: OK");
