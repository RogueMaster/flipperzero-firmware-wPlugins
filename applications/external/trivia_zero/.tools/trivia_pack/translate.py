"""Translation backends for the data pipeline.

The Translator protocol defines the contract every backend implements. Two
concrete backends are shipped:

  StubTranslator     — deterministic, network-free, default.
  AnthropicTranslator — real Claude Haiku translations behind an env var.

Both share the same on-disk JSON cache so switching between them mid-build
preserves any work already done.
"""

from __future__ import annotations

import json
import os
from pathlib import Path
from typing import Protocol

import anthropic

from trivia_pack.models import Lang


def _key(text: str, source: Lang, target: Lang) -> str:
    s = "es" if source == Lang.ES else "en"
    t = "es" if target == Lang.ES else "en"
    return f"{s}->{t}|{text}"


class Translator(Protocol):
    cache_hits: int
    cache_misses: int

    def translate(self, text: str, *, source: Lang, target: Lang) -> str: ...

    def flush(self) -> None: ...


class _CachedTranslator:
    def __init__(self, cache_path: Path) -> None:
        self._cache_path = cache_path
        self._cache: dict[str, str] = {}
        if cache_path.exists():
            self._cache = json.loads(cache_path.read_text(encoding="utf-8"))
        self.cache_hits = 0
        self.cache_misses = 0

    def translate(self, text: str, *, source: Lang, target: Lang) -> str:
        if not text:
            return ""
        if source == target:
            return text
        k = _key(text, source, target)
        cached = self._cache.get(k)
        if cached is not None:
            self.cache_hits += 1
            return cached
        self.cache_misses += 1
        out = self._translate_uncached(text, source=source, target=target)
        self._cache[k] = out
        return out

    def flush(self) -> None:
        self._cache_path.parent.mkdir(parents=True, exist_ok=True)
        self._cache_path.write_text(
            json.dumps(self._cache, ensure_ascii=False, indent=2),
            encoding="utf-8",
        )

    def _translate_uncached(self, text: str, *, source: Lang, target: Lang) -> str:
        raise NotImplementedError


class StubTranslator(_CachedTranslator):
    def _translate_uncached(self, text: str, *, source: Lang, target: Lang) -> str:
        del source
        prefix = "[es]" if target == Lang.ES else "[en]"
        return f"{prefix} {text}"


class AnthropicTranslator(_CachedTranslator):
    """Real translations via Claude Haiku.

    Activated when `TZ_TRANSLATOR=anthropic`. Requires `ANTHROPIC_API_KEY`.

    Uses prompt caching on the system prompt so the second-and-onward calls
    within the 5-minute TTL pay roughly 10% of the input-token cost.
    """

    _MODEL = "claude-haiku-4-5-20251001"
    _SYSTEM_PROMPT = (
        "You translate trivia questions or short answers between Spanish and English. "
        "Return only the translation, with no commentary, no quotes, and no surrounding "
        "whitespace. Output must not contain tab or newline characters."
    )

    def __init__(self, cache_path: Path) -> None:
        super().__init__(cache_path=cache_path)
        api_key = os.environ.get("ANTHROPIC_API_KEY")
        if not api_key:
            raise RuntimeError("ANTHROPIC_API_KEY is required for AnthropicTranslator.")
        self._client = anthropic.Anthropic(api_key=api_key)

    def _translate_uncached(self, text: str, *, source: Lang, target: Lang) -> str:
        target_name = "Spanish" if target == Lang.ES else "English"
        source_name = "Spanish" if source == Lang.ES else "English"
        # Prompt caching on the system prompt: subsequent calls within the
        # 5-minute TTL pay ~10% of input tokens for the cached portion.
        system_block: dict[str, object] = {
            "type": "text",
            "text": self._SYSTEM_PROMPT,
            "cache_control": {"type": "ephemeral"},
        }
        msg = self._client.messages.create(
            model=self._MODEL,
            max_tokens=400,
            system=[system_block],  # type: ignore[list-item]
            messages=[
                {
                    "role": "user",
                    "content": f"Translate from {source_name} to {target_name}:\n{text}",
                },
            ],
        )
        parts: list[str] = []
        for block in msg.content:
            text_attr = getattr(block, "text", None)
            if isinstance(text_attr, str):
                parts.append(text_attr)
        return "".join(parts).strip().replace("\t", " ").replace("\n", " ")


def translator_from_env(cache_path: Path) -> Translator:
    """Picks the backend based on TZ_TRANSLATOR (defaults to stub)."""
    backend = os.environ.get("TZ_TRANSLATOR", "stub").lower()
    if backend == "anthropic":
        return AnthropicTranslator(cache_path=cache_path)
    return StubTranslator(cache_path=cache_path)
