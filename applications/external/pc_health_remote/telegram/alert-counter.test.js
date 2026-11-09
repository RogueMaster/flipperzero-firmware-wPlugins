import { test } from "node:test";
import assert from "node:assert/strict";
import { hitCounter } from "./alert-counter.js";

test("allows alerts up to the limit, then notes the limit once without incrementing", () => {
  let current = { n: 0, noted: false };
  for (let n = 1; n <= 3; n++) {
    const { next, result } = hitCounter(current, 3);
    assert.deepEqual(result, { n, allowed: true, noteNow: false });
    assert.deepEqual(next, { n, noted: false });
    current = next;
  }
  const denied = hitCounter(current, 3);
  assert.deepEqual(denied, {
    next: { n: 3, noted: true },
    result: { n: 3, allowed: false, noteNow: true },
  });
  assert.deepEqual(hitCounter(denied.next, 3), {
    next: null,
    result: { n: 3, allowed: false, noteNow: false },
  });
});

test("zero limit and counts above the limit are denied with a single note", () => {
  for (const n of [0, 4]) {
    assert.deepEqual(hitCounter({ n, noted: false }, 0), {
      next: { n, noted: true },
      result: { n, allowed: false, noteNow: true },
    });
    assert.deepEqual(hitCounter({ n, noted: true }, 0), {
      next: null,
      result: { n, allowed: false, noteNow: false },
    });
  }
});

test("preserves stored fields and does not mutate the input on any branch", () => {
  for (const noted of [false, true]) {
    const current = Object.freeze({ n: 2, noted, extra: "kept" });
    assert.deepEqual(hitCounter(current, 3).next, { n: 3, noted, extra: "kept" });
    assert.deepEqual(hitCounter(current, 2).next, noted ? null : { n: 2, noted: true, extra: "kept" });
    assert.deepEqual(current, { n: 2, noted, extra: "kept" });
  }
});
