"""Regression tests for reset preserving attestation identity."""

from __future__ import annotations

import re
import unittest

from tests.harness import ROOT


class ResetAttestationIdentityTests(unittest.TestCase):
    def test_u2f_reset_wipe_preserves_attestation_assets(self) -> None:
        source = (ROOT / "src/u2f/persistence.c").read_text(encoding="utf-8")
        match = re.search(r"bool u2f_data_wipe\(Storage \*storage\) \{(?P<body>.*?)\n\}", source, re.S)

        self.assertIsNotNone(match)
        body = match.group("body")
        self.assertNotIn("U2F_CERT_FILE", body)
        self.assertNotIn("U2F_CERT_KEY_FILE", body)
        self.assertIn("U2F_KEY_FILE", body)
        self.assertIn("U2F_CNT_FILE", body)


if __name__ == "__main__":
    unittest.main()
