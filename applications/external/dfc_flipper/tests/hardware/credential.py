"""Read the small part of a DFC text fixture needed by host probes."""

from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class ReadFixture:
    uid: bytes
    aid: bytes
    file_number: int
    key_number: int
    key: bytes
    data: bytes


def _fields(path: Path) -> dict[str, str]:
    return {
        name.strip(): value.strip()
        for line in path.read_text().splitlines()
        if ":" in line
        for name, value in (line.split(":", 1),)
    }


def _read_fixture(
    fields: dict[str, str], application: int, file_index: int, key_index: int
) -> ReadFixture:
    app = f"Application {application:02d}"
    file_prefix = f"{app} File {file_index:02d}"
    fixture = ReadFixture(
        uid=bytes.fromhex(fields["UID"]),
        aid=bytes.fromhex(fields[f"{app} AID"])[::-1],
        file_number=int(fields[f"{file_prefix} Number"], 16),
        key_number=key_index,
        key=bytes.fromhex(fields[f"{app} Key {key_index:02d}"]),
        data=bytes.fromhex(fields[f"{file_prefix} Data"]),
    )
    if len(fixture.aid) != 3 or not fixture.data:
        raise ValueError("fixture needs a three-byte AID and nonempty file data")
    return fixture


def load_read_fixture(
    path: Path, application: int = 0, file_index: int = 0, key_index: int = 1
) -> ReadFixture:
    return _read_fixture(_fields(path), application, file_index, key_index)


def load_read_fixtures(
    path: Path, application: int = 0, key_index: int = 1
) -> tuple[ReadFixture, ...]:
    fields = _fields(path)
    count = int(fields[f"Application {application:02d} File Count"])
    return tuple(
        _read_fixture(fields, application, file_index, key_index)
        for file_index in range(count)
    )
