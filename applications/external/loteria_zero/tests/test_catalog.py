#!/usr/bin/env python3
"""Catalog integrity tests for Lotería Zero."""

from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from gen_cards import validate  # noqa: E402


class CatalogTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.catalog = json.loads((ROOT / "cards.json").read_text(encoding="utf-8"))
        cls.images = ROOT / "images"

    def test_total_cards(self) -> None:
        self.assertEqual(self.catalog["total_cards"], 54)
        self.assertEqual(len(self.catalog["cards"]), 54)

    def test_unique_ids(self) -> None:
        ids = [c["id"] for c in self.catalog["cards"]]
        self.assertEqual(sorted(ids), list(range(1, 55)))

    def test_required_fields_and_rhymes(self) -> None:
        for card in self.catalog["cards"]:
            self.assertTrue(card["name"])
            self.assertTrue(card["image"])
            self.assertGreaterEqual(len(card["rhyme"]), 8)

    def test_illustrations_exist(self) -> None:
        for card in self.catalog["cards"]:
            path = self.images / f"{card['image']}.png"
            self.assertTrue(path.is_file(), f"missing {path.name}")

    def test_validator_clean(self) -> None:
        errors = validate(self.catalog, self.images)
        self.assertEqual(errors, [])

    def test_generated_display_strings_are_ascii(self) -> None:
        from gen_cards import to_ascii

        for card in self.catalog["cards"]:
            name = to_ascii(card["name"])
            rhyme = to_ascii(card["rhyme"])
            self.assertTrue(name.isascii(), card["name"])
            self.assertTrue(rhyme.isascii(), card["rhyme"])
            self.assertTrue(len(name) > 0)
            self.assertTrue(len(rhyme) > 0)

    def test_traditional_order_names(self) -> None:
        expected = [
            "El Gallo",
            "El Diablito",
            "La Dama",
            "El Catrín",
            "El Paraguas",
            "La Sirena",
            "La Escalera",
            "La Botella",
            "El Barril",
            "El Árbol",
            "El Melón",
            "El Valiente",
            "El Gorrito",
            "La Muerte",
            "La Pera",
            "La Bandera",
            "El Bandolón",
            "El Violoncello",
            "La Garza",
            "El Pájaro",
            "La Mano",
            "La Bota",
            "La Luna",
            "El Cotorro",
            "El Borracho",
            "El Negrito",
            "El Corazón",
            "La Sandía",
            "El Tambor",
            "El Camarón",
            "Las Jaras",
            "El Músico",
            "La Araña",
            "El Soldado",
            "La Estrella",
            "El Cazo",
            "El Mundo",
            "El Apache",
            "El Nopal",
            "El Alacrán",
            "La Rosa",
            "La Calavera",
            "La Campana",
            "El Cantarito",
            "El Venado",
            "El Sol",
            "La Corona",
            "La Chalupa",
            "El Pino",
            "El Pescado",
            "La Palma",
            "La Maceta",
            "El Arpa",
            "La Rana",
        ]
        names = [c["name"] for c in sorted(self.catalog["cards"], key=lambda c: c["id"])]
        self.assertEqual(names, expected)

    def test_shuffle_no_duplicates_simulation(self) -> None:
        import random

        order = list(range(54))
        random.seed(42)
        for _ in range(20):
            random.shuffle(order)
            self.assertEqual(len(set(order)), 54)
            self.assertEqual(sorted(order), list(range(54)))


class DedicationTests(unittest.TestCase):
    def test_readme_contains_exact_dedication(self) -> None:
        readme = (ROOT / "README.md").read_text(encoding="utf-8")
        self.assertIn("## A special dedication", readme)
        self.assertIn("# For APA", readme)
        self.assertIn(
            "The inspiration behind every line of code, every idea, and every project.",
            readme,
        )
        self.assertIn(
            "Because the best creations are born from what we love most.",
            readme,
        )


if __name__ == "__main__":
    unittest.main()
