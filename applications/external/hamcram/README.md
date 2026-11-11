# Ham Cram

Study for the US amateur radio (ham) license exams on your Flipper Zero. Offline, distraction-free, and always in your pocket.

Ham Cram carries the complete official question pools for all three license classes:

- **Technician** (Element 2), 2026-2030 pool
- **General** (Element 3), 2023-2027 pool
- **Amateur Extra** (Element 4), 2024-2028 pool

## What it does

- **Practice exams** built like the real thing: one question drawn at random from every question group (35 questions for Technician and General, 50 for Extra), graded against the real 74% pass mark (26 of 35, or 37 of 50). Nothing is revealed until you grade the exam, and you can step back to change an answer first. Afterwards, **Missed** walks you through the questions you got wrong with the right answers marked.
- **Study by topic**. Work through a whole pool, one subelement (for example _T5 Electrical Principles_) or a single question group. You see the right answer immediately after each question. Questions you already know drop out, so each session picks up where you left off.
- **Review missed**. Every question you get wrong, in study or in an exam, goes on a review list. The ones you miss most often come first, and a question leaves the list once you have answered it correctly enough times to make up for the misses.
- **Progress that sticks**. Mastered questions, the review list and your exam history are saved to the SD card, per pool.
- **Shuffled answers**, so you learn the content and not the letter. "All these choices are correct" stays in last place where it makes sense. Shuffling can be turned off under **Settings**.
- **Instant feedback**. In study and review the LED blinks green for a right answer and red, with a short buzz, for a wrong one, and a graded exam gets two green blinks for a pass. It makes no sound, and can be turned off under **Settings**.
- **Rule citations**. Where the pool gives one, the FCC Part 97 rule behind a question (for example "FCC rule: 97.1") is shown under the answers once the answer is revealed.

## Controls

- **Up / Down**: scroll through a long question, then move between the answers
- **OK**: choose the highlighted answer; after the answer is shown, go to the next question
- **Left / Right**: previous or next question (use Right to skip one in an exam)
- **Back**: leave the session; in an exam it asks whether to grade it, quit or resume

A check mark shows the correct answer and a cross shows a wrong pick. In an exam, the answer you chose is shown with its letter boxed.

## Questions with figures

A few questions refer to a schematic or chart (for example "figure T-1"). Those drawings are not readable on a 128x64 screen, so by default Ham Cram leaves these questions out of exams, study and review: 12 in Technician, 5 in General and 25 in Extra. They can still appear on your real exam. To practice them, set **Settings > Figure Qs** to **Show** and keep the figures from [ncvec.org](https://www.ncvec.org/index.php/amateur-question-pools) at hand. Such questions are marked "Figure not shown".

## Good to know

- "Mastered" means you answered the question correctly the last time you saw it.
- An unanswered exam question counts as wrong, as it would on the real exam, but is not added to your review list.
- Question pools are replaced every four years. Ham Cram shows each pool's validity dates under **Progress** and marks a pool as expired once it is no longer used for exams. The General pool changes on July 1, 2027; update the app to get the new one.
- Progress lives in the apps_data/hamcram folder on the SD card. **Reset Progress** clears one pool at a time.

## Question pools

The questions are the official pools released into the public domain by the NCVEC Question Pool Committee, including all errata published as of November 2026:

- Technician 2026-2030, effective July 1, 2026 through June 30, 2030, with the errata of February 19, 2026 (409 questions)
- General 2023-2027, effective July 1, 2023 through June 30, 2027, with the 6th errata of February 4, 2026 (423 questions)
- Amateur Extra 2024-2028, effective July 1, 2024 through June 30, 2028, with the 4th errata of February 4, 2026 (599 questions)

Questions withdrawn by the committee are not included. Wording is unchanged apart from typographic characters the Flipper font does not have (curly quotes and dashes).

## Disclaimer

Ham Cram is an independent, open source project. It is **not affiliated with, sponsored by or endorsed by** the ARRL, the NCVEC or the FCC. It is a study aid only: check [ncvec.org](https://www.ncvec.org/) for the current pools and any newer errata before your exam.

## Source, issues and license

Source code, build instructions and the pool conversion tools are at [github.com/NicPWNs/HamCram](https://github.com/NicPWNs/HamCram). Bug reports are welcome there. For developers: see [docs/development.md](https://github.com/NicPWNs/HamCram/blob/main/docs/development.md) and [docs/updating-pools.md](https://github.com/NicPWNs/HamCram/blob/main/docs/updating-pools.md).

The app is released under the MIT license. The question pools are in the public domain.
