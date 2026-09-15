from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from host.fibp_host.permissions import PermissionStore, permission_key
from scripts.fibp_codec import Hello


def example_hello(version: str = "0.3") -> Hello:
    return Hello(
        1, 0, 1, 0, 1, 512, 4096, 123, "Flipper Zero", "Mico", 1, b"12345678", version
    )


class PermissionStoreTests(unittest.TestCase):
    def test_grant_stores_only_hash_and_can_be_revoked(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "permissions.json"
            store = PermissionStore(path)
            hello = example_hello()
            self.assertFalse(store.contains(hello))
            store.grant(hello)
            self.assertTrue(store.contains(hello))
            encoded = path.read_text(encoding="utf-8")
            self.assertNotIn(hello.device_id.hex(), encoded)
            self.assertEqual(json.loads(encoded)["grants"], [permission_key(hello)])
            store.revoke(hello)
            self.assertFalse(path.exists())

    def test_application_version_changes_permission_identity(self) -> None:
        self.assertNotEqual(
            permission_key(example_hello("0.2")), permission_key(example_hello("0.3"))
        )


if __name__ == "__main__":
    unittest.main()
