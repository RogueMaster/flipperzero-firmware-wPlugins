from __future__ import annotations

from collections import Counter
from pathlib import Path

from trivia_pack.models import Lang, MappedQuestion, RawQuestion
from trivia_pack.pipeline import _balance_by_bucket, run_pipeline
from trivia_pack.translate import StubTranslator


class _FakeOpenTdb:
    def __init__(self, by_lang: dict[Lang, list[RawQuestion]]) -> None:
        self._by_lang = by_lang

    def iter_all(self, lang: Lang) -> list[RawQuestion]:
        return list(self._by_lang.get(lang, []))


def test_pipeline_translates_en_source_to_spanish(tmp_path: Path) -> None:
    cache_dir = tmp_path / "cache"
    out_dir = tmp_path / "out"
    blacklist_path = tmp_path / "bl.txt"
    blacklist_path.write_text("NFL\nSuper Bowl\n", encoding="utf-8")

    fake = _FakeOpenTdb(
        by_lang={
            Lang.EN: [
                RawQuestion(Lang.EN, "Geography", "Capital of Spain?", "Madrid"),
                RawQuestion(Lang.EN, "Sports", "Who won Super Bowl XII?", "Cowboys"),  # blacklist
                RawQuestion(Lang.EN, "Sports", "Who won the NFL MVP?", "X"),  # blacklist
                RawQuestion(Lang.EN, "History", "Who painted the Mona Lisa?", "Da Vinci"),
            ],
        }
    )
    translator = StubTranslator(cache_path=cache_dir / "translations.json")

    run_pipeline(
        opentdb=fake,
        translator=translator,
        blacklist_path=blacklist_path,
        out_dir=out_dir,
    )

    es_lines = (out_dir / "trivia_es.tsv").read_text(encoding="utf-8").splitlines()
    en_lines = (out_dir / "trivia_en.tsv").read_text(encoding="utf-8").splitlines()
    assert len(es_lines) == 2
    assert len(en_lines) == 2

    for es, en in zip(es_lines, en_lines, strict=True):
        es_parts = es.split("\t")
        en_parts = en.split("\t")
        assert es_parts[0] == en_parts[0]  # id
        assert es_parts[1] == en_parts[1]  # category_id

    en_content = "\n".join(en_lines)
    es_content = "\n".join(es_lines)
    assert "Capital of Spain?" in en_content  # EN preserved verbatim (after sanitize)
    assert "[es] Capital of Spain?" in es_content  # ES is the EN→ES stub translation


def test_pipeline_respects_limit(tmp_path: Path) -> None:
    blacklist_path = tmp_path / "bl.txt"
    blacklist_path.write_text("", encoding="utf-8")
    fake = _FakeOpenTdb(
        by_lang={
            Lang.EN: [RawQuestion(Lang.EN, "Geography", f"Q{i}?", f"A{i}") for i in range(50)],
        }
    )
    translator = StubTranslator(cache_path=tmp_path / "cache" / "translations.json")
    out_dir = tmp_path / "out"

    run_pipeline(
        opentdb=fake,
        translator=translator,
        blacklist_path=blacklist_path,
        out_dir=out_dir,
        limit=10,
    )
    assert len((out_dir / "trivia_en.tsv").read_text(encoding="utf-8").splitlines()) == 10


def test_pipeline_drops_multiple_choice_phrasing(tmp_path: Path) -> None:
    # OpenTDB items whose phrasing assumes the original 4-option list
    # ("Which of the following...", "All of the above") must be dropped:
    # without the choices they're unanswerable. Regression: 60 such items
    # leaked into the shipped pack before this filter existed.
    blacklist_path = tmp_path / "bl.txt"
    blacklist_path.write_text("", encoding="utf-8")

    fake = _FakeOpenTdb(
        by_lang={
            Lang.EN: [
                RawQuestion(Lang.EN, "Geography", "Capital of Spain?", "Madrid"),
                RawQuestion(
                    Lang.EN,
                    "Geography",
                    "Which of the following countries is an island?",
                    "Cyprus",
                ),
                RawQuestion(
                    Lang.EN,
                    "Geography",
                    "Which of these countries borders Poland?",
                    "Lithuania",
                ),
                RawQuestion(
                    Lang.EN,
                    "Entertainment: Music",
                    "Which song did Hendrix cover?",
                    "All of the above",
                ),
            ],
        }
    )
    translator = StubTranslator(cache_path=tmp_path / "cache" / "translations.json")
    out_dir = tmp_path / "out"

    run_pipeline(
        opentdb=fake,
        translator=translator,
        blacklist_path=blacklist_path,
        out_dir=out_dir,
    )

    en_lines = (out_dir / "trivia_en.tsv").read_text(encoding="utf-8").splitlines()
    assert len(en_lines) == 1
    assert "Capital of Spain?" in en_lines[0]


def test_pipeline_drops_questions_whose_translation_overflows(tmp_path: Path) -> None:
    # The pre-translation filter only sees the EN side. The StubTranslator
    # prefixes "[es] " (5 chars), so an EN question that fits at the boundary
    # will overflow once translated. Such items must be dropped post-translate
    # so the Flipper UI never has to render an oversized question.
    blacklist_path = tmp_path / "bl.txt"
    blacklist_path.write_text("", encoding="utf-8")

    short_q = "Capital of Spain?"  # well under the limit, must survive
    # 91 EN chars: passes pre-filter (<=95), but with "[es] " becomes 96 → drop.
    long_q = "Q" + "a" * 90
    assert len(long_q) == 91
    assert len(long_q) <= 95  # pre-filter passes
    assert len(f"[es] {long_q}") > 95  # post-filter must reject

    fake = _FakeOpenTdb(
        by_lang={
            Lang.EN: [
                RawQuestion(Lang.EN, "Geography", short_q, "Madrid"),
                RawQuestion(Lang.EN, "Geography", long_q, "X"),
            ],
        }
    )
    translator = StubTranslator(cache_path=tmp_path / "cache" / "translations.json")
    out_dir = tmp_path / "out"

    run_pipeline(
        opentdb=fake,
        translator=translator,
        blacklist_path=blacklist_path,
        out_dir=out_dir,
    )

    en_lines = (out_dir / "trivia_en.tsv").read_text(encoding="utf-8").splitlines()
    es_lines = (out_dir / "trivia_es.tsv").read_text(encoding="utf-8").splitlines()
    assert len(en_lines) == 1
    assert len(es_lines) == 1
    assert short_q in en_lines[0]
    assert long_q not in "\n".join(en_lines)


def _mq(bucket: int, idx: int) -> MappedQuestion:
    return MappedQuestion(
        source_lang=Lang.EN,
        bucket_id=bucket,
        question=f"q{bucket}-{idx}",
        answer=f"a{bucket}-{idx}",
    )


def test_balance_spillover_is_round_robin_across_buckets() -> None:
    # When several buckets have leftover questions after the per-bucket quota,
    # the spillover must be distributed round-robin so no single bucket
    # absorbs all the remainder. Regression: prior impl drained buckets in
    # bucket_id order, which made bucket 2 (Entertainment) own the entire
    # spillover whenever it had more questions than the others.
    mapped: list[MappedQuestion] = []
    mapped += [_mq(1, i) for i in range(10)]
    mapped += [_mq(2, i) for i in range(1000)]
    mapped += [_mq(3, i) for i in range(100)]
    mapped += [_mq(4, i) for i in range(5)]

    selected = _balance_by_bucket(mapped, target_total=50)
    counts = Counter(q.bucket_id for q in selected)

    assert sum(counts.values()) == 50
    quota = 50 // 7  # = 7
    # Buckets with surplus must each receive *some* spillover, not just bucket 2.
    assert counts[1] > quota
    assert counts[3] > quota
    # And no single bucket should hoard the spillover.
    assert counts[2] < 30


def test_balance_does_not_oversample_small_buckets() -> None:
    # If a bucket has fewer questions than the quota, take what's available
    # and fill the rest from larger buckets — without inventing duplicates.
    mapped: list[MappedQuestion] = []
    mapped += [_mq(1, i) for i in range(2)]
    mapped += [_mq(2, i) for i in range(100)]

    selected = _balance_by_bucket(mapped, target_total=20)
    counts = Counter(q.bucket_id for q in selected)

    assert sum(counts.values()) == 20
    assert counts[1] == 2  # only 2 available, can't exceed
    assert counts[2] == 18

    # No duplicates introduced by the balancing logic.
    questions = [q.question for q in selected]
    assert len(questions) == len(set(questions))
