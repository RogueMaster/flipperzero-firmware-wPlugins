import unittest
from scripts.stage_desktop_release import validate_inventory


class ReleaseInventoryTests(unittest.TestCase):
    def item(self, version):
        return {"name": "flipper-usb-internet-bridge-host", "version": version}

    def test_editable_install_duplicate_metadata_is_allowed_when_identical(self):
        validate_inventory([self.item("0.5.1"), self.item("0.5.1")], "0.5.1")

    def test_stale_or_conflicting_versions_are_never_allowed(self):
        for inventory in (
            [self.item("0.5.0")],
            [self.item("0.5.1"), self.item("0.5.0")],
            [],
        ):
            with self.subTest(inventory=inventory), self.assertRaises(ValueError):
                validate_inventory(inventory, "0.5.1")
