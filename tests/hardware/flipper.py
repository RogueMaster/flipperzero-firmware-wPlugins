"""Flipper deployment and CLI control using uFBT's storage transport."""

import hashlib
import importlib
import re
import sys
import time
from pathlib import Path


TEST_DIRECTORY = "/ext/apps_data/dfc/.hardware-test"
FAP_PATH = "/ext/apps/NFC/dfc.fap"
PROFILE_PATH = f"{TEST_DIRECTORY}/profile.csv"
MEMORY_PATTERN = re.compile(
    rb"DfcMemory.*?free=(\d+) minimum=(\d+) largest=(\d+)", re.IGNORECASE
)
HEAP_PATTERN = re.compile(
    rb"Free heap size: (\d+).*?Total heap size: (\d+).*?"
    rb"Minimum heap size: (\d+).*?Maximum heap block: (\d+).*?"
    rb"Pool free: (\d+).*?Maximum pool block: (\d+)",
    re.DOTALL,
)


def _storage_type():
    scripts = Path.home() / ".ufbt/current/scripts"
    if not scripts.is_dir():
        raise RuntimeError("uFBT SDK scripts were not found; run ufbt update")
    for packages in (Path.home() / ".ufbt/toolchain").glob(
        "*/lib/python*/site-packages"
    ):
        sys.path.insert(0, str(packages))
    sys.path.insert(0, str(scripts))
    return importlib.import_module("flipper.storage").FlipperStorage


def detect_port(requested: str) -> str:
    if requested != "auto":
        return requested
    from serial.tools import list_ports

    matches = [
        port.device
        for port in list_ports.comports()
        if port.vid == 0x0483 or "Flipper" in (port.description or "")
    ]
    if len(matches) != 1:
        raise RuntimeError(f"expected one Flipper serial port, found {matches}")
    return matches[0]


class Flipper:
    def __init__(self, port: str):
        self.storage = _storage_type()(detect_port(port))
        self.app_running = None

    def __enter__(self):
        self.storage.start()
        return self

    def __exit__(self, *_):
        try:
            self.close_app()
        finally:
            self.storage.stop()

    def close_app(self) -> bytes:
        if self.app_running is False:
            return b""
        for _ in range(10):
            self.storage.send_and_wait_eol("loader close\r")
            result = self.storage.read.until(self.storage.CLI_EOL)
            if b"was closed" in result:
                self.storage.read.until(self.storage.CLI_EOL)
                time.sleep(0.2)
                self.app_running = False
                return result
            if result.startswith(b"No application"):
                self.storage.read.until(self.storage.CLI_EOL)
                self.app_running = False
                return result
        raise RuntimeError(f"could not close app: {result.decode(errors='replace')}")

    def prepare_directory(self) -> None:
        if not self.storage.exist_dir(TEST_DIRECTORY):
            self.storage.mkdir(TEST_DIRECTORY)

    def upload_verified(self, local: Path, remote_name: str) -> str:
        remote = f"{TEST_DIRECTORY}/{remote_name}"
        self.storage.send_file(str(local), remote)
        uploaded = bytes(self.storage.read_file(remote))
        local_hash = hashlib.sha256(local.read_bytes()).hexdigest()
        if hashlib.sha256(uploaded).hexdigest() != local_hash:
            raise RuntimeError(f"upload verification failed for {remote}")
        return remote

    def launch_fixture(self, remote: str) -> bytes:
        self.storage.send_and_wait_eol(
            f'loader open "{FAP_PATH}" --hardware-test {remote}\r'
        )
        output = self.storage.read.until(self.storage.CLI_EOL)
        if b"error" in output.lower() or b"failed" in output.lower():
            raise RuntimeError(output.decode(errors="replace"))
        self.app_running = True
        return output

    def drain_logs(self) -> bytes:
        buffered = bytes(self.storage.read.buffer)
        self.storage.read.buffer.clear()
        waiting = self.storage.port.in_waiting
        return buffered + (self.storage.port.read(waiting) if waiting else b"")

    def memory_samples(self, data: bytes) -> list[dict[str, int]]:
        return [
            {"free": int(free), "minimum": int(minimum), "largest": int(largest)}
            for free, minimum, largest in MEMORY_PATTERN.findall(data)
        ]

    def heap_snapshot(self) -> dict[str, int]:
        self.storage.send_and_wait_eol("free\r")
        output = bytes(self.storage.read.until(self.storage.CLI_PROMPT))
        match = HEAP_PATTERN.search(output)
        if not match:
            raise RuntimeError(f"cannot parse Flipper heap output: {output!r}")
        names = (
            "free",
            "total",
            "minimum",
            "largest",
            "pool_free",
            "pool_largest",
        )
        return dict(zip(names, (int(value) for value in match.groups())))

    def read_hardware_profile(self) -> dict[str, float]:
        if not self.storage.exist_file(PROFILE_PATH):
            raise RuntimeError("hardware timing profile was not written")
        lines = bytes(self.storage.read_file(PROFILE_PATH)).decode().splitlines()
        if len(lines) != 2:
            raise RuntimeError("hardware timing profile is malformed")
        names = lines[0].split(",")
        values = [int(value) for value in lines[1].split(",")]
        profile = dict(zip(names, values))
        cycles_per_us = profile.pop("cycles_per_us")
        result = {"cycles_per_us": cycles_per_us}
        for prefix in (
            "read_prepare",
            "read_send",
            "continuation_prepare",
            "continuation_send",
        ):
            count = profile[f"{prefix}_count"]
            result[f"{prefix}_count"] = count
            result[f"{prefix}_mean_us"] = (
                profile[f"{prefix}_total"] / count / cycles_per_us if count else 0.0
            )
            result[f"{prefix}_max_us"] = profile[f"{prefix}_max"] / cycles_per_us
        return result

    def remove_fixture(self, remote: str) -> None:
        if self.storage.exist_file(remote):
            self.storage.remove(remote)

    def cleanup(self) -> None:
        if self.storage.exist_dir(TEST_DIRECTORY):
            _, _, files = next(self.storage.walk(TEST_DIRECTORY))
            for name in files:
                self.storage.remove(f"{TEST_DIRECTORY}/{name}")
            self.storage.remove(TEST_DIRECTORY)
