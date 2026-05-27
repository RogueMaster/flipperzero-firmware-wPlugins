"""CLI entry point: builds the bilingual question pack from Open Trivia DB.

Outputs:
- data/trivia_{es,en}.{tsv,idx}        — binary pack for review/debug.
- src/data/embedded_pack_{es,en}.c     — C source compiled into the FAP.

The pack size is capped via TZ_PACK_LIMIT (default 1000). EN is the
canonical source; ES is produced by translating each EN question.

Usage:
    python build_pack.py
"""

from __future__ import annotations

import os
import sys
from pathlib import Path

from trivia_pack.opentdb import OpenTdbClient
from trivia_pack.pipeline import run_pipeline
from trivia_pack.translate import translator_from_env

_REPO_ROOT = Path(__file__).resolve().parent.parent
_DATA_DIR = _REPO_ROOT / "data"
_CACHE_DIR = _DATA_DIR / "_cache"
_BLACKLIST = _DATA_DIR / "blacklist.txt"
_C_OUT_DIR = _REPO_ROOT / "src" / "data"
_DEFAULT_LIMIT = 1000


def main() -> int:
    if not _BLACKLIST.exists():
        print(f"error: blacklist not found at {_BLACKLIST}", file=sys.stderr)
        return 1

    limit_env = os.environ.get("TZ_PACK_LIMIT")
    limit = int(limit_env) if limit_env else _DEFAULT_LIMIT

    opentdb = OpenTdbClient(cache_dir=_CACHE_DIR / "opentdb")
    translator = translator_from_env(cache_path=_CACHE_DIR / "translations.json")

    run_pipeline(
        opentdb=opentdb,
        translator=translator,
        blacklist_path=_BLACKLIST,
        out_dir=_DATA_DIR,
        c_out_dir=_C_OUT_DIR,
        limit=limit,
    )
    print("ok: pack written to data/trivia_{es,en}.{tsv,idx}")
    print("ok: embedded pack written to src/data/embedded_pack_{es,en}.c")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
