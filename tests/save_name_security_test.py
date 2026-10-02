#!/usr/bin/env python3
"""Regression checks for the Save Name buffer and format-string boundaries."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


class SaveNameSecurityTests(unittest.TestCase):
    def test_ui_buffer_matches_credential_name_capacity(self):
        header = (ROOT / "dfc_i.h").read_text()
        self.assertIn("char text_store[DFC_FILE_NAME_MAX_LENGTH + 1];", header)

    def test_loaded_name_is_copied_as_literal_text(self):
        source = (ROOT / "dfc.c").read_text()
        self.assertIn(
            "strlcpy(dfc->text_store, text, sizeof(dfc->text_store));",
            source,
        )
        self.assertNotIn("vsnprintf(dfc->text_store", source)

    def test_save_uses_destination_capacity(self):
        source = (ROOT / "scenes" / "dfc_scene_save_name.c").read_text()
        self.assertIn("sizeof(dfc->credential->name)", source)
        self.assertNotIn("strlen(dfc->text_store) + 1", source)

    def test_reader_allocation_failures_stop_cleanly(self):
        source = (ROOT / "dfc_reader.c").read_text()
        self.assertIn("if(!reader) return NULL;", source)
        self.assertIn("if(!reader->secure_messaging)", source)
        self.assertIn("if(!reader->tx_buffer || !reader->rx_buffer)", source)

    def test_radio_send_checks_capacity_and_allocation(self):
        source = (ROOT / "dfc_emulator_listener.c").read_text()
        self.assertIn("length > DFC_BYTEBUF_MAX", source)
        self.assertIn("if(!radio_buffer)", source)

    def test_keys_are_cleared_before_free(self):
        reader = (ROOT / "dfc_reader.c").read_text()
        app = (ROOT / "dfc.c").read_text()
        self.assertIn(
            "memset(reader->secure_messaging, 0, sizeof(*reader->secure_messaging));",
            reader,
        )
        self.assertIn("memset(dfc->credential, 0, sizeof(*dfc->credential));", app)


if __name__ == "__main__":
    unittest.main()
