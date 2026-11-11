#!/usr/bin/env python3
"""Ham Cram question pool tooling.

Turns the official NCVEC question pool documents (.docx) into the compact
binary files (.hcp) that the Flipper app reads from the SD card.

    python tools/hampool.py fetch    # download the .docx files listed in pools/sources.json
    python tools/hampool.py parse    # .docx -> pools/<stem>.json (reviewable, committed)
    python tools/hampool.py pack     # pools/<stem>.json -> files/pools/<stem>.hcp
    python tools/hampool.py verify   # decode every .hcp and compare with its .json
    python tools/hampool.py all      # fetch + parse + pack + verify
    python tools/hampool.py upstream # has NCVEC published a newer pool or errata?

Only the Python standard library is used. See docs/pool-format.md for the
binary layout.
"""

from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import re
import struct
import sys
import urllib.parse
import urllib.request
import zipfile
from pathlib import Path
from xml.etree import ElementTree

ROOT = Path(__file__).resolve().parent.parent
SOURCES = ROOT / "pools" / "sources.json"
JSON_DIR = ROOT / "pools"
CACHE_DIR = ROOT / "pools" / ".cache"
HCP_DIR = ROOT / "files" / "pools"

W = "{http://schemas.openxmlformats.org/wordprocessingml/2006/main}"

# ---------------------------------------------------------------------------
# Binary format constants (keep in sync with core/pool.h)
# ---------------------------------------------------------------------------
MAGIC = b"HCP1"
FORMAT_VERSION = 1
HEADER_FMT = "<4sBBBBHBBII16s12sH14s"
HEADER_SIZE = struct.calcsize(HEADER_FMT)  # 64
SUB_FMT = "<3sBBBHH38s"
SUB_SIZE = struct.calcsize(SUB_FMT)  # 48
GROUP_FMT = "<4sHBB"
GROUP_SIZE = struct.calcsize(GROUP_FMT)  # 8
INDEX_FMT = "<IHBB"
INDEX_SIZE = struct.calcsize(INDEX_FMT)  # 8
RECORD_FMT = "<6sBBBBH4B"
RECORD_SIZE = struct.calcsize(RECORD_FMT)  # 16

FLAG_FIGURE = 0x01
FLAG_PIN_LAST = 0x02

MAX_RECORD_SIZE = 1536  # the app refuses pools with larger records
MAX_QUESTIONS = 1000
MAX_SUB_TITLE = 37
ANSWER_LETTERS = "ABCD"

assert HEADER_SIZE == 64 and SUB_SIZE == 48 and GROUP_SIZE == 8
assert INDEX_SIZE == 8 and RECORD_SIZE == 16


class PoolError(Exception):
    pass


# ---------------------------------------------------------------------------
# Text handling
# ---------------------------------------------------------------------------

# The Flipper fonts only cover printable ASCII, so everything else is mapped.
# Anything not in this table is a hard error: a human should decide.
ASCII_MAP = {
    "‘": "'",
    "’": "'",
    "“": '"',
    "”": '"',
    "–": "-",
    "—": "-",
    "−": "-",
    "…": "...",
    " ": " ",
    "°": " deg",
    "µ": "u",
    "μ": "u",
    "Ω": " ohms",
    "π": "pi",
    "×": "x",
    "÷": "/",
    "±": "+/-",
    "½": "1/2",
    "¼": "1/4",
    "≤": "<=",
    "≥": ">=",
    "\t": " ",
}


def clean(text: str) -> str:
    """Collapse whitespace but keep the original characters."""
    return re.sub(r"\s+", " ", text.replace(" ", " ")).strip()


def to_ascii(text: str, where: str) -> bytes:
    out = []
    for ch in text:
        ch = ASCII_MAP.get(ch, ch)
        for c in ch:
            if not 32 <= ord(c) <= 126:
                raise PoolError(f"{where}: unmapped character {c!r} (U+{ord(c):04X}); add it to ASCII_MAP")
        out.append(ch)
    return re.sub(r" +", " ", "".join(out)).strip().encode("ascii")


SMALL_WORDS = {"and", "of", "the", "for", "in", "to"}
ACRONYMS = {"RF", "FCC", "AC", "DC"}


def title_case(text: str) -> str:
    words = []
    for i, word in enumerate(text.split()):
        if word.upper() in ACRONYMS:
            words.append(word.upper())
        elif i and word.lower() in SMALL_WORDS:
            words.append(word.lower())
        else:
            words.append(word[:1].upper() + word[1:].lower())
    return " ".join(words)


# ---------------------------------------------------------------------------
# .docx parsing
# ---------------------------------------------------------------------------


def docx_lines(path: Path) -> list[str]:
    """Return the document body as plain text lines, in reading order."""
    with zipfile.ZipFile(path) as zf:
        root = ElementTree.fromstring(zf.read("word/document.xml"))
    lines: list[str] = []
    for para in root.iter(W + "p"):
        parts: list[str] = []
        for node in para.iter():
            if node.tag == W + "t":
                parts.append(node.text or "")
            elif node.tag == W + "tab":
                parts.append("\t")
            elif node.tag in (W + "br", W + "cr"):
                parts.append("\n")
        lines.extend("".join(parts).split("\n"))
    return lines


DASH = r"[-–—]"
RE_SUB = re.compile(
    rf"^SUBELEMENT\s+([TGE]\d)\s*{DASH}*\s*(.+?)\s*{DASH}*\s*"
    rf"\[\s*(\d+)\s+Exam\s+Questions?\s*{DASH}+\s*(\d+)\s+Groups?\s*\]\s*(?:(\d+)\s+Questions)?\s*$",
    re.I,
)
RE_GROUP = re.compile(rf"^([TGE]\d[A-Z])\s*{DASH}?\s+(\S.*)$")
RE_QUESTION = re.compile(r"^([TGE]\d[A-Z]\d\d)\s*\(([A-D])\)\s*(?:\[(.*)\])?\s*$")
RE_DELETED = re.compile(r"^([TGE]\d[A-Z]\d\d)\s+Question Deleted", re.I)
RE_ANSWER = re.compile(r"^([A-D])\.\s*(\S.*)$")
RE_END = re.compile(r"^~~+\s*end of question pool text", re.I)
RE_FIGURE = re.compile(r"\bfigure\s+([TGE]\d?-\d+)", re.I)
RE_PIN_LAST = re.compile(r"^(all|none|both|neither)\b.*\b(these|choices|above)\b", re.I)
RE_POSITIONAL = re.compile(r"\b(?:[A-D]\s+(?:and|or|&)\s+[A-D]\b|(?:answers?|choices?)\s+[A-D]\b|(?:of|all)\s+the\s+above\b)")


def parse_docx(path: Path, src: dict) -> dict:
    prefix = src["prefix"]
    lines = [clean(line) for line in docx_lines(path)]

    # The syllabus at the top repeats every SUBELEMENT line; the pool body
    # starts at the second occurrence of the first subelement.
    # Syllabus lines (and errata notes quoting them) carry a trailing
    # "NN Questions" count; the lines that open each part of the pool body
    # do not. The body starts at the first line without a count.
    syllabus_counts = {}
    body_subs = []
    for i, line in enumerate(lines):
        m = RE_SUB.match(line)
        if m and m.group(5):
            syllabus_counts[m.group(1).upper()] = int(m.group(5))  # last one wins
        elif m:
            body_subs.append((i, m.group(1).upper()))
    if not body_subs or sorted(s for _, s in body_subs) != sorted(syllabus_counts):
        raise PoolError(f"{path.name}: syllabus and pool body list different subelements")
    body_start = body_subs[0][0]

    head = " ".join(lines[:body_start])
    y, mo, d = src["valid_from"].split("-")
    date_forms = [f"{int(mo)}/{d}/{y}", f"{int(mo)}/{int(d)}/{y}", f"July {int(d)}, {y}"]
    if not any(f"Effective {form}" in head for form in date_forms):
        raise PoolError(f"{path.name}: effective date {src['valid_from']} not confirmed by the document")

    subelements: list[dict] = []
    deleted: list[str] = []
    sub = group = question = None
    ended = False

    def fail(msg: str, lineno: int):
        raise PoolError(f"{path.name} line {lineno + 1}: {msg}: {lines[lineno]!r}")

    for n in range(body_start, len(lines)):
        line = lines[n]
        if not line:
            continue
        if RE_END.match(line):
            ended = True
            break
        if question is not None:
            if line.startswith("~~"):
                if len(question["answers"]) != 4 or not question["question"]:
                    fail(f"{question['id']} is incomplete", n)
                group["questions"].append(question)
                question = None
                continue
            m = RE_ANSWER.match(line)
            if m and ANSWER_LETTERS[len(question["answers"]) % 4] == m.group(1) and len(question["answers"]) < 4:
                question["answers"].append(m.group(2))
            elif not question["answers"]:
                question["question"] = clean(question["question"] + " " + line)
            else:
                fail(f"unexpected line inside {question['id']}", n)
            continue
        m = RE_SUB.match(line)
        if m:
            sub = {
                "id": m.group(1).upper(),
                "title": title_case(m.group(2).strip(" -–—")),
                "exam_questions": int(m.group(3)),
                "declared_groups": int(m.group(4)),
                "groups": [],
            }
            subelements.append(sub)
            group = None
            continue
        m = RE_DELETED.match(line)
        if m:
            deleted.append(m.group(1))
            continue
        if line == "~~" and deleted:
            continue  # separator left behind after a deleted question
        m = RE_QUESTION.match(line)
        if m:
            if group is None or not m.group(1).startswith(group["id"]):
                fail("question outside its group", n)
            question = {
                "id": m.group(1),
                "correct": m.group(2),
                "ref": (m.group(3) or "").strip(),
                "question": "",
                "answers": [],
            }
            continue
        m = RE_GROUP.match(line)
        if m and sub is not None and m.group(1).startswith(sub["id"]):
            group = {"id": m.group(1), "topic": m.group(2), "questions": []}
            sub["groups"].append(group)
            continue
        fail("unrecognised line", n)

    if not ended or question is not None:
        raise PoolError(f"{path.name}: end-of-pool marker not found")

    # Structural validation against what the document itself declares.
    seen: set[str] = set()
    total = 0
    for sub in subelements:
        if not sub["id"].startswith(prefix):
            raise PoolError(f"{sub['id']}: wrong prefix for {src['name']}")
        if len(sub["groups"]) != sub.pop("declared_groups"):
            raise PoolError(f"{sub['id']}: group count differs from the SUBELEMENT header")
        if sub["exam_questions"] != len(sub["groups"]):
            raise PoolError(f"{sub['id']}: exam draws are not one per group")
        count = 0
        for group in sub["groups"]:
            if not group["questions"]:
                raise PoolError(f"{group['id']}: no questions")
            for q in group["questions"]:
                if q["id"] in seen:
                    raise PoolError(f"{q['id']}: duplicate")
                seen.add(q["id"])
                fig = RE_FIGURE.search(q["question"])
                if fig:
                    q["figure"] = fig.group(1).upper()
                for ans in q["answers"]:
                    if RE_POSITIONAL.search(ans):
                        raise PoolError(f"{q['id']}: answer refers to a position, unsafe to shuffle: {ans!r}")
            count += len(group["questions"])
        declared = syllabus_counts.get(sub["id"])
        if declared is not None and declared != count:
            print(f"  note: {sub['id']} syllabus says {declared} questions, pool has {count} (errata deletions)")
        total += count
    exam = sum(s["exam_questions"] for s in subelements)
    if exam != src["exam_questions"]:
        raise PoolError(f"{src['name']}: {exam} groups but {src['exam_questions']} exam questions expected")
    if src["pass_correct"] != -(-src["exam_questions"] * 74 // 100):
        raise PoolError(f"{src['name']}: pass_correct is not 74% of the exam")

    return {
        "name": src["name"],
        "element": src["element"],
        "years": src["years"],
        "valid_from": src["valid_from"],
        "valid_to": src["valid_to"],
        "exam_questions": src["exam_questions"],
        "pass_correct": src["pass_correct"],
        "source": {"page": src["page"], "url": src["url"], "sha256": src["sha256"], "revision": src["revision"]},
        "question_count": total,
        "deleted": deleted,
        "subelements": subelements,
    }


# ---------------------------------------------------------------------------
# Binary packing
# ---------------------------------------------------------------------------


def question_code(qid: str) -> int:
    """Stable 16-bit identity of a question, e.g. T1A01 -> 1*2600 + 0*100 + 1."""
    return int(qid[1]) * 2600 + (ord(qid[2]) - ord("A")) * 100 + int(qid[3:5])


def date_int(text: str) -> int:
    return int(text.replace("-", ""))


def pack_pool(pool: dict) -> bytes:
    subs = pool["subelements"]
    groups = [g for s in subs for g in s["groups"]]
    questions = [(gi, q) for gi, g in enumerate(groups) for q in g["questions"]]
    if len(subs) > 255 or len(groups) > 255 or not 0 < len(questions) <= MAX_QUESTIONS:
        raise PoolError("pool too large for the format")

    records = []
    for gi, q in questions:
        where = q["id"]
        ref = to_ascii(q["ref"], where)
        text = to_ascii(q["question"], where)
        answers = [to_ascii(a, where) for a in q["answers"]]
        if len(ref) > 255 or any(not 0 < len(a) <= 255 for a in answers) or not text:
            raise PoolError(f"{where}: field too long or empty")
        flags = 0
        if "figure" in q:
            flags |= FLAG_FIGURE
        if RE_PIN_LAST.match(answers[3].decode()):
            flags |= FLAG_PIN_LAST
        rec = struct.pack(
            RECORD_FMT,
            q["id"].encode(),
            ANSWER_LETTERS.index(q["correct"]),
            flags,
            gi,
            len(ref),
            len(text),
            *[len(a) for a in answers],
        )
        rec += ref + text + b"".join(answers)
        if len(rec) > MAX_RECORD_SIZE:
            raise PoolError(f"{where}: record is {len(rec)} bytes, limit {MAX_RECORD_SIZE}")
        records.append((rec, flags))

    sub_table = b""
    group_table = b""
    gi = qi = 0
    for si, sub in enumerate(subs):
        title = to_ascii(sub["title"], sub["id"])
        if len(title) > MAX_SUB_TITLE:
            raise PoolError(f"{sub['id']}: title longer than {MAX_SUB_TITLE} characters")
        nq = sum(len(g["questions"]) for g in sub["groups"])
        sub_table += struct.pack(SUB_FMT, sub["id"].encode(), gi, len(sub["groups"]), 0, qi, nq, title)
        for group in sub["groups"]:
            if len(group["questions"]) > 255:
                raise PoolError(f"{group['id']}: too many questions")
            group_table += struct.pack(GROUP_FMT, group["id"].encode(), qi, len(group["questions"]), si)
            qi += len(group["questions"])
            gi += 1

    offset = HEADER_SIZE + len(sub_table) + len(group_table) + INDEX_SIZE * len(questions)
    index = b""
    for (gidx, q), (rec, flags) in zip(questions, records, strict=True):
        index += struct.pack(INDEX_FMT, offset, question_code(q["id"]), flags, gidx)
        offset += len(rec)

    header = struct.pack(
        HEADER_FMT,
        MAGIC,
        FORMAT_VERSION,
        pool["element"],
        len(subs),
        len(groups),
        len(questions),
        pool["exam_questions"],
        pool["pass_correct"],
        date_int(pool["valid_from"]),
        date_int(pool["valid_to"]),
        pool["name"].encode("ascii"),
        pool["years"].encode("ascii"),
        max(len(rec) for rec, _ in records),
        b"",
    )
    return header + sub_table + group_table + index + b"".join(rec for rec, _ in records)


def unpack_pool(data: bytes) -> dict:
    """Decode a .hcp file the same way the app does. Used for verification."""

    def cstr(raw: bytes) -> str:
        return raw.split(b"\0", 1)[0].decode("ascii")

    (magic, version, element, n_sub, n_group, n_q, exam, need, v_from, v_to, name, years, max_rec, _) = struct.unpack_from(
        HEADER_FMT, data, 0
    )
    if magic != MAGIC or version != FORMAT_VERSION:
        raise PoolError("bad magic or version")
    pos = HEADER_SIZE
    subs = []
    for _ in range(n_sub):
        sid, first_g, n_g, _pad, first_q, nq, title = struct.unpack_from(SUB_FMT, data, pos)
        subs.append({"id": cstr(sid), "first_group": first_g, "groups": n_g, "first_q": first_q, "nq": nq, "title": cstr(title)})
        pos += SUB_SIZE
    groups = []
    for _ in range(n_group):
        gid, first_q, nq, si = struct.unpack_from(GROUP_FMT, data, pos)
        groups.append({"id": cstr(gid), "first_q": first_q, "nq": nq, "sub": si})
        pos += GROUP_SIZE
    questions = []
    for i in range(n_q):
        offset, code, flags, gi = struct.unpack_from(INDEX_FMT, data, pos + i * INDEX_SIZE)
        qid, correct, rflags, rgi, ref_len, q_len, *a_len = struct.unpack_from(RECORD_FMT, data, offset)
        if (rflags, rgi) != (flags, gi) or question_code(cstr(qid)) != code:
            raise PoolError(f"index/record mismatch at question {i}")
        p = offset + RECORD_SIZE
        fields = []
        for length in (ref_len, q_len, *a_len):
            fields.append(data[p : p + length].decode("ascii"))
            p += length
        if p - offset > max_rec:
            raise PoolError("record exceeds max_record_size")
        questions.append(
            {
                "id": cstr(qid),
                "correct": correct,
                "flags": flags,
                "group": gi,
                "ref": fields[0],
                "question": fields[1],
                "answers": fields[2:],
            }
        )
    if p != len(data):
        raise PoolError("trailing or missing bytes")
    return {
        "element": element,
        "exam_questions": exam,
        "pass_correct": need,
        "valid_from": v_from,
        "valid_to": v_to,
        "name": cstr(name),
        "years": cstr(years),
        "subelements": subs,
        "groups": groups,
        "questions": questions,
    }


def verify_pool(pool: dict, data: bytes) -> None:
    got = unpack_pool(data)
    want_q = [(g["id"], q) for s in pool["subelements"] for g in s["groups"] for q in g["questions"]]
    if len(got["questions"]) != len(want_q) or len(want_q) != pool["question_count"]:
        raise PoolError("question count mismatch")
    for (gid, want), have in zip(want_q, got["questions"], strict=True):
        where = want["id"]
        same = (
            have["id"] == want["id"]
            and ANSWER_LETTERS[have["correct"]] == want["correct"]
            and got["groups"][have["group"]]["id"] == gid
            and have["question"].encode() == to_ascii(want["question"], where)
            and [a.encode() for a in have["answers"]] == [to_ascii(a, where) for a in want["answers"]]
            and bool(have["flags"] & FLAG_FIGURE) == ("figure" in want)
        )
        if not same:
            raise PoolError(f"{where}: packed data differs from JSON")
    for gi, g in enumerate(got["groups"]):
        members = [q for q in got["questions"][g["first_q"] : g["first_q"] + g["nq"]]]
        if any(q["group"] != gi or not q["id"].startswith(g["id"]) for q in members):
            raise PoolError(f"{g['id']}: group range is wrong")
        if all(q["flags"] & FLAG_FIGURE for q in members):
            print(f"  note: every question in {g['id']} needs a figure")
    if got["exam_questions"] != len(got["groups"]):
        raise PoolError("exam length differs from group count")


# ---------------------------------------------------------------------------
# Commands
# ---------------------------------------------------------------------------


def load_sources() -> list[dict]:
    return json.loads(SOURCES.read_text(encoding="utf-8"))["pools"]


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def cmd_fetch(sources: list[dict]) -> None:
    CACHE_DIR.mkdir(parents=True, exist_ok=True)
    for src in sources:
        dest = CACHE_DIR / f"{src['stem']}.docx"
        if not (dest.exists() and sha256(dest) == src["sha256"]):
            print(f"fetch  {src['url']}")
            req = urllib.request.Request(src["url"], headers={"User-Agent": "hampool/1.0"})
            with urllib.request.urlopen(req, timeout=60) as resp:
                dest.write_bytes(resp.read())
        if sha256(dest) != src["sha256"]:
            raise PoolError(
                f"{dest.name}: sha256 {sha256(dest)} differs from sources.json. "
                "NCVEC may have published new errata; review the pool page and update sources.json."
            )
        print(f"ok     {dest.relative_to(ROOT)}")


def cmd_parse(sources: list[dict]) -> None:
    for src in sources:
        docx = CACHE_DIR / f"{src['stem']}.docx"
        if not docx.exists():
            raise PoolError(f"{docx} missing; run `hampool.py fetch` first")
        pool = parse_docx(docx, src)
        out = JSON_DIR / f"{src['stem']}.json"
        out.write_text(json.dumps(pool, indent=1, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
        figures = sum("figure" in q for s in pool["subelements"] for g in s["groups"] for q in g["questions"])
        print(
            f"parse  {out.relative_to(ROOT)}: {pool['question_count']} questions, "
            f"{sum(len(s['groups']) for s in pool['subelements'])} groups, "
            f"{len(pool['deleted'])} deleted, {figures} need a figure"
        )


def cmd_pack(sources: list[dict]) -> None:
    HCP_DIR.mkdir(parents=True, exist_ok=True)
    for src in sources:
        pool = json.loads((JSON_DIR / f"{src['stem']}.json").read_text(encoding="utf-8"))
        data = pack_pool(pool)
        out = HCP_DIR / f"{src['stem']}.hcp"
        out.write_bytes(data)
        print(f"pack   {out.relative_to(ROOT)}: {len(data)} bytes")


def cmd_verify(sources: list[dict]) -> None:
    for src in sources:
        pool = json.loads((JSON_DIR / f"{src['stem']}.json").read_text(encoding="utf-8"))
        hcp = HCP_DIR / f"{src['stem']}.hcp"
        data = hcp.read_bytes()
        if data != pack_pool(pool):
            raise PoolError(f"{hcp.name} is stale; run `hampool.py pack`")
        verify_pool(pool, data)
        print(f"verify {hcp.relative_to(ROOT)}: ok")


POOLS_INDEX = "https://www.ncvec.org/index.php/amateur-question-pools"


def http_get(url: str) -> str:
    req = urllib.request.Request(url, headers={"User-Agent": "hampool/1.0"})
    with urllib.request.urlopen(req, timeout=60) as resp:
        return resp.read().decode("utf-8", errors="replace")


def cmd_upstream(sources: list[dict]) -> None:
    """Fail when NCVEC has published something this repo does not have yet."""
    problems = []

    def doc_name(url: str) -> str:
        return urllib.parse.unquote(url).rsplit("/", 1)[-1].strip().lower()

    for src in sources:
        html = http_get(src["page"])
        docs = re.findall(r'href="([^"]+\.docx)"', html, re.I)
        if not docs:
            problems.append(f"{src['name']}: no .docx link on {src['page']}")
        elif doc_name(docs[0]) != doc_name(src["url"]):
            # NCVEC lists the newest release (latest errata) first
            problems.append(f"{src['name']}: newest document is now {doc_name(docs[0])!r}")
        else:
            print(f"ok     {src['name']}: {src['revision']}")
        if date_int(src["valid_to"]) < int(datetime.date.today().strftime("%Y%m%d")):
            problems.append(f"{src['name']}: pool expired on {src['valid_to']}")

    known = {s["page"].rsplit("/", 1)[-1] for s in load_sources()}
    listed = set(re.findall(r'href="[^"]*/index\.php/(\d{4}-\d{4}-[a-z-]*question-pool[a-z-]*)"', http_get(POOLS_INDEX)))
    for page in sorted(listed - known):
        problems.append(f"new pool page listed by NCVEC: {page}")

    if problems:
        raise PoolError("upstream changed:\n  " + "\n  ".join(problems) + "\nSee docs/updating-pools.md")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("command", choices=["fetch", "parse", "pack", "verify", "all", "upstream"])
    parser.add_argument("--only", help="limit to one pool stem, e.g. technician")
    args = parser.parse_args()
    sources = [s for s in load_sources() if args.only in (None, s["stem"])]
    if not sources:
        parser.error(f"no pool named {args.only!r} in {SOURCES.name}")
    steps = ["fetch", "parse", "pack", "verify"] if args.command == "all" else [args.command]
    try:
        for step in steps:
            globals()[f"cmd_{step}"](sources)
    except PoolError as err:
        print(f"error: {err}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
