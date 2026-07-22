// parsePack() is the one piece of sim/ logic that duplicates real firmware behaviour
// (trivia_stream_pack in flipper/hotspot-arcade/helpers/ha_session.c), so it's the one
// thing in this project that can silently drift from the engine it mirrors. These
// cases each pin down a place the previous "split on ---" implementation got wrong.
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";
import { parsePack } from "../web/trivia-packs.js";

const here = dirname(fileURLToPath(import.meta.url));

// (a) packs/README.md line 39: blocks may be separated by blank lines alone,
// with no "---" at all. The C parser has no notion of "---" as a delimiter -- it
// flushes on the next "Q:" line (or end of input) regardless. A pack that never uses
// "---" must still parse every question, not collapse into one giant block.
{
  const text = `
Pack: No Dashes

Q: 2 + 2?
A: 3
B: 4
C: 5
D: 6
Answer: B

Q: Capital of Japan?
A: Seoul
B: Beijing
C: Tokyo
D: Bangkok
Answer: C
`;
  const pack = parsePack(text, "fallback");
  assert.equal(pack.name, "No Dashes");
  assert.equal(pack.questions.length, 2, "both questions parse with no --- present");
  assert.deepEqual(pack.questions[0], { q: "2 + 2?", o: ["3", "4", "5", "6"], c: 1 });
  assert.deepEqual(
    pack.questions[1],
    { q: "Capital of Japan?", o: ["Seoul", "Beijing", "Tokyo", "Bangkok"], c: 2 },
  );
}

// (b) Options are stored by letter in the C parser (o[t[0]-'A']), so a block that
// lists D: before A: must still map "Answer:" to the correct option, not to
// whichever line happened to arrive first.
{
  const text = `
Q: Out of order?
D: fourth
B: second
A: first
C: third
Answer: A
`;
  const pack = parsePack(text, "fallback");
  assert.equal(pack.questions.length, 1);
  assert.deepEqual(pack.questions[0].o, ["first", "second", "third", "fourth"]);
  assert.equal(pack.questions[0].c, 0, "Answer: A indexes the option actually labeled A:");
}

// (c) A question missing its Answer: line has gotAns stay false, so it must be
// dropped rather than silently defaulted to option A.
{
  const text = `
Q: Has all options but no answer
A: one
B: two
C: three
D: four

Q: Complete question
A: x
B: y
C: z
D: w
Answer: D
`;
  const pack = parsePack(text, "fallback");
  assert.equal(pack.questions.length, 1, "the answerless question is dropped");
  assert.equal(pack.questions[0].q, "Complete question");
}

// (d) The real shipped pack still parses to the expected shape end to end. It uses
// "---" throughout, so this also proves the rewrite didn't break the old path while
// fixing the new one.
{
  const path = join(here, "..", "..", "packs", "trivia", "general.txt");
  const text = readFileSync(path, "utf8");
  const pack = parsePack(text, "general");
  assert.equal(pack.name, "General Knowledge");
  assert.equal(pack.questions.length, 15, "general.txt has 15 questions");
  for (const q of pack.questions) {
    assert.equal(typeof q.q, "string");
    assert.ok(q.q.length > 0);
    assert.equal(q.o.length, 4, "every question carries exactly four options");
    for (const opt of q.o) assert.ok(opt.length > 0, "no option is left blank");
    assert.ok(Number.isInteger(q.c) && q.c >= 0 && q.c <= 3, "c is a valid option index");
  }
}

console.log("packs: OK");
