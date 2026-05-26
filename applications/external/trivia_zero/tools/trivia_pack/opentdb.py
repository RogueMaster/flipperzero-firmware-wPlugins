"""Open Trivia DB API client with on-disk JSON cache.

The cache is a single JSON file per language (cache_dir/opentdb_{lang}.json)
written incrementally after each page so a Ctrl+C mid-fetch never loses work.
A session token is requested at the start so OpenTDB returns NEW questions
on each page instead of random ones — without a token the API would loop
forever for English (it has thousands of unique questions and no end-of-data
signal without the token).
"""

from __future__ import annotations

import html
import json
import time
from pathlib import Path
from typing import TYPE_CHECKING

import httpx

from trivia_pack.models import Lang, RawQuestion

if TYPE_CHECKING:
    from collections.abc import Iterator

_API_URL = "https://opentdb.com/api.php"
_TOKEN_URL = "https://opentdb.com/api_token.php"  # noqa: S105 — API URL, not a credential
_PAGE_SIZE = 50
_REQUEST_DELAY_SECONDS = 5.0  # OpenTDB enforces this; less triggers HTTP 429.
_RATE_LIMIT_BACKOFFS = (10.0, 20.0, 40.0)  # progressive sleeps before each retry
_HTTP_TOO_MANY_REQUESTS = 429

_RESPONSE_OK = 0
_RESPONSE_NO_RESULTS = 1
_RESPONSE_TOKEN_EMPTY = 4


class OpenTdbClient:
    def __init__(self, cache_dir: Path, *, sleep_between: float = _REQUEST_DELAY_SECONDS) -> None:
        self._cache_dir = cache_dir
        self._sleep_between = sleep_between

    def iter_all(self, lang: Lang) -> Iterator[RawQuestion]:
        cache_file = self._cache_path(lang)
        if cache_file.exists():
            yield from self._load_cache(cache_file)
            return

        yield from self._fetch_with_token(lang, cache_file)

    def _cache_path(self, lang: Lang) -> Path:
        suffix = "es" if lang == Lang.ES else "en"
        return self._cache_dir / f"opentdb_{suffix}.json"

    def _fetch_with_token(self, lang: Lang, cache_file: Path) -> Iterator[RawQuestion]:
        suffix = "es" if lang == Lang.ES else "en"
        questions: list[RawQuestion] = []

        with httpx.Client(timeout=30.0) as client:
            token = self._request_token(client)
            print(f"opentdb {suffix}: token acquired, starting fetch...", flush=True)

            page = 0
            while True:
                time.sleep(self._sleep_between)
                page += 1

                payload = self._get_with_retry(
                    client,
                    _API_URL,
                    params={
                        "amount": str(_PAGE_SIZE),
                        "language": suffix,
                        "token": token,
                    },
                    label=f"opentdb {suffix} page {page}",
                )
                code = payload.get("response_code", _RESPONSE_OK)

                if code in (_RESPONSE_TOKEN_EMPTY, _RESPONSE_NO_RESULTS):
                    print(
                        f"opentdb {suffix}: no more results (code={code}); "
                        f"final count = {len(questions)}",
                        flush=True,
                    )
                    break
                if code != _RESPONSE_OK:
                    msg = f"OpenTDB returned response_code={code} for {suffix}"
                    raise RuntimeError(msg)

                results = payload.get("results")
                if not isinstance(results, list) or not results:
                    break

                questions.extend(
                    RawQuestion(
                        source_lang=lang,
                        opentdb_category=html.unescape(r["category"]),
                        question=html.unescape(r["question"]),
                        answer=html.unescape(r["correct_answer"]),
                    )
                    for r in results
                )

                self._save_cache(cache_file, questions)
                print(
                    f"opentdb {suffix}: page {page} → {len(questions)} questions cached",
                    flush=True,
                )

        yield from questions

    def _request_token(self, client: httpx.Client) -> str:
        payload = self._get_with_retry(
            client,
            _TOKEN_URL,
            params={"command": "request"},
            label="opentdb token",
        )
        if payload.get("response_code", _RESPONSE_OK) != _RESPONSE_OK:
            msg = f"OpenTDB token request failed: {payload}"
            raise RuntimeError(msg)
        return str(payload["token"])

    def _get_with_retry(
        self,
        client: httpx.Client,
        url: str,
        *,
        params: dict[str, str],
        label: str,
    ) -> dict[str, object]:
        last_error: Exception | None = None
        for attempt, backoff in enumerate((0.0, *_RATE_LIMIT_BACKOFFS)):
            if backoff > 0.0:
                print(
                    f"{label}: HTTP 429, retry {attempt} after {backoff:.0f}s...",
                    flush=True,
                )
                time.sleep(backoff)
            try:
                resp = client.get(url, params=params)
                if resp.status_code == _HTTP_TOO_MANY_REQUESTS:
                    last_error = httpx.HTTPStatusError(
                        f"{label}: 429 Too Many Requests",
                        request=resp.request,
                        response=resp,
                    )
                    continue
                resp.raise_for_status()
                data: dict[str, object] = resp.json()
            except httpx.HTTPError as e:
                last_error = e
                continue
            else:
                return data
        msg = f"{label}: gave up after {len(_RATE_LIMIT_BACKOFFS) + 1} attempts"
        raise RuntimeError(msg) from last_error

    def _save_cache(self, path: Path, questions: list[RawQuestion]) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        serializable = [
            {
                "source_lang": "ES" if q.source_lang == Lang.ES else "EN",
                "opentdb_category": q.opentdb_category,
                "question": q.question,
                "answer": q.answer,
            }
            for q in questions
        ]
        tmp = path.with_suffix(path.suffix + ".tmp")
        tmp.write_text(
            json.dumps(serializable, ensure_ascii=False, indent=2),
            encoding="utf-8",
        )
        tmp.replace(path)

    def _load_cache(self, path: Path) -> Iterator[RawQuestion]:
        for entry in json.loads(path.read_text(encoding="utf-8")):
            yield RawQuestion(
                source_lang=Lang.ES if entry["source_lang"] == "ES" else Lang.EN,
                opentdb_category=entry["opentdb_category"],
                question=entry["question"],
                answer=entry["answer"],
            )
