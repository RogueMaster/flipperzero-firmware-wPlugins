from __future__ import annotations

import hashlib
import json
import os
import sys
from pathlib import Path

from scripts.fibp_codec import Hello


def default_permission_path() -> Path:
    if sys.platform == "win32":
        root = Path(os.environ.get("APPDATA", Path.home() / "AppData" / "Roaming"))
    elif sys.platform == "darwin":
        root = Path.home() / "Library" / "Application Support"
    else:
        root = Path(os.environ.get("XDG_CONFIG_HOME", Path.home() / ".config"))
    return root / "FlipperUSBInternetBridge" / "permissions.json"


def permission_key(hello: Hello) -> str:
    """Return a non-reversible identifier; raw hardware IDs are never stored."""

    material = b"fibp-host-permission-v1\0"
    material += bytes((hello.id_type, hello.maximum_major, hello.maximum_minor))
    material += hello.device_id
    # FIBP 1.0 has no application id. Including the application version avoids
    # silently carrying grants between differently-versioned client builds.
    material += b"\0" + hello.app_version.encode("utf-8")
    return hashlib.sha256(material).hexdigest()


class PermissionStore:
    def __init__(self, path: Path | None = None):
        self.path = path or default_permission_path()

    def _load(self) -> set[str]:
        try:
            data = json.loads(self.path.read_text(encoding="utf-8"))
        except (FileNotFoundError, OSError, ValueError, TypeError):
            return set()
        if not isinstance(data, dict) or data.get("schema") != 1:
            return set()
        grants = data.get("grants", [])
        return {item for item in grants if isinstance(item, str) and len(item) == 64}

    def contains(self, hello: Hello) -> bool:
        return permission_key(hello) in self._load()

    def grant(self, hello: Hello) -> None:
        grants = self._load()
        grants.add(permission_key(hello))
        self.path.parent.mkdir(parents=True, exist_ok=True)
        temporary = self.path.with_suffix(".tmp")
        temporary.write_text(
            json.dumps({"schema": 1, "grants": sorted(grants)}, indent=2) + "\n",
            encoding="utf-8",
        )
        try:
            os.chmod(temporary, 0o600)
        except OSError:
            pass
        temporary.replace(self.path)

    def revoke(self, hello: Hello) -> None:
        grants = self._load()
        grants.discard(permission_key(hello))
        if not grants:
            try:
                self.path.unlink()
            except FileNotFoundError:
                pass
            return
        self.path.write_text(
            json.dumps({"schema": 1, "grants": sorted(grants)}, indent=2) + "\n",
            encoding="utf-8",
        )
