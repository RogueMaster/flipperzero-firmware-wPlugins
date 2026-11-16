#!/usr/bin/env python3
"""Regression checks for credential-loading resource use."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


class CredentialStorageResourceTests(unittest.TestCase):
    def test_text_load_does_not_round_trip_through_der(self):
        source = (ROOT / "dfc_credential_storage.c").read_text()
        load = source.split("static bool dfc_credential_file_load(", 1)[1]
        load = load.split("bool dfc_credential_load_selected_file(", 1)[0]

        self.assertIn("codec.api->parse(credential", load)
        self.assertNotIn("dfc_der_encoded_size", load)
        self.assertNotIn("dfc_der_encode", load)
        self.assertEqual(load.count("dfc_der_decode"), 1)


if __name__ == "__main__":
    unittest.main()
