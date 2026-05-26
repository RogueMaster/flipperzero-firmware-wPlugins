"""Composition root for the off-Flipper data pipeline."""

from __future__ import annotations

from collections import defaultdict, deque
from collections.abc import Iterable
from dataclasses import dataclass
from pathlib import Path
from typing import Protocol

from trivia_pack.blacklist import Blacklist
from trivia_pack.category_map import is_skipped, map_opentdb_to_bucket
from trivia_pack.mc_phrasing import is_multiple_choice_phrasing
from trivia_pack.models import BilingualQuestion, Lang, MappedQuestion, RawQuestion
from trivia_pack.pack_writer import write_embedded_pack, write_pack
from trivia_pack.translate import Translator

_MAX_QUESTION_LEN = 95
_MAX_ANSWER_LEN = 40
_BUCKET_COUNT = 7


class _OpenTdbSource(Protocol):
    def iter_all(self, lang: Lang) -> Iterable[RawQuestion]: ...


@dataclass(frozen=True)
class _DedupeKey:
    bucket_id: int
    source_lang: Lang
    question_normalized: str


def _normalize(s: str) -> str:
    return " ".join(s.lower().strip().split())


def _filter_and_map(
    raw: Iterable[RawQuestion],
    blacklist: Blacklist,
) -> list[MappedQuestion]:
    out: list[MappedQuestion] = []
    for q in raw:
        if len(q.question) > _MAX_QUESTION_LEN or len(q.answer) > _MAX_ANSWER_LEN:
            continue
        if is_skipped(q.opentdb_category):
            continue
        if is_multiple_choice_phrasing(q.question, q.answer):
            continue
        if blacklist.is_blacklisted(q.question, q.answer):
            continue
        try:
            bucket = map_opentdb_to_bucket(q.opentdb_category)
        except KeyError:
            print(
                f"warn: unknown OpenTDB category {q.opentdb_category!r}, dropping question.",
            )
            continue
        out.append(
            MappedQuestion(
                source_lang=q.source_lang,
                bucket_id=int(bucket),
                question=q.question,
                answer=q.answer,
            ),
        )
    return out


def _take_quota(
    queues: dict[int, deque[MappedQuestion]],
    bucket_ids: list[int],
    quota: int,
    target_total: int,
    selected: list[MappedQuestion],
) -> None:
    for bucket_id in bucket_ids:
        q = queues[bucket_id]
        for _ in range(quota):
            if not q or len(selected) >= target_total:
                break
            selected.append(q.popleft())


def _round_robin_spillover(
    queues: dict[int, deque[MappedQuestion]],
    bucket_ids: list[int],
    target_total: int,
    selected: list[MappedQuestion],
) -> None:
    while len(selected) < target_total:
        progress = False
        for bucket_id in bucket_ids:
            if len(selected) >= target_total:
                break
            q = queues[bucket_id]
            if q:
                selected.append(q.popleft())
                progress = True
        if not progress:
            break


def _print_balance_report(
    mapped_count: int,
    selected: list[MappedQuestion],
    target_total: int,
    quota: int,
) -> None:
    print(
        f"balance: {mapped_count} mapped → {len(selected)} after "
        f"per-bucket quota + round-robin spillover "
        f"(target={target_total}, quota/bucket={quota})",
        flush=True,
    )
    by_bucket: dict[int, int] = defaultdict(int)
    for q in selected:
        by_bucket[q.bucket_id] += 1
    for bucket_id in sorted(by_bucket.keys()):
        print(f"balance:   bucket {bucket_id}: {by_bucket[bucket_id]}", flush=True)


def _balance_by_bucket(
    mapped: list[MappedQuestion],
    target_total: int,
) -> list[MappedQuestion]:
    """Sample across the 7 buckets so the final pack feels like classic Trivial
    Pursuit (balanced across categories) rather than dominated by whichever
    bucket OpenTDB happens to have most of.

    Two-phase strategy:
      1. Per-bucket quota (target/7): take up to that many from each bucket.
      2. Round-robin spillover: cycle through the buckets that still have
         questions, taking one at a time, until target_total is reached or
         everything is exhausted.

    Within each bucket the original order from OpenTDB is preserved.
    """
    queues: dict[int, deque[MappedQuestion]] = defaultdict(deque)
    for q in mapped:
        queues[q.bucket_id].append(q)

    quota = max(1, target_total // _BUCKET_COUNT)
    bucket_ids = sorted(queues.keys())
    selected: list[MappedQuestion] = []

    _take_quota(queues, bucket_ids, quota, target_total, selected)
    _round_robin_spillover(queues, bucket_ids, target_total, selected)
    _print_balance_report(len(mapped), selected, target_total, quota)
    return selected


_FLUSH_EVERY = 20


def _build_bilingual(
    mapped: list[MappedQuestion],
    translator: Translator,
    *,
    limit: int | None = None,
) -> list[BilingualQuestion]:
    seen: set[_DedupeKey] = set()
    out: list[BilingualQuestion] = []
    post_filtered = 0
    total = len(mapped)
    cap_msg = f", capping at {limit}" if limit is not None else ""
    print(f"translate: starting, {total} mapped questions to process{cap_msg}", flush=True)
    for idx, q in enumerate(mapped, start=1):
        if limit is not None and len(out) >= limit:
            print(f"translate: cap reached at {len(out)}, stopping early", flush=True)
            break
        key = _DedupeKey(
            bucket_id=q.bucket_id,
            source_lang=q.source_lang,
            question_normalized=_normalize(q.question),
        )
        if key in seen:
            continue
        seen.add(key)

        if q.source_lang == Lang.EN:
            question_en = q.question
            answer_en = q.answer
            question_es = translator.translate(q.question, source=Lang.EN, target=Lang.ES)
            answer_es = translator.translate(q.answer, source=Lang.EN, target=Lang.ES)
        else:
            question_es = q.question
            answer_es = q.answer
            question_en = translator.translate(q.question, source=Lang.ES, target=Lang.EN)
            answer_en = translator.translate(q.answer, source=Lang.ES, target=Lang.EN)

        # Post-translation length guard: ES tends to expand 15-25% over EN, so
        # an EN question that fit pre-translation can still overflow the
        # Flipper screen once translated. Drop borderline cases here.
        if (
            len(question_es) > _MAX_QUESTION_LEN
            or len(question_en) > _MAX_QUESTION_LEN
            or len(answer_es) > _MAX_ANSWER_LEN
            or len(answer_en) > _MAX_ANSWER_LEN
        ):
            post_filtered += 1
            continue

        # Post-translation MC-phrasing guard: translations occasionally
        # introduce "cual de los siguientes..." phrasing on the ES side
        # that wasn't present in the EN source.
        if is_multiple_choice_phrasing(question_es, answer_es) or is_multiple_choice_phrasing(
            question_en, answer_en
        ):
            post_filtered += 1
            continue

        out.append(
            BilingualQuestion(
                bucket_id=q.bucket_id,
                question_es=question_es,
                answer_es=answer_es,
                question_en=question_en,
                answer_en=answer_en,
            ),
        )

        if idx % _FLUSH_EVERY == 0 or idx == total:
            translator.flush()
            print(
                f"translate: {idx}/{total} "
                f"(unique={len(out)}, cache hits={translator.cache_hits}, "
                f"LLM calls={translator.cache_misses})",
                flush=True,
            )
    if post_filtered:
        print(
            f"translate: dropped {post_filtered} questions exceeding "
            f"post-translation length limits "
            f"(question>{_MAX_QUESTION_LEN} or answer>{_MAX_ANSWER_LEN})",
            flush=True,
        )
    return out


def run_pipeline(
    *,
    opentdb: _OpenTdbSource,
    translator: Translator,
    blacklist_path: Path,
    out_dir: Path,
    c_out_dir: Path | None = None,
    limit: int | None = None,
) -> None:
    """Runs the full pipeline.

    English is the canonical source. Each EN question (up to `limit`) is
    translated to Spanish via the configured translator, producing a
    symmetric bilingual pack where EN is native and ES is derived. The
    Spanish OpenTDB corpus is intentionally not fetched in this mode.

    Always emits the binary pack to `out_dir` (data/trivia_*.{tsv,idx}) for
    review and debugging. If `c_out_dir` is provided, also emits the
    embedded C source files (src/data/embedded_pack_*.c) which the FAP
    compiles into its binary.
    """
    blacklist = Blacklist.from_file(blacklist_path)
    raw_en = list(opentdb.iter_all(Lang.EN))
    mapped = _filter_and_map(raw_en, blacklist)
    if limit is not None:
        mapped = _balance_by_bucket(mapped, target_total=limit)
    bilingual = _build_bilingual(mapped, translator, limit=limit)
    bilingual.sort(key=lambda q: (q.bucket_id, q.question_en))
    write_pack(bilingual, out_dir=out_dir)
    if c_out_dir is not None:
        write_embedded_pack(bilingual, c_out_dir=c_out_dir)
    translator.flush()
