from __future__ import annotations

from pathlib import Path

from build_pack import output_dirs

_REPO_ROOT = Path(__file__).resolve().parent.parent.parent
_SHIPPED = {_REPO_ROOT / "data", _REPO_ROOT / "src" / "data"}


def test_stub_never_writes_the_committed_pack() -> None:
    assert not _SHIPPED & set(output_dirs("stub"))


def test_anthropic_writes_the_committed_pack() -> None:
    assert set(output_dirs("anthropic")) == _SHIPPED
