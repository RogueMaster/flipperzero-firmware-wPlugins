# Updating the question pools

Each pool is valid for four years and NCVEC occasionally publishes errata in
between. Shipping a new or corrected pool is a data change: the app discovers
whatever `.hcp` files are in `files/pools/` at start-up and reads the exam
length, pass mark, names and validity dates from the file itself. No C code
needs to change.

Upcoming changes:

| Pool       | Valid through | Replacement expected |
| ---------- | ------------- | -------------------- |
| General    | June 30, 2027 | 2027-2031 pool, usually released about six months earlier |
| Extra      | June 30, 2028 | 2028-2032 pool |
| Technician | June 30, 2030 | 2030-2034 pool |

CI runs `python tools/hampool.py upstream` every week. It fails when a pool
page on ncvec.org lists a newer document than the one recorded in
`pools/sources.json`, when NCVEC lists a pool page this repo does not know,
or when a shipped pool has expired.

## Errata for a pool we already ship

1. Open the pool's page (the `page` field in `pools/sources.json`) and read
   what changed.
2. In `pools/sources.json`, set `url` to the newest `.docx`, update
   `revision`, and set `sha256` to the checksum of the downloaded file
   (`python tools/hampool.py fetch` prints the actual checksum when it does
   not match).
3. Run `python tools/hampool.py all`.
4. Review `git diff pools/`. It should show exactly the errata: reworded
   questions, or questions that disappeared and are now listed under
   `deleted`.
5. Run `python tests/run_host_tests.py`, bump the app version, add a
   changelog line, release.

Users keep their progress: stats are stored by question number, so only
withdrawn questions lose theirs.

## A new four-year pool

1. Add a new entry to `pools/sources.json`, or replace the old one. Give it a
   new `stem` (for example `general-2027`) if both editions should ship
   during the changeover; the app lists the newest edition of a class first
   and labels an out-of-date one "(expired)".
2. Fill in `years`, `valid_from`, `valid_to` (from the "Effective" line at
   the top of the syllabus), `page`, `url`, `sha256` and `revision`.
   `exam_questions` and `pass_correct` only change if the FCC changes the
   exam itself.
3. Run `python tools/hampool.py all` and commit `pools/<stem>.json` and
   `files/pools/<stem>.hcp`. Remove the files of a pool you no longer ship.
4. Update the pool list in `README.md`, bump the version, add a changelog
   entry, test and release.

A new edition starts with empty progress, because question numbers are
reused for different questions.

## When the converter stops

`hampool.py` is deliberately strict and stops rather than guess. Typical
causes:

- **unmapped character**: the pool uses a character the Flipper fonts lack.
  Add an ASCII replacement to `ASCII_MAP`.
- **unrecognised line**: the document layout changed. Adjust the patterns
  near the top of the parsing section; the line number refers to the plain
  text extracted from the `.docx`.
- **answer refers to a position**: an answer says something like "Both A and
  B", which would be wrong once answers are shuffled. Extend `RE_PIN_LAST`
  or handle the question explicitly.
- **effective date not confirmed**: `valid_from` in `sources.json` does not
  match the document.
- **record is N bytes, limit 1536**: a question is longer than the app's
  buffer. Raise `MAX_RECORD_SIZE` in the tool and `POOL_MAX_RECORD` in
  `core/pool.h` together (and `QUIZ_TEXT_SIZE` in `views/quiz_view.c`).

Figures: questions whose text mentions "figure X-n" are flagged
automatically and handled by the app's **Figure Qs** setting.
