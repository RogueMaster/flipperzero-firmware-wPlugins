# Pool file format (.hcp, version 1)

One file holds one question pool. It is written by `tools/hampool.py pack`
and read by `core/pool.c`. All integers are little-endian, all text is
printable ASCII, and fixed-width strings are NUL padded.

```
header            64 bytes
subelement table  48 bytes x num_subelements
group table        8 bytes x num_groups
question index     8 bytes x num_questions
question records  variable length
```

The app keeps the header and the three tables in RAM and reads one question
record at a time.

## Header (64 bytes)

| Offset | Size | Field           | Notes                                 |
| ------ | ---- | --------------- | ------------------------------------- |
| 0      | 4    | magic           | `HCP1`                                |
| 4      | 1    | version         | 1                                     |
| 5      | 1    | element         | FCC exam element: 2, 3 or 4           |
| 6      | 1    | num_subelements |                                       |
| 7      | 1    | num_groups      |                                       |
| 8      | 2    | num_questions   | at most 1000                          |
| 10     | 1    | exam_questions  | equals num_groups (one per group)     |
| 11     | 1    | pass_correct    | correct answers needed to pass        |
| 12     | 4    | valid_from      | decimal YYYYMMDD, e.g. 20260701       |
| 16     | 4    | valid_to        | decimal YYYYMMDD                      |
| 20     | 16   | name            | e.g. `Technician`                     |
| 36     | 12   | years           | e.g. `2026-2030`                      |
| 48     | 2    | max_record      | size of the largest question record   |
| 50     | 14   | reserved        | zero                                  |

## Subelement (48 bytes)

| Offset | Size | Field          |
| ------ | ---- | -------------- |
| 0      | 3    | id, e.g. `T1`  |
| 3      | 1    | first_group    |
| 4      | 1    | num_groups     |
| 5      | 1    | reserved       |
| 6      | 2    | first_question |
| 8      | 2    | num_questions  |
| 10     | 38   | title          |

## Group (8 bytes)

| Offset | Size | Field           |
| ------ | ---- | --------------- |
| 0      | 4    | id, e.g. `T1A`  |
| 4      | 2    | first_question  |
| 6      | 1    | num_questions   |
| 7      | 1    | subelement      |

Subelements, groups and questions are stored in pool order, so every
subelement and every group is one contiguous range of question indexes.

## Question index entry (8 bytes)

| Offset | Size | Field  | Notes                                             |
| ------ | ---- | ------ | ------------------------------------------------- |
| 0      | 4    | offset | absolute file offset of the record                |
| 4      | 2    | code   | stable identity, see below                        |
| 6      | 1    | flags  | bit 0: needs a figure; bit 1: keep last answer last |
| 7      | 1    | group  | index into the group table                        |

`code` identifies a question independently of its position, so saved
progress survives errata that remove questions. For `T1A01` it is
`1 * 2600 + ('A' - 'A') * 100 + 1`: subelement digit, group letter, number.

## Question record (16 bytes + text)

| Offset | Size | Field        | Notes                              |
| ------ | ---- | ------------ | ---------------------------------- |
| 0      | 6    | id           | e.g. `T1A01`                       |
| 6      | 1    | correct      | 0-3 for A-D                        |
| 7      | 1    | flags        | same as the index entry            |
| 8      | 1    | group        | same as the index entry            |
| 9      | 1    | ref_len      | FCC rule citation, may be 0        |
| 10     | 2    | question_len |                                    |
| 12     | 4    | answer_len   | one byte per answer, never 0       |
| 16     | ...  | text         | citation, question, answers A to D |

The six strings follow one another with no separators. A record is at most
1536 bytes.

## Progress files

Progress is saved next to the app's other data as `<pool file name>.sav`:
a 20-byte header (`HCS1`, element, question count, the pool's `valid_from`,
exam counters) followed by 3 bytes per question (`code`, status byte). The
status byte holds a 3-bit review weight, a "last answer correct" bit and a
"seen" bit; see `core/progress.h`. A save is only loaded into the pool
edition it was written for.
