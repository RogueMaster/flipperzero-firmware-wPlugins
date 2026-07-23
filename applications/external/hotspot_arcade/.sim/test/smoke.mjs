// Headless smoke test: the real engine, loaded in Node, answers a hello.
import assert from "node:assert/strict";
import { newEngine } from "./harness-lib.mjs";

const e = await newEngine();
e.reset();
const out = e.join(1, "tarik");

const welcome = out.find((o) => o.to === "ws" && o.msg.t === "welcome");
assert.ok(welcome, "expected a welcome addressed to the joining socket");
assert.equal(welcome.id, 1, "welcome goes to the socket that said hello");
assert.equal(welcome.msg.nick, "TARIK", "engine uppercases nicknames on hello");

const join = out.find((o) => o.to === "uart" && o.kind === "join");
assert.ok(join, "expected a UART join for the Flipper");
assert.equal(join.nick, "TARIK");

console.log("smoke: OK");

// Regression: the outbox is built by hand-splicing strings (see esc() in
// ha_sim.cpp) rather than by a JSON library, so a nickname containing both a
// double quote and a backslash is the one input shape that can break the
// outbox's own JSON. Prove ha_drain() still parses, and that the nick survives
// (uppercased) round-trip through the UART join item.
const trickyNick = 'a"b\\c'; // literal chars: a " b \ c
let trickyOut;
assert.doesNotThrow(() => {
  trickyOut = e.join(2, trickyNick);
}, "ha_drain() must still parse as JSON when a nickname needs escaping");

const trickyJoin = trickyOut.find((o) => o.to === "uart" && o.kind === "join" && o.pid === 2);
assert.ok(trickyJoin, "expected a UART join for the escaped nickname");
assert.equal(
  trickyJoin.nick,
  trickyNick.toUpperCase(),
  "nick round-trips to the uppercased original despite the quote and backslash",
);

console.log("smoke (escaped nick): OK");
